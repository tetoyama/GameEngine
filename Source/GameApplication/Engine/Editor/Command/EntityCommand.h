// =======================================================================
// 
// EntityCommand.h
// 
// =======================================================================
#pragma once

#include "ICommand.h"
#include "EntitySnapshotHelper.h"
#include "Scene/Component/entityNameComponent.h"
#include <string>
#include <vector>
#include <functional>

// -----------------------------------------------------------------------
// SetParentCommand
// D&D などで親子関係を変更するコマンド（Undo で旧親に戻す）
// -----------------------------------------------------------------------
class SetParentCommand : public ICommand {
public:
	SetParentCommand(SceneContext* context, Entity child, Entity oldParent, Entity newParent)
		: m_pContext(context)
		, m_child(child)
		, m_oldParent(oldParent)
		, m_newParent(newParent)
	{}

	void Execute() override {
		if (m_pContext) EntityCommandHelper::SetParent(m_child, m_newParent, m_pContext);
	}

	void Undo() override {
		if (m_pContext) EntityCommandHelper::SetParent(m_child, m_oldParent, m_pContext);
	}

	std::string GetDescription() const override { return "親子関係を変更"; }

private:
	SceneContext* m_pContext;
	Entity m_Child;
	Entity m_OldParent;
	Entity m_NewParent;
};

// -----------------------------------------------------------------------
// EntityCreateCommand
// エンティティ作成コマンド（Undo で削除）
// -----------------------------------------------------------------------
class EntityCreateCommand : public ICommand {
public:
	using PostCreateCallback = std::function<void(Entity, SceneContext*)>;
	using PostUndoCallback   = std::function<void()>;

	EntityCreateCommand(SceneContext* context, Entity parentEntity = 0,
		PostCreateCallback onCreated = nullptr,
		PostUndoCallback   onUndone  = nullptr)
		: m_pContext(context)
		, m_parent(parentEntity)
		, m_created(0)
		, m_onCreated(onCreated)
		, m_onUndone(onUndone)
	{}

	void Execute() override {
		if (!m_pContext) return;

		// リドゥ時は同じ ID を再利用
		Entity e = (m_created != 0)
			? m_pContext->entity->CreateID(m_created)
			: m_pContext->entity->Create();
		m_created = e;

		YAML::Node nameNode;
		nameNode["Name"] = "Entity";
		m_pContext->component->CreateFromYAML("NameComponent", e, nameNode);
		m_pContext->component->CreateFromYAML("TransformComponent", e, YAML::Node());

		// 親子関係
		if (m_parent != 0 && m_pContext->entity->IsAlive(m_parent)) {
			EntityCommandHelper::SetParent(m_created, m_parent, m_pContext);
		}

		if (m_onCreated) m_onCreated(m_created, m_pContext);
	}

	void Undo() override {
		if (!m_pContext || m_created == 0) return;
		if (!m_pContext->entity->IsAlive(m_created)) return;

		// 親子関係を解除
		EntityCommandHelper::SetParent(m_created, 0, m_pContext);

		m_pContext->component->OnEntityDestroyed(m_created);
		m_pContext->entity->Destroy(m_created);
		if (m_onUndone) m_onUndone();
	}

	// Undo でエンティティを破棄するため、Redo スタックの生ポインタを事前クリアする
	bool ClearsRedoOnUndo() const override { return true; }

	Entity GetCreatedEntity() const { return m_created; }

	std::string GetDescription() const override { return "エンティティを作成"; }

private:
	SceneContext*      m_pContext;
	Entity m_Parent;
	Entity m_Created;
	PostCreateCallback m_OnCreated;
	PostUndoCallback m_OnUndone;
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
		: m_pContext(context)
		, m_entity(entity)
		, m_onDeleted(onDeleted)
		, m_onRestored(onRestored)
	{
		// 削除前にエンティティ名を記録（UI ヒント用）
		auto* nameComp = context->component->GetComponent<NameComponent>(entity);
		m_entityName = nameComp ? nameComp->name : std::to_string(static_cast<uint32_t>(entity));
		// 構築時にスナップショット取得
		EntityCommandHelper::SnapshotRecursive(m_entity, m_pContext, m_snapshots);
	}

	void Execute() override {
		if (!m_pContext || !m_pContext->entity->IsAlive(m_entity)) return;
		EntityCommandHelper::DestroyRecursive(m_entity, m_pContext);
		if (m_onDeleted) m_onDeleted();
	}

	void Undo() override {
		if (!m_pContext || m_pContext->entity->IsAlive(m_entity)) return;
		EntityCommandHelper::RestoreAll(m_snapshots, m_pContext);
		if (m_onRestored) m_onRestored(m_entity, m_pContext);
	}

	// エンティティを破棄することで、それ以前のコマンドのコンポーネントポインタを無効化する。
	// Undo スタックを事前クリアしてダングリングポインタ使用を防ぐ。
	bool ClearsUndoHistory() const override { return true; }

	std::string GetDescription() const override { return "エンティティを削除: " + m_entityName; }

private:
	SceneContext*                                    m_pContext;
	Entity m_Entity;
	std::string m_EntityName;
	std::vector<EntityCommandHelper::EntitySnapshot> m_Snapshots;
	PostDeleteCallback m_OnDeleted;
	PostRestoreCallback m_OnRestored;
};

// -----------------------------------------------------------------------
// RenameCommand
// エンティティ名変更コマンド（Undo で元の名前に戻す）
// -----------------------------------------------------------------------
class RenameCommand : public ICommand {
public:
	RenameCommand(SceneContext* context, Entity entity, std::string oldName, std::string newName)
		: m_pContext(context)
		, m_entity(entity)
		, m_oldName(std::move(oldName))
		, m_newName(std::move(newName))
	{}

	void Execute() override {
		auto* name = _GetName();
		if (name) name->name = m_newName;
	}

	void Undo() override {
		auto* name = _GetName();
		if (name) name->name = m_oldName;
	}

	std::string GetDescription() const override { return "名前変更: " + m_oldName + " → " + m_newName; }

private:
	NameComponent* _GetName() {
		if (!m_pContext || !m_pContext->entity->IsAlive(m_entity)) return nullptr;
		return m_pContext->component->GetComponent<NameComponent>(m_entity);
	}

	SceneContext* m_pContext;
	Entity m_Entity;
	std::string m_OldName;
	std::string m_NewName;
};

// -----------------------------------------------------------------------
// EntityDuplicateCommand
// エンティティ複製コマンド（Undo で複製エンティティを削除）
// -----------------------------------------------------------------------
class EntityDuplicateCommand : public ICommand {
public:
	using PostCallback     = std::function<void(Entity, SceneContext*)>;
	using PostUndoCallback = std::function<void()>;

	EntityDuplicateCommand(SceneContext* ctx, Entity src,
		PostCallback     onDuplicated = nullptr,
		PostUndoCallback onUndone     = nullptr)
		: m_pContext(ctx)
		, m_src(src)
		, m_duplicated(0)
		, m_onDuplicated(onDuplicated)
		, m_onUndone(onUndone)
	{}

	void Execute() override {
		if (!m_pContext) return;

		if (m_duplicated != 0) {
			// Redo: スナップショットから復元
			EntityCommandHelper::RestoreAll(m_snapshots, m_pContext);
			if (m_onDuplicated) m_onDuplicated(m_duplicated, m_pContext);
			return;
		}

		// 初回: 複製
		auto* t = m_pContext->component->GetComponent<TransformComponent>(m_src);
		Entity newParent = t ? t->parent : 0;
		m_duplicated = _DuplicateRecursive(m_src, newParent);

		// Undo/Redo 用スナップショット
		m_snapshots.clear();
		EntityCommandHelper::SnapshotRecursive(m_duplicated, m_pContext, m_snapshots);

		if (m_onDuplicated) m_onDuplicated(m_duplicated, m_pContext);
	}

	void Undo() override {
		if (!m_pContext || m_duplicated == 0) return;
		if (!m_pContext->entity->IsAlive(m_duplicated)) return;
		EntityCommandHelper::DestroyRecursive(m_duplicated, m_pContext);
		if (m_onUndone) m_onUndone();
	}

	// Undo でエンティティを破棄するため、Redo スタックの生ポインタを事前クリアする
	bool ClearsRedoOnUndo() const override { return true; }

	std::string GetDescription() const override { return "エンティティを複製"; }

private:
	Entity _DuplicateRecursive(Entity src, Entity newParent) {
		Entity m_NewEntity= m_pContext->entity->Create();

		const auto& idToName = m_pContext->component->GetComponentIDToNameMap();
		auto m_Comps= m_pContext->component->GetAllComponentsOfEntitySorted(src);
		for (IComponent* comp : comps) {
			ComponentTypeID m_Tid= m_pContext->component->GetComponentIDByTypeIndex(std::type_index(typeid(*comp)));
			if (tid == static_cast<ComponentTypeID>(-1)) continue;
			auto m_NameIt= idToName.find(tid);
			if (nameIt == idToName.end()) continue;
			m_pContext->component->CreateFromYAML(nameIt->second, newEntity, comp->encode());
		}

		auto* newT = m_pContext->component->GetComponent<TransformComponent>(newEntity);
		if (newT) {
			newT->children.clear();
			newT->parent = newParent;
			if (newParent != 0) {
				auto* parentT = m_pContext->component->GetComponent<TransformComponent>(newParent);
				if (parentT) parentT->children.push_back(newEntity);
			}
		}

		auto* srcT = m_pContext->component->GetComponent<TransformComponent>(src);
		if (srcT) {
			for (Entity srcChild : srcT->children)
				_DuplicateRecursive(srcChild, newEntity);
		}

		return m_NewEntity;
	}

	SceneContext*                                    m_pContext;
	Entity m_Src;
	Entity m_Duplicated;
	std::vector<EntityCommandHelper::EntitySnapshot> m_Snapshots;
	PostCallback m_OnDuplicated;
	PostUndoCallback m_OnUndone;
};

// -----------------------------------------------------------------------
// EmptyParentCommand
// 選択エンティティの上に空の親エンティティを挿入するコマンド
// -----------------------------------------------------------------------
class EmptyParentCommand : public ICommand {
public:
	using PostCallback     = std::function<void(Entity, SceneContext*)>;
	using PostUndoCallback = std::function<void()>;

	EmptyParentCommand(SceneContext* ctx, Entity entity,
		PostCallback     onCreated = nullptr,
		PostUndoCallback onUndone  = nullptr)
		: m_pContext(ctx)
		, m_entity(entity)
		, m_entityOldParent(0)
		, m_newParent(0)
		, m_onCreated(onCreated)
		, m_onUndone(onUndone)
	{
		auto* t = ctx->component->GetComponent<TransformComponent>(entity);
		m_entityOldParent = t ? t->parent : 0;
	}

	void Execute() override {
		if (!m_pContext) return;

		if (m_newParent != 0) {
			// Redo: スナップショットから新親を復元
			if (!m_pContext->entity->IsAlive(m_newParent))
				m_pContext->entity->CreateID(m_newParent);
			for (auto& [compName, node] : m_snapshot)
				m_pContext->component->CreateFromYAML(compName, m_newParent, node);
			EntityCommandHelper::SetParent(m_entity, m_newParent, m_pContext);
			if (m_onCreated) m_onCreated(m_newParent, m_pContext);
			return;
		}

		// 初回: 新しい空エンティティを作成
		m_newParent = m_pContext->entity->Create();
		YAML::Node nameNode; nameNode["Name"] = "Entity";
		m_pContext->component->CreateFromYAML("NameComponent", m_newParent, nameNode);
		m_pContext->component->CreateFromYAML("TransformComponent", m_newParent, YAML::Node());

		// 元エンティティを新親の子にする
		EntityCommandHelper::SetParent(m_entity, m_newParent, m_pContext);

		// Redo 用スナップショット
		const auto& idToName = m_pContext->component->GetComponentIDToNameMap();
		for (IComponent* comp : m_pContext->component->GetAllComponentsOfEntitySorted(m_newParent)) {
			std::type_index ti(typeid(*comp));
			ComponentTypeID tid = m_pContext->component->GetComponentIDByTypeIndex(ti);
			auto nameIt = idToName.find(tid);
			if (nameIt != idToName.end())
				m_snapshot.emplace_back(nameIt->second, comp->encode());
		}

		if (m_onCreated) m_onCreated(m_newParent, m_pContext);
	}

	void Undo() override {
		if (!m_pContext || m_newParent == 0) return;
		if (!m_pContext->entity->IsAlive(m_newParent)) return;

		// 元エンティティの親を元に戻す
		EntityCommandHelper::SetParent(m_entity, m_entityOldParent, m_pContext);

		// 新親エンティティを削除
		m_pContext->component->OnEntityDestroyed(m_newParent);
		m_pContext->entity->Destroy(m_newParent);
		if (m_onUndone) m_onUndone();
	}

	// Undo でエンティティを破棄するため、Redo スタックの生ポインタを事前クリアする
	bool ClearsRedoOnUndo() const override { return true; }

	std::string GetDescription() const override { return "空の親エンティティを作成"; }

private:
	SceneContext* m_pContext;
	Entity m_Entity;
	Entity m_EntityOldParent;
	Entity m_NewParent;
	std::vector<std::pair<std::string, YAML::Node>> m_Snapshot;
	PostCallback m_OnCreated;
	PostUndoCallback m_OnUndone;
};
