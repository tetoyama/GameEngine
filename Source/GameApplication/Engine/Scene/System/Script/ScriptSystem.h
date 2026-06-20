// =======================================================================
//
// ScriptSystem.h
//
// =======================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>

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
		IScriptComponent* script = m_createScriptFunc
			? m_createScriptFunc(scriptName)
			: nullptr;

		if(script && !script->HasRegistrationOrder()){
			script->SetRegistrationOrder(m_nextScriptRegistrationOrder++);
		}
		return script;
	}

	const ScriptModuleAPI& GetModuleAPI() const {
		return m_moduleAPI;
	}

	void Destroy(IScriptComponent* script) const {
		if(script && m_moduleAPI.destroy){
			m_moduleAPI.destroy(script);
		}
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
	using CreateScriptFunc = IScriptComponent* (*)(const char*);

	SceneManagerContext* m_context = nullptr;
	EntityCommandCommitSystem m_commandCommitSystem;

	template<typename Func>
	void ForEachScript(SystemTaskDomain domain, Func&& func);

	void DestroyAllScriptInstances();
	void UnloadCurrentModule();

	std::unordered_map<std::string, ComponentTypeID> m_nameToEngineTypeID;
	std::unordered_map<std::type_index, ComponentTypeID> m_engineTypeIDs;
	std::function<void(void*)> m_setImGuiContextFunc = nullptr;
	uint64_t m_nextScriptRegistrationOrder = 1;
	uint64_t m_reloadGeneration = 0;
	HMODULE m_scriptModule = nullptr;
	CreateScriptFunc m_createScriptFunc = nullptr;
	ScriptModuleAPI m_moduleAPI{};
	std::wstring m_loadedModulePath;
};
