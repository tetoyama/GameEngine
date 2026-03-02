#pragma once

// =======================================================================
// PrefabCommand.h
// プレファブのインスタンス化を Undo/Redo するコマンド
// =======================================================================

#include "ICommand.h"
#include "EntitySnapshotHelper.h"
#include "Scene/Component/PrefabComponent.h"
#include "Scene/Reference/EntityRef.h"
#include "Scene/Prefab/PrefabSystem.h"
#include <string>
#include <vector>
#include <functional>

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
			EntityCommandHelper::RestoreAll(m_snapshots, m_context);
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
		EntityCommandHelper::SnapshotRecursive(m_spawned, m_context, m_snapshots);

		if (m_onInstantiated) m_onInstantiated(EntityRef(m_spawned, m_context));
	}

	void Undo() override {
		if (!m_context || m_spawned == 0) return;
		if (!m_context->entity->IsAlive(m_spawned)) return;
		EntityCommandHelper::DestroyRecursive(m_spawned, m_context);
	}

	std::string GetDescription() const override { return "Prefabを配置"; }

private:
	SceneContext*                                    m_context;
	std::string                                      m_filePath;
	bool                                             m_removePrefabComp;
	Entity                                           m_spawned;
	std::vector<EntityCommandHelper::EntitySnapshot> m_snapshots;
	PostInstantiateCallback                          m_onInstantiated;
};
