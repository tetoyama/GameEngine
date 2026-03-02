#pragma once

// =======================================================================
// EntityCommand.h
// エンティティの作成・削除に対応するコマンド
// =======================================================================

#include "ICommand.h"
#include "Entity/Entity.h"
#include "Scene/scene.h"
#include "Scene/Interface/IComponent.h"
#include "Scene/Component/transformComponent.h"
#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>
#include <functional>
#include <typeindex>
#include <algorithm>

// -----------------------------------------------------------------------
// 共通ヘルパー：TransformComponent 経由で親子関係を設定する
// -----------------------------------------------------------------------
namespace EntityCommandHelper {

inline void SetParent(Entity child, Entity newParent, SceneContext* ctx) {
	auto* childT = ctx->component->GetComponent<TransformComponent>(child);
	if (!childT) return;

	// 旧親の children から除去
	if (childT->parent != 0) {
		auto* oldParentT = ctx->component->GetComponent<TransformComponent>(childT->parent);
		if (oldParentT) {
			auto& ch = oldParentT->children;
			ch.erase(std::remove(ch.begin(), ch.end(), child), ch.end());
		}
	}

	childT->parent = newParent;

	// 新親の children に追加
	if (newParent != 0) {
		auto* newParentT = ctx->component->GetComponent<TransformComponent>(newParent);
		if (newParentT) {
			newParentT->children.push_back(child);
		}
	}
}

} // namespace EntityCommandHelper

// -----------------------------------------------------------------------
// EntityCreateCommand
// エンティティ作成コマンド（Undo で削除）
// -----------------------------------------------------------------------
class EntityCreateCommand : public ICommand {
public:
	using PostCreateCallback = std::function<void(Entity, SceneContext*)>;

	EntityCreateCommand(SceneContext* context, Entity parentEntity = 0,
		PostCreateCallback onCreated = nullptr)
		: m_context(context)
		, m_parent(parentEntity)
		, m_created(0)
		, m_onCreated(onCreated)
	{}

	void Execute() override {
		if (!m_context) return;

		// リドゥ時は同じ ID を再利用
		Entity e = (m_created != 0)
			? m_context->entity->CreateID(m_created)
			: m_context->entity->Create();
		m_created = e;

		YAML::Node nameNode;
		nameNode["Name"] = "Entity";
		m_context->component->CreateFromYAML("NameComponent", e, nameNode);
		m_context->component->CreateFromYAML("TransformComponent", e, YAML::Node());

		// 親子関係
		if (m_parent != 0 && m_context->entity->IsAlive(m_parent)) {
			EntityCommandHelper::SetParent(m_created, m_parent, m_context);
		}

		if (m_onCreated) m_onCreated(m_created, m_context);
	}

	void Undo() override {
		if (!m_context || m_created == 0) return;
		if (!m_context->entity->IsAlive(m_created)) return;

		// 親子関係を解除
		EntityCommandHelper::SetParent(m_created, 0, m_context);

		m_context->component->OnEntityDestroyed(m_created);
		m_context->entity->Destroy(m_created);
	}

	Entity GetCreatedEntity() const { return m_created; }

private:
	SceneContext*      m_context;
	Entity             m_parent;
	Entity             m_created;
	PostCreateCallback m_onCreated;
};

// -----------------------------------------------------------------------
// EntityDeleteCommand
// エンティティ削除コマンド（Undo で完全復元）
// 削除前に全コンポーネントを YAML でスナップショット
// -----------------------------------------------------------------------
class EntityDeleteCommand : public ICommand {
public:
	using PostDeleteCallback  = std::function<void()>;
	using PostRestoreCallback = std::function<void(Entity, SceneContext*)>;

	EntityDeleteCommand(SceneContext* context, Entity entity,
		PostDeleteCallback  onDeleted  = nullptr,
		PostRestoreCallback onRestored = nullptr)
		: m_context(context)
		, m_entity(entity)
		, m_onDeleted(onDeleted)
		, m_onRestored(onRestored)
	{
		// 構築時にスナップショット取得
		_SnapshotRecursive(m_entity);
	}

	void Execute() override {
		if (!m_context || !m_context->entity->IsAlive(m_entity)) return;
		_DestroyRecursive(m_entity);
		if (m_onDeleted) m_onDeleted();
	}

	void Undo() override {
		if (!m_context || m_context->entity->IsAlive(m_entity)) return;
		_RestoreAll();
		if (m_onRestored) m_onRestored(m_entity, m_context);
	}

private:
	struct EntitySnapshot {
		Entity entity;
		std::vector<std::pair<std::string, YAML::Node>> components;
	};

	// ---- スナップショット ----
	void _SnapshotRecursive(Entity e) {
		EntitySnapshot snap;
		snap.entity = e;

		const auto& idToName = m_context->component->GetComponentIDToNameMap();
		for (IComponent* comp : m_context->component->GetAllComponentsOfEntitySorted(e)) {
			std::type_index ti(typeid(*comp));
			ComponentTypeID typeID = m_context->component->GetComponentIDByTypeIndex(ti);
			if (typeID == static_cast<ComponentTypeID>(-1)) continue;
			auto nameIt = idToName.find(typeID);
			if (nameIt == idToName.end()) continue;
			snap.components.emplace_back(nameIt->second, comp->encode());
		}
		m_snapshots.push_back(std::move(snap));

		// 子エンティティも再帰スナップショット
		auto* t = m_context->component->GetComponent<TransformComponent>(e);
		if (t) {
			for (Entity child : t->children) {
				_SnapshotRecursive(child);
			}
		}
	}

	// ---- 削除 ----
	void _DestroyRecursive(Entity e) {
		if (!m_context->entity->IsAlive(e)) return;

		auto* t = m_context->component->GetComponent<TransformComponent>(e);
		std::vector<Entity> children = t ? t->children : std::vector<Entity>{};

		for (Entity child : children) {
			_DestroyRecursive(child);
		}

		// 親の children リストから除去
		EntityCommandHelper::SetParent(e, 0, m_context);

		m_context->component->OnEntityDestroyed(e);
		m_context->entity->Destroy(e);
	}

	// ---- 復元 ----
	void _RestoreAll() {
		// まず全エンティティを作成（コンポーネントなし）
		for (auto& snap : m_snapshots) {
			if (!m_context->entity->IsAlive(snap.entity)) {
				m_context->entity->CreateID(snap.entity);
			}
		}
		// 次にコンポーネントを復元（TransformComponent の親子関係も含む）
		for (auto& snap : m_snapshots) {
			for (auto& [name, node] : snap.components) {
				m_context->component->CreateFromYAML(name, snap.entity, node);
			}
		}
		// TransformComponent の children リストを親子関係から再構築
		for (auto& snap : m_snapshots) {
			auto* t = m_context->component->GetComponent<TransformComponent>(snap.entity);
			if (!t || t->parent == 0) continue;
			auto* parentT = m_context->component->GetComponent<TransformComponent>(t->parent);
			if (!parentT) continue;
			if (std::find(parentT->children.begin(), parentT->children.end(), snap.entity)
				== parentT->children.end()) {
				parentT->children.push_back(snap.entity);
			}
		}
	}

	SceneContext*               m_context;
	Entity                      m_entity;
	std::vector<EntitySnapshot> m_snapshots;
	PostDeleteCallback          m_onDeleted;
	PostRestoreCallback         m_onRestored;
};
