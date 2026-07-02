// =======================================================================
//
// ComponentCommand.h
//
// =======================================================================
#pragma once

#include "ICommand.h"
#include "Entity/Entity.h"
#include "Scene/scene.h"

#include <yaml-cpp/yaml.h>

#include <functional>
#include <string>
#include <utility>

class ComponentAddCommand: public ICommand {
public:
	ComponentAddCommand(
		SceneContext* context,
		Entity entity,
		std::string componentName,
		std::function<void(Entity)> addFunction
	)
		: m_context(context),
		  m_entity(entity),
		  m_componentName(std::move(componentName)),
		  m_addFunction(std::move(addFunction)){}

	void Execute() override {
		if(!IsEntityValid()) return;

		if(!m_snapshot.IsNull()){
			m_context->component->CreateFromYAML(
				m_componentName,
				m_entity,
				m_snapshot
			);
			return;
		}

		m_addFunction(m_entity);
		CaptureSnapshot();
	}

	void Undo() override {
		if(!IsEntityValid()) return;

		const ComponentTypeID typeID =
			m_context->component->GetComponentIDByName(m_componentName);
		if(typeID == INVALID_COMPONENT_TYPE_ID) return;

		CaptureSnapshot();
		m_context->component->RemoveComponentByID(m_entity, typeID);
	}

	bool ClearsRedoOnUndo() const override { return true; }

	std::string GetDescription() const override {
		return "コンポーネントを追加: " + m_componentName;
	}

private:
	bool IsEntityValid() const {
		return m_context && m_context->entity &&
			m_context->entity->IsAlive(m_entity);
	}

	void CaptureSnapshot(){
		const ComponentTypeID typeID =
			m_context->component->GetComponentIDByName(m_componentName);
		if(typeID == INVALID_COMPONENT_TYPE_ID) return;

		const ComponentView component =
			m_context->component->GetComponentByID(m_entity, typeID);
		if(component){
			m_snapshot = m_context->component->EncodeComponent(component);
		}
	}

	SceneContext* m_context = nullptr;
	Entity m_entity{};
	std::string m_componentName;
	std::function<void(Entity)> m_addFunction;
	YAML::Node m_snapshot;
};

class ComponentRemoveCommand: public ICommand {
public:
	ComponentRemoveCommand(
		SceneContext* context,
		Entity entity,
		ComponentView component
	)
		: m_context(context),
		  m_entity(entity),
		  m_typeID(component.typeID)
	{
		if(!m_context || !component) return;
		m_componentName =
			m_context->component->GetComponentName(component.typeID);
		m_snapshot = m_context->component->EncodeComponent(component);
	}

	void Execute() override {
		if(!IsEntityValid() || m_typeID == INVALID_COMPONENT_TYPE_ID) return;
		m_context->component->RemoveComponentByID(m_entity, m_typeID);
	}

	void Undo() override {
		if(!IsEntityValid() || m_componentName.empty()) return;
		m_context->component->CreateFromYAML(
			m_componentName,
			m_entity,
			m_snapshot
		);
	}

	bool ClearsUndoHistory() const override { return true; }

	std::string GetDescription() const override {
		return "コンポーネントを削除: " + m_componentName;
	}

private:
	bool IsEntityValid() const {
		return m_context && m_context->entity &&
			m_context->entity->IsAlive(m_entity);
	}

	SceneContext* m_context = nullptr;
	Entity m_entity{};
	ComponentTypeID m_typeID = INVALID_COMPONENT_TYPE_ID;
	std::string m_componentName;
	YAML::Node m_snapshot;
};
