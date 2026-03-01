#include "PrefabSystem.h"
#include "scene.h"

#include <fstream>
#include <filesystem>
#include <sstream>
#include <backends/yaml-cpp/yaml.h>

#include "Resources/Data/prefabData.h"
#include "Resources/resourceService.h"
#include "sceneManager.h"
#include "Registry/componentRegistry.h"
#include "Registry/entityRegistry.h"
#include "Interface/IComponent.h"
#include "Component/transformComponent.h"
#include "Component/PrefabComponent.h"

// ルートエンティティから Transform 親子関係をたどり、
// 階層全体を BFS 順（親が子より先）で収集する
static std::vector<Entity> CollectHierarchy(Entity root, SceneContext* context) {
	std::vector<Entity> result;
	std::vector<Entity> queue = { root };
	size_t head = 0;
	while (head < queue.size()) {
		Entity e = queue[head++];
		result.push_back(e);
		auto* tc = context->component->GetComponent<TransformComponent>(e);
		if (tc) {
			for (Entity child : tc->children) {
				queue.push_back(child);
			}
		}
	}
	return result;
}

// TransformComponent の children リストを parent 参照から再構築する
static void RebuildTransformChildren(ComponentRegistry* registry) {
	auto entities = registry->FindEntitiesWithComponent<TransformComponent>();
	for (Entity e : entities) {
		auto* tc = registry->GetComponent<TransformComponent>(e);
		if (tc) tc->children.clear();
	}
	for (Entity e : entities) {
		auto* tc = registry->GetComponent<TransformComponent>(e);
		if (!tc || tc->parent == 0) continue;
		auto* parentTc = registry->GetComponent<TransformComponent>(tc->parent);
		if (parentTc) parentTc->children.push_back(e);
	}
}

bool PrefabSystem::SavePrefab(EntityRef ref, const std::string& filePath) {
	SceneContext* context = ref.GetScene();
	Entity entity = ref.GetEntityID();
	if (!context || !context->entity || !context->component) return false;
	if (!context->entity->IsAlive(entity)) return false;

	// 出力ディレクトリを作成
	std::filesystem::path path(filePath);
	std::filesystem::create_directories(path.parent_path());

	// children リストが古い可能性があるため、収集前に必ず再構築する
	RebuildTransformChildren(context->component);

	// 階層全体（ルート + 全子孫）を収集して旧 Entity → ローカル ID マッピングを構築
	std::vector<Entity> hierarchy = CollectHierarchy(entity, context);
	std::unordered_map<Entity, int> entityToLocal;
	for (int i = 0; i < static_cast<int>(hierarchy.size()); i++) {
		entityToLocal[hierarchy[i]] = i;
	}

	YAML::Node root;
	YAML::Node entitiesSeq = YAML::Node(YAML::NodeType::Sequence);

	for (int i = 0; i < static_cast<int>(hierarchy.size()); i++) {
		Entity e = hierarchy[i];
		YAML::Node entityNode;
		entityNode["LocalID"] = i;

		// 親のローカル ID を記録する。ルート（親なし）の場合はキーを省略する
		auto* tc = context->component->GetComponent<TransformComponent>(e);
		if (tc && tc->parent != 0) {
			auto it = entityToLocal.find(tc->parent);
			if (it != entityToLocal.end()) {
				entityNode["PrefabParent"] = it->second;
			}
		}

		// コンポーネントをエンコード
		YAML::Node componentsSeq = YAML::Node(YAML::NodeType::Sequence);
		for (IComponent* comp : context->component->GetAllComponentsOfEntitySorted(e)) {
			if (!comp) continue;
			// PrefabComponent はランタイム管理用のため、プレファブファイルには書き出さない
			if (dynamic_cast<PrefabComponent*>(comp)) continue;

			std::type_index ti(typeid(*comp));
			ComponentTypeID compId = context->component->GetComponentIDByTypeIndex(ti);
			const auto& idToName = context->component->GetComponentIDToNameMap();
			auto it = idToName.find(compId);
			if (it == idToName.end()) continue;

			YAML::Node compNode;
			compNode["Component"] = it->second;

			YAML::Node encoded = comp->encode();
			// TransformComponent の Parent フィールドは PrefabParent で管理するため
			// エンコード済みノードでは常に 0（親なし）に上書きする
			if (dynamic_cast<TransformComponent*>(comp)) {
				encoded["Parent"] = 0;
			}
			if (encoded && encoded.IsMap()) {
				for (auto kv = encoded.begin(); kv != encoded.end(); ++kv) {
					compNode[kv->first.as<std::string>()] = kv->second;
				}
			}
			componentsSeq.push_back(compNode);
		}
		entityNode["Components"] = componentsSeq;
		entitiesSeq.push_back(entityNode);
	}

	root["Entities"] = entitiesSeq;

	// YAML を一度文字列にシリアライズして、ファイル書き出しと PrefabComponent 両方に使う
	std::ostringstream ss;
	ss << root;
	const std::string savedYaml = ss.str();

	std::ofstream fout(filePath);
	if (!fout.is_open()) return false;
	fout << savedYaml;

	// 保存後にキャッシュを無効化する（次回ロード時に最新の内容が使われるよう）
	if (context->manager && context->manager->resource) {
		context->manager->resource->Unload<PrefabData>(filePath);
	}

	// ルートエンティティに PrefabComponent を付与・更新する
	// すでに付いている場合はパスと YAML スナップショットを最新の保存内容で上書きする
	auto* prefabComp = context->component->GetComponent<PrefabComponent>(entity);
	if (!prefabComp) {
		prefabComp = context->component->AddComponent<PrefabComponent>(entity);
	}
	if (prefabComp) {
		prefabComp->filePath   = filePath;
		prefabComp->sourceYaml = savedYaml;
	}

	return true;
}

EntityRef PrefabSystem::InstantiatePrefab(SceneContext* context, const std::string& filePath) {
	if (!context) return EntityRef{};

	// リソースシステム経由でロード（キャッシュが効く）
	if (context->manager && context->manager->resource) {
		auto data = context->manager->resource->Load<PrefabData>(filePath);
		if (!data) return EntityRef{};
		return Instantiate(context, data);
	}

	// リソースシステムが使えない場合は直接ロード（フォールバック）
	std::ifstream fin(filePath);
	if (!fin.is_open()) return EntityRef{};
	auto data = std::make_shared<PrefabData>();
	data->filePath = filePath;
	data->root = YAML::Load(fin);
	if (!data->root["Entities"] || !data->root["Entities"].IsSequence()) return EntityRef{};
	return Instantiate(context, data);
}

EntityRef PrefabSystem::Instantiate(SceneContext* context, const std::shared_ptr<PrefabData>& data) {
	if (!context || !context->entity || !context->component) return EntityRef{};
	if (!data || !data->root["Entities"] || !data->root["Entities"].IsSequence()) return EntityRef{};

	const YAML::Node& entitiesSeq = data->root["Entities"];
	if (entitiesSeq.size() == 0) return EntityRef{};

	// Pass 1: エンティティを生成し LocalID → 新 Entity マッピングを構築する
	std::vector<Entity> newEntities;
	newEntities.reserve(entitiesSeq.size());
	for (size_t i = 0; i < entitiesSeq.size(); i++) {
		newEntities.push_back(context->entity->Create());
	}

	// Pass 2: 各エンティティにコンポーネントを追加する
	//         TransformComponent::parent は一時的に 0 のまま（Pass 3 で設定する）
	for (size_t i = 0; i < entitiesSeq.size(); i++) {
		const YAML::Node& entityNode = entitiesSeq[i];
		if (!entityNode["Components"] || !entityNode["Components"].IsSequence()) continue;
		for (const auto& compNode : entityNode["Components"]) {
			if (!compNode["Component"]) continue;
			const std::string compType = compNode["Component"].as<std::string>();
			context->component->CreateFromYAML(compType, newEntities[i], compNode);
		}
	}

	// Pass 3: PrefabParent を使って TransformComponent::parent を正しい Entity に設定する
	//         PrefabParent キーが存在しないエントリはルート（parent = 0）のまま
	for (size_t i = 0; i < entitiesSeq.size(); i++) {
		const YAML::Node& entityNode = entitiesSeq[i];
		if (!entityNode["PrefabParent"]) continue;
		int localParent = entityNode["PrefabParent"].as<int>();
		if (localParent < 0 || localParent >= static_cast<int>(newEntities.size())) continue;
		auto* tc = context->component->GetComponent<TransformComponent>(newEntities[i]);
		if (tc) {
			tc->parent = newEntities[localParent];
		}
	}

	// 親子リストを再構築する
	RebuildTransformChildren(context->component);

	// PrefabComponent をルートエンティティに付与する
	// filePath は上書き保存に、sourceYaml はインスタンス化時の状態記録（差分検出の基準値）に使用する
	if (!data->filePath.empty()) {
		auto* prefabComp = context->component->AddComponent<PrefabComponent>(newEntities[0]);
		if (prefabComp) {
			prefabComp->filePath = data->filePath;
			std::ostringstream ss;
			ss << data->root;
			prefabComp->sourceYaml = ss.str();
		}
	}

	return EntityRef(newEntities[0], context);
}

