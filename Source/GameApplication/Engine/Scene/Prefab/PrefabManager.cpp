#include "PrefabManager.h"

#include <fstream>
#include <filesystem>
#include <backends/yaml-cpp/yaml.h>

#include "Resources/Data/prefabData.h"
#include "Resources/resourceService.h"
#include "sceneManager.h"
#include "Registry/componentRegistry.h"
#include "Registry/entityRegistry.h"
#include "Interface/IComponent.h"

bool PrefabManager::SavePrefab(Entity entity, SceneContext* context, const std::string& filePath) {
	if (!context || !context->entity || !context->component) return false;
	if (!context->entity->IsAlive(entity)) return false;

	// 出力ディレクトリを作成
	std::filesystem::path path(filePath);
	std::filesystem::create_directories(path.parent_path());

	YAML::Node root;
	root["Entity"] = static_cast<int>(entity);

	YAML::Node componentsNode = YAML::Node(YAML::NodeType::Sequence);
	for (IComponent* comp : context->component->GetAllComponentsOfEntitySorted(entity)) {
		if (!comp) continue;

		std::type_index ti(typeid(*comp));
		ComponentTypeID compId = context->component->GetComponentIDByTypeIndex(ti);
		const auto& idToName = context->component->GetComponentIDToNameMap();
		auto it = idToName.find(compId);
		if (it == idToName.end()) continue;

		YAML::Node compNode;
		compNode["Component"] = it->second;

		YAML::Node encoded = comp->encode();
		if (encoded && encoded.IsMap()) {
			for (auto kv = encoded.begin(); kv != encoded.end(); ++kv) {
				compNode[kv->first.as<std::string>()] = kv->second;
			}
		}
		componentsNode.push_back(compNode);
	}
	root["Components"] = componentsNode;

	std::ofstream fout(filePath);
	if (!fout.is_open()) return false;
	fout << root;

	// 保存後にキャッシュを無効化する（次回ロード時に最新の内容が使われるよう）
	if (context->manager && context->manager->resource) {
		context->manager->resource->Unload<PrefabData>(filePath);
	}

	return true;
}

Entity PrefabManager::InstantiatePrefab(SceneContext* context, const std::string& filePath) {
	if (!context) return 0;

	// リソースシステム経由でロード（キャッシュが効く）
	if (context->manager && context->manager->resource) {
		auto data = context->manager->resource->Load<PrefabData>(filePath);
		if (!data) return 0;
		return Instantiate(context, data);
	}

	// リソースシステムが使えない場合は直接ロード（フォールバック）
	std::ifstream fin(filePath);
	if (!fin.is_open()) return 0;
	auto data = std::make_shared<PrefabData>();
	data->filePath = filePath;
	data->root = YAML::Load(fin);
	if (!data->root["Components"] || !data->root["Components"].IsSequence()) return 0;
	return Instantiate(context, data);
}

Entity PrefabManager::Instantiate(SceneContext* context, const std::shared_ptr<PrefabData>& data) {
	if (!context || !context->entity || !context->component) return 0;
	if (!data || !data->root["Components"] || !data->root["Components"].IsSequence()) return 0;

	Entity newEntity = context->entity->Create();

	for (const auto& compNode : data->root["Components"]) {
		if (!compNode["Component"]) continue;
		const std::string compType = compNode["Component"].as<std::string>();
		context->component->CreateFromYAML(compType, newEntity, compNode);
	}

	return newEntity;
}
