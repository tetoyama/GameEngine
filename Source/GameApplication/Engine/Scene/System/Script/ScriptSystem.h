// =======================================================================
//
// ScriptSystem.h
//
// =======================================================================
#pragma once

#include <unordered_map>
#include <functional>
#include <memory>
#include <typeindex>

#include "Scene/Interface/ISystem.h"
#include "Scene/sceneManager.h"
#include "Interface/IScriptComponent.h"
#include "Scene/Component/scriptComponent.h"
#include "Scene/Registry/componentRegistry.h"
#include "Scene/System/Command/EntityCommandCommitSystem.h"

using SetImGuiContextFunc = void(*)(void*);

class ScriptSystem: public ISystem {
public:
	const char* GetSystemName() const override{
		return "ScriptSystem";
	}

	explicit ScriptSystem(SceneManagerContext* context)
		: m_context(context)
		, m_commandCommitSystem(context) {
	}

	void Initialize() override;
	void Finalize() override;
	void Start() override;
	void Stop() override;
	void Update(float deltaTime) override;
	void FixedUpdate(float fixedDeltaTime) override;
	void EditorUpdate(float deltaTime) override;
	void Draw() override;

	void RegisterTasks(SystemScheduleBuilder& builder) override {
		m_commandCommitSystem.RegisterTasks(builder);
		ISystem::RegisterTasks(builder);
	}

	template<typename T>
	void RegisterEngineComponent(const char* name){
		static_assert(std::is_base_of_v<IComponent, T>,
			"T must derive from IComponent");
		m_nameToEngineTypeID[name] = ComponentType::Get<T>();
	}

	IScriptComponent* Create(const char* scriptName){
		IScriptComponent* script = m_scriptBridge
			? m_scriptBridge(scriptName)
			: nullptr;

		if(script && !script->HasRegistrationOrder()){
			script->SetRegistrationOrder(m_nextScriptRegistrationOrder++);
		}
		return script;
	}

	template<typename T>
	ComponentTypeID GetEngineComponentTypeID() const{
		auto it = m_engineTypeIDs.find(std::type_index(typeid(T)));
		if(it == m_engineTypeIDs.end()){
			return static_cast<ComponentTypeID>(-1);
		}
		return it->second;
	}

	bool ReloadScriptDLL(const char* dllPath);

	void SetScriptBridge(
		std::function<IScriptComponent*(const char*)> bridge){
		m_scriptBridge = std::move(bridge);
	}

	ComponentTypeID GetEngineComponentTypeIDByName(
		const std::string& name) const{
		auto it = m_nameToEngineTypeID.find(name);
		return it != m_nameToEngineTypeID.end()
			? it->second
			: static_cast<ComponentTypeID>(-1);
	}

	void SystemSetting() override {
		if(ImGui::Button("Reload Script DLL")){
			ReloadScriptDLL("Script/Script.dll");
		}
	}

	bool HasSystemSetting() const override { return true; }

private:
	SceneManagerContext* m_context = nullptr;
	EntityCommandCommitSystem m_commandCommitSystem;

	template<typename Func>
	void ForEachScript(SystemTaskDomain domain, Func&& func);

	std::unordered_map<std::string, ComponentTypeID> m_nameToEngineTypeID;
	std::unordered_map<std::type_index, ComponentTypeID> m_engineTypeIDs;
	std::function<IScriptComponent*(const char*)> m_scriptBridge = nullptr;
	std::function<void(void*)> m_setImGuiContextFunc = nullptr;
	uint64_t m_nextScriptRegistrationOrder = 1;
	HMODULE m_scriptModule = nullptr;

	using CreateScriptFunc = IScriptComponent * (*)(const char*);
};
