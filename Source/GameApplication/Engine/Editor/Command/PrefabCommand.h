#pragma once

// =======================================================================
// PrefabCommand.h
// プレファブのインスタンス化を Undo/Redo するコマンド
// =======================================================================

#include "ICommand.h"
#include "Entity/Entity.h"
#include "Scene/scene.h"
#include "Scene/Interface/IComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Component/PrefabComponent.h"
#include "Scene/Reference/EntityRef.h"
#include "Scene/Prefab/PrefabSystem.h"
#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>
#include <functional>
#include <typeindex>
#include <algorithm>

// -----------------------------------------------------------------------
// PrefabInstantiateCommand
// プレファブインスタンス化コマンド（Undo で生成エンティティをすべて削除）
// -----------------------------------------------------------------------
class PrefabInstantiateCommand : public ICommand {
public:
	using PostInstantiateCallback = std::function<void(EntityRef)>;

	PrefabInstantiateCommand(SceneContext* context, const std::string& filePath,
		bool removePrefabComp = false,
		PostInstantiateCallback onInstantiated = nullptr)
		: m_context(context)
		, m_filePath(filePath)
		, m_removePrefabComp(removePrefabComp)
		, m_spawned(0)
		, m_onInstantiated(onInstantiated)
	{}

	void Execute() override {
		if (!m_context || !m_context->prefab) return;

		if (m_spawned != 0) {
			// Redo: スナップショットから復元
			_RestoreAll();
			if (m_onInstantiated) m_onInstantiated(EntityRef(m_spawned, m_context));
			return;
		}

		// 初回: プレファブをインスタンス化
		EntityRef ref = m_context->prefab->InstantiatePrefab(m_context, m_filePath);
		if (!ref) return;
		m_spawned = ref.GetEntityID();

		// テンプレート系では PrefabComponent を取り除く
		if (m_removePrefabComp) {
			m_context->component->RemoveComponent<PrefabComponent>(m_spawned);
		}

		// Undo に備えてスナップショット取得
		m_snapshots.clear();
		_SnapshotRecursive(m_spawned);

		if (m_onInstantiated) m_onInstantiated(EntityRef(m_spawned, m_context));
	}

	void Undo() override {
		if (!m_context || m_spawned == 0) return;
		if (!m_context->entity->IsAlive(m_spawned)) return;
		_DestroyRecursive(m_spawned);
	}

	std::string GetDescription() const override { return "Prefabを配置"; }

private:
	struct EntitySnapshot {
		Entity entity;
		std::vector<std::pair<std::string, YAML::Node>> components;
	};

	// ---- スナップショット ----
	void _SnapshotRecursive(Entity e) {
		EntitySnapshot snap;
		snap.entity = e;

		static constexpr ComponentTypeID kInvalidTypeID = static_cast<ComponentTypeID>(-1);

		const auto& idToName = m_context->component->GetComponentIDToNameMap();
		for (IComponent* comp : m_context->component->GetAllComponentsOfEntitySorted(e)) {
			std::type_index ti(typeid(*comp));
			ComponentTypeID typeID = m_context->component->GetComponentIDByTypeIndex(ti);
			if (typeID == kInvalidTypeID) continue;
			auto nameIt = idToName.find(typeID);
			if (nameIt == idToName.end()) continue;
			snap.components.emplace_back(nameIt->second, comp->encode());
		}
		m_snapshots.push_back(std::move(snap));

		auto* t = m_context->component->GetComponent<TransformComponent>(e);
		if (t) {
			for (Entity child : t->children) {
				_SnapshotRecursive(child);
			}
		}
	}

	// ---- 削除（子から再帰的に） ----
	void _DestroyRecursive(Entity e) {
		if (!m_context->entity->IsAlive(e)) return;

		auto* t = m_context->component->GetComponent<TransformComponent>(e);
		std::vector<Entity> children = t ? t->children : std::vector<Entity>{};

		for (Entity child : children) {
			_DestroyRecursive(child);
		}

		// 親の children リストから除去
		if (t && t->parent != 0) {
			auto* parentT = m_context->component->GetComponent<TransformComponent>(t->parent);
			if (parentT) {
				auto& ch = parentT->children;
				ch.erase(std::remove(ch.begin(), ch.end(), e), ch.end());
			}
			t->parent = 0;
		}

		m_context->component->OnEntityDestroyed(e);
		m_context->entity->Destroy(e);
	}

	// ---- 復元（スナップショットから） ----
	void _RestoreAll() {
		// Pass 1: スナップショットは _SnapshotRecursive で親→子順に積まれているため
		//         そのままの順序で CreateID を呼べば親が先に生成される
		for (auto& snap : m_snapshots) {
			if (!m_context->entity->IsAlive(snap.entity)) {
				m_context->entity->CreateID(snap.entity);
			}
		}
		// Pass 2: コンポーネントを復元（TransformComponent の parent フィールドも含む）
		for (auto& snap : m_snapshots) {
			for (auto& [name, node] : snap.components) {
				m_context->component->CreateFromYAML(name, snap.entity, node);
			}
		}
		// Pass 3: TransformComponent の children リストを親子関係から再構築
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
	std::string                 m_filePath;
	bool                        m_removePrefabComp;
	Entity                      m_spawned;
	std::vector<EntitySnapshot> m_snapshots;
	PostInstantiateCallback     m_onInstantiated;
};
