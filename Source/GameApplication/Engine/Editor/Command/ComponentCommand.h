// =======================================================================
// 
// ComponentCommand.h
// 
// =======================================================================
#pragma once

#include "ICommand.h"
#include "Entity/Entity.h"
#include "Scene/scene.h"
#include "Scene/Interface/IComponent.h"
#include <yaml-cpp/yaml.h>
#include <string>
#include <typeindex>
#include <functional>

// -----------------------------------------------------------------------
// ComponentAddCommand
// コンポーネント追加コマンド（Undo で削除）
// -----------------------------------------------------------------------
class ComponentAddCommand : public ICommand {
public:
	ComponentAddCommand(SceneContext* context, Entity entity, const std::string& componentName,
		std::function<void(Entity)> addFunc)
		: m_context(context)
		, m_entity(entity)
		, m_componentName(componentName)
		, m_addFunc(addFunc)
	{}

	void Execute() override {
		if (!m_context || !m_context->entity->IsAlive(m_entity)) return;

		// リドゥ時はスナップショットから復元し、初回はファクトリ関数を使う
		if (!m_snapshot.IsNull()) {
			m_context->component->CreateFromYAML(m_componentName, m_entity, m_snapshot);
		} else {
			m_addFunc(m_entity);

			// リドゥに備えて追加直後のコンポーネント状態をスナップショット
			auto comps = m_context->component->GetAllComponentsOfEntitySorted(m_entity);
			const auto& idToName = m_context->component->GetComponentIDToNameMap();
			for (IComponent* comp : comps) {
				std::type_index ti(typeid(*comp));
				ComponentTypeID typeID = m_context->component->GetComponentIDByTypeIndex(ti);
				auto nameIt = idToName.find(typeID);
				if (nameIt != idToName.end() && nameIt->second == m_componentName) {
					m_snapshot = comp->encode();
					break;
				}
			}
		}
	}

	void Undo() override {
		if (!m_context || !m_context->entity->IsAlive(m_entity)) return;

		ComponentTypeID id = m_context->component->GetComponentIDByName(m_componentName);
		if (id == static_cast<ComponentTypeID>(-1)) return;

		// 削除前にスナップショット（リドゥ用）
		auto comps = m_context->component->GetAllComponentsOfEntitySorted(m_entity);
		const auto& idToName = m_context->component->GetComponentIDToNameMap();
		for (IComponent* comp : comps) {
			std::type_index ti(typeid(*comp));
			ComponentTypeID typeID = m_context->component->GetComponentIDByTypeIndex(ti);
			auto nameIt = idToName.find(typeID);
			if (nameIt != idToName.end() && nameIt->second == m_componentName) {
				m_snapshot = comp->encode();
				break;
			}
		}

		m_context->component->RemoveComponentByID(m_entity, id);
	}

	// Undo でコンポーネントを削除するため、Redo スタックの生ポインタを事前クリアする
	bool ClearsRedoOnUndo() const override { return true; }

	std::string GetDescription() const override { return "コンポーネントを追加: " + m_componentName; }

private:
	SceneContext*               m_context;
	Entity m_Entity;
	std::string m_ComponentName;
	std::function<void(Entity)> m_addFunc;
	YAML::Node m_Snapshot;
};

// -----------------------------------------------------------------------
// ComponentRemoveCommand
// コンポーネント削除コマンド（Undo で復元）
// -----------------------------------------------------------------------
class ComponentRemoveCommand : public ICommand {
public:
	ComponentRemoveCommand(SceneContext* context, Entity entity, IComponent* component)
		: m_context(context)
		, m_entity(entity)
	{
		// 削除前にスナップショット取得
		std::type_index ti(typeid(*component));
		m_typeID = context->component->GetComponentIDByTypeIndex(ti);
		const auto& idToName = context->component->GetComponentIDToNameMap();
		auto nameIt = idToName.find(m_typeID);
		if (nameIt != idToName.end()) {
			m_componentName = nameIt->second;
		}
		m_snapshot = component->encode();
	}

	void Execute() override {
		if (!m_context || !m_context->entity->IsAlive(m_entity)) return;
		if (m_typeID == static_cast<ComponentTypeID>(-1)) return;
		m_context->component->RemoveComponentByID(m_entity, m_typeID);
	}

	void Undo() override {
		if (!m_context || !m_context->entity->IsAlive(m_entity)) return;
		if (m_componentName.empty()) return;
		m_context->component->CreateFromYAML(m_componentName, m_entity, m_snapshot);
	}

	// コンポーネントを破棄することで、それ以前のコマンドのコンポーネントポインタを無効化する。
	// Undo スタックを事前クリアしてダングリングポインタ使用を防ぐ。
	bool ClearsUndoHistory() const override { return true; }

	std::string GetDescription() const override { return "コンポーネントを削除: " + m_componentName; }

private:
	SceneContext*   m_context;
	Entity m_Entity;
	ComponentTypeID m_TypeId= static_cast<ComponentTypeID>(-1);
	std::string m_ComponentName;
	YAML::Node m_Snapshot;
};
