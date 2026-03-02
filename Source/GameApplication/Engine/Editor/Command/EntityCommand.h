#pragma once

// =======================================================================
// EntityCommand.h
// エンティティの作成・削除に対応するコマンド
// =======================================================================

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
		: m_context(context)
		, m_child(child)
		, m_oldParent(oldParent)
		, m_newParent(newParent)
	{}

	void Execute() override {
		if (m_context) EntityCommandHelper::SetParent(m_child, m_newParent, m_context);
	}

	void Undo() override {
		if (m_context) EntityCommandHelper::SetParent(m_child, m_oldParent, m_context);
	}

	std::string GetDescription() const override { return "親子関係を変更"; }

private:
	SceneContext* m_context;
	Entity        m_child;
	Entity        m_oldParent;
	Entity        m_newParent;
};

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

	std::string GetDescription() const override { return "エンティティを作成"; }

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
		// 削除前にエンティティ名を記録（UI ヒント用）
		auto* nameComp = context->component->GetComponent<NameComponent>(entity);
		m_entityName = nameComp ? nameComp->name : std::to_string(static_cast<uint32_t>(entity));
		// 構築時にスナップショット取得
		EntityCommandHelper::SnapshotRecursive(m_entity, m_context, m_snapshots);
	}

	void Execute() override {
		if (!m_context || !m_context->entity->IsAlive(m_entity)) return;
		EntityCommandHelper::DestroyRecursive(m_entity, m_context);
		if (m_onDeleted) m_onDeleted();
	}

	void Undo() override {
		if (!m_context || m_context->entity->IsAlive(m_entity)) return;
		EntityCommandHelper::RestoreAll(m_snapshots, m_context);
		if (m_onRestored) m_onRestored(m_entity, m_context);
	}

	std::string GetDescription() const override { return "エンティティを削除: " + m_entityName; }

private:
	SceneContext*                                    m_context;
	Entity                                           m_entity;
	std::string                                      m_entityName;
	std::vector<EntityCommandHelper::EntitySnapshot> m_snapshots;
	PostDeleteCallback                               m_onDeleted;
	PostRestoreCallback                              m_onRestored;
};

// -----------------------------------------------------------------------
// RenameCommand
// エンティティ名変更コマンド（Undo で元の名前に戻す）
// -----------------------------------------------------------------------
class RenameCommand : public ICommand {
public:
	RenameCommand(SceneContext* context, Entity entity, std::string oldName, std::string newName)
		: m_context(context)
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
		if (!m_context || !m_context->entity->IsAlive(m_entity)) return nullptr;
		return m_context->component->GetComponent<NameComponent>(m_entity);
	}

	SceneContext* m_context;
	Entity        m_entity;
	std::string   m_oldName;
	std::string   m_newName;
};

// -----------------------------------------------------------------------
// EntityDuplicateCommand
// エンティティ複製コマンド（Undo で複製エンティティを削除）
// -----------------------------------------------------------------------
class EntityDuplicateCommand : public ICommand {
public:
	using PostCallback = std::function<void(Entity, SceneContext*)>;

	EntityDuplicateCommand(SceneContext* ctx, Entity src, PostCallback onDuplicated = nullptr)
		: m_context(ctx)
		, m_src(src)
		, m_duplicated(0)
		, m_onDuplicated(onDuplicated)
	{}

	void Execute() override {
		if (!m_context) return;

		if (m_duplicated != 0) {
			// Redo: スナップショットから復元
			EntityCommandHelper::RestoreAll(m_snapshots, m_context);
			if (m_onDuplicated) m_onDuplicated(m_duplicated, m_context);
			return;
		}

		// 初回: 複製
		auto* t = m_context->component->GetComponent<TransformComponent>(m_src);
		Entity newParent = t ? t->parent : 0;
		m_duplicated = _DuplicateRecursive(m_src, newParent);

		// Undo/Redo 用スナップショット
		m_snapshots.clear();
		EntityCommandHelper::SnapshotRecursive(m_duplicated, m_context, m_snapshots);

		if (m_onDuplicated) m_onDuplicated(m_duplicated, m_context);
	}

	void Undo() override {
		if (!m_context || m_duplicated == 0) return;
		if (!m_context->entity->IsAlive(m_duplicated)) return;
		EntityCommandHelper::DestroyRecursive(m_duplicated, m_context);
	}

	std::string GetDescription() const override { return "エンティティを複製"; }

private:
	Entity _DuplicateRecursive(Entity src, Entity newParent) {
		Entity newEntity = m_context->entity->Create();

		const auto& idToName = m_context->component->GetComponentIDToNameMap();
		auto comps = m_context->component->GetAllComponentsOfEntitySorted(src);
		for (IComponent* comp : comps) {
			ComponentTypeID tid = m_context->component->GetComponentIDByTypeIndex(std::type_index(typeid(*comp)));
			if (tid == static_cast<ComponentTypeID>(-1)) continue;
			auto nameIt = idToName.find(tid);
			if (nameIt == idToName.end()) continue;
			m_context->component->CreateFromYAML(nameIt->second, newEntity, comp->encode());
		}

		auto* newT = m_context->component->GetComponent<TransformComponent>(newEntity);
		if (newT) {
			newT->children.clear();
			newT->parent = newParent;
			if (newParent != 0) {
				auto* parentT = m_context->component->GetComponent<TransformComponent>(newParent);
				if (parentT) parentT->children.push_back(newEntity);
			}
		}

		auto* srcT = m_context->component->GetComponent<TransformComponent>(src);
		if (srcT) {
			for (Entity srcChild : srcT->children)
				_DuplicateRecursive(srcChild, newEntity);
		}

		return newEntity;
	}

	SceneContext*                                    m_context;
	Entity                                           m_src;
	Entity                                           m_duplicated;
	std::vector<EntityCommandHelper::EntitySnapshot> m_snapshots;
	PostCallback                                     m_onDuplicated;
};

// -----------------------------------------------------------------------
// EmptyParentCommand
// 選択エンティティの上に空の親エンティティを挿入するコマンド
// -----------------------------------------------------------------------
class EmptyParentCommand : public ICommand {
public:
	using PostCallback = std::function<void(Entity, SceneContext*)>;

	EmptyParentCommand(SceneContext* ctx, Entity entity, PostCallback onCreated = nullptr)
		: m_context(ctx)
		, m_entity(entity)
		, m_entityOldParent(0)
		, m_newParent(0)
		, m_onCreated(onCreated)
	{
		auto* t = ctx->component->GetComponent<TransformComponent>(entity);
		m_entityOldParent = t ? t->parent : 0;
	}

	void Execute() override {
		if (!m_context) return;

		if (m_newParent != 0) {
			// Redo: スナップショットから新親を復元
			if (!m_context->entity->IsAlive(m_newParent))
				m_context->entity->CreateID(m_newParent);
			for (auto& [compName, node] : m_snapshot)
				m_context->component->CreateFromYAML(compName, m_newParent, node);
			EntityCommandHelper::SetParent(m_entity, m_newParent, m_context);
			if (m_onCreated) m_onCreated(m_newParent, m_context);
			return;
		}

		// 初回: 新しい空エンティティを作成
		m_newParent = m_context->entity->Create();
		YAML::Node nameNode; nameNode["Name"] = "Entity";
		m_context->component->CreateFromYAML("NameComponent", m_newParent, nameNode);
		m_context->component->CreateFromYAML("TransformComponent", m_newParent, YAML::Node());

		// 元エンティティを新親の子にする
		EntityCommandHelper::SetParent(m_entity, m_newParent, m_context);

		// Redo 用スナップショット
		const auto& idToName = m_context->component->GetComponentIDToNameMap();
		for (IComponent* comp : m_context->component->GetAllComponentsOfEntitySorted(m_newParent)) {
			std::type_index ti(typeid(*comp));
			ComponentTypeID tid = m_context->component->GetComponentIDByTypeIndex(ti);
			auto nameIt = idToName.find(tid);
			if (nameIt != idToName.end())
				m_snapshot.emplace_back(nameIt->second, comp->encode());
		}

		if (m_onCreated) m_onCreated(m_newParent, m_context);
	}

	void Undo() override {
		if (!m_context || m_newParent == 0) return;
		if (!m_context->entity->IsAlive(m_newParent)) return;

		// 元エンティティの親を元に戻す
		EntityCommandHelper::SetParent(m_entity, m_entityOldParent, m_context);

		// 新親エンティティを削除
		m_context->component->OnEntityDestroyed(m_newParent);
		m_context->entity->Destroy(m_newParent);
	}

	std::string GetDescription() const override { return "空の親エンティティを作成"; }

private:
	SceneContext* m_context;
	Entity        m_entity;
	Entity        m_entityOldParent;
	Entity        m_newParent;
	std::vector<std::pair<std::string, YAML::Node>> m_snapshot;
	PostCallback  m_onCreated;
};
