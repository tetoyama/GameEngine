// =======================================================================
// 
// PrefabCommand.h
// 
// =======================================================================
#pragma once

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
	using PostUndoCallback        = std::function<void()>;

	PrefabInstantiateCommand(SceneContext* context, const std::string& filePath,
		bool removePrefabComp = false,
		PostInstantiateCallback onInstantiated = nullptr,
		PostUndoCallback        onUndone       = nullptr)
		: m_pContext(context)
		, m_filePath(filePath)
		, m_removePrefabComp(removePrefabComp)
		, m_spawned(0)
		, m_onInstantiated(onInstantiated)
		, m_onUndone(onUndone)
	{}

	void Execute() override {
		if (!m_pContext || !m_pContext->prefab) return;

		if (m_spawned != 0) {
			// Redo: スナップショットから復元
			EntityCommandHelper::RestoreAll(m_snapshots, m_pContext);
			if (m_onInstantiated) m_onInstantiated(EntityRef(m_spawned, m_pContext));
			return;
		}

		// 初回: プレファブをインスタンス化
		EntityRef ref = m_pContext->prefab->InstantiatePrefab(m_pContext, m_filePath);
		if (!ref) return;
		m_spawned = ref.GetEntityID();

		// テンプレート系では PrefabComponent を取り除く
		if (m_removePrefabComp) {
			m_pContext->component->RemoveComponent<PrefabComponent>(m_spawned);
		}

		// Undo に備えてスナップショット取得
		m_snapshots.clear();
		EntityCommandHelper::SnapshotRecursive(m_spawned, m_pContext, m_snapshots);

		if (m_onInstantiated) m_onInstantiated(EntityRef(m_spawned, m_pContext));
	}

	void Undo() override {
		if (!m_pContext || m_spawned == 0) return;
		if (!m_pContext->entity->IsAlive(m_spawned)) return;
		EntityCommandHelper::DestroyRecursive(m_spawned, m_pContext);
		if (m_onUndone) m_onUndone();
	}

	// Undo でエンティティを破棄するため、Redo スタックの生ポインタを事前クリアする
	bool ClearsRedoOnUndo() const override { return true; }

	std::string GetDescription() const override { return "Prefabを配置"; }

private:
	SceneContext*                                    m_pContext;
	std::string m_FilePath;
	bool m_RemovePrefabComp;
	Entity m_Spawned;
	std::vector<EntityCommandHelper::EntitySnapshot> m_Snapshots;
	PostInstantiateCallback m_OnInstantiated;
	PostUndoCallback m_OnUndone;
};
