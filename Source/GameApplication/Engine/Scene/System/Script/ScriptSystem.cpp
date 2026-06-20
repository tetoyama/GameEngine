// =======================================================================
//
// ScriptSystem.cpp
//
// =======================================================================
#include "ScriptSystem.h"
#include "Scene/Component/scriptComponent.h"
#include "Component/componentList.h"
#include "Scene/Reference/EntityRef.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

void ScriptSystem::Start(){
	m_commandCommitSystem.PrepareBuffers();

	ForEachScript(SystemTaskDomain::Frame, [](IScriptComponent* script){
		script->Start();
	});

	m_commandCommitSystem.CommitNow();
}

void ScriptSystem::Stop(){
	ForEachScript(SystemTaskDomain::Frame, [](IScriptComponent* script){
		script->Stop();
	});

	m_commandCommitSystem.CommitNow();
}

void ScriptSystem::Update(float deltaTime){
	ForEachScript(SystemTaskDomain::Frame, [deltaTime](IScriptComponent* script){
		script->Update(deltaTime);
	});
}

void ScriptSystem::FixedUpdate(float fixedDeltaTime){
	ForEachScript(SystemTaskDomain::Fixed, [fixedDeltaTime](IScriptComponent* script){
		script->FixedUpdate(fixedDeltaTime);
	});
}

void ScriptSystem::EditorUpdate(float deltaTime){
	ForEachScript(SystemTaskDomain::Editor, [deltaTime](IScriptComponent* script){
		script->EditorUpdate(deltaTime);
	});
}

void ScriptSystem::Draw(){
	if(m_setImGuiContextFunc){
		m_setImGuiContextFunc(static_cast<void*>(ImGui::GetCurrentContext()));
	}

	ForEachScript(SystemTaskDomain::Render, [](IScriptComponent* script){
		script->Draw();
	});
}

bool ScriptSystem::ReloadScriptDLL(const char* originalPath){
	if(!originalPath || originalPath[0] == '\0'){
		return false;
	}

	// DLLを解放する前に、旧DLL由来のvtableを持つ全Scriptを破棄する。
	if(m_scriptModule){
		DestroyAllScriptInstances();
		UnloadCurrentModule();
	}

	m_scriptModule = LoadLibraryA(originalPath);
	if(!m_scriptModule){
		return false;
	}

	m_createScriptFunc = reinterpret_cast<CreateScriptFunc>(
		GetProcAddress(m_scriptModule, "CreateScript")
	);
	m_destroyScriptFunc = reinterpret_cast<DestroyScriptFunc>(
		GetProcAddress(m_scriptModule, "DestroyScript")
	);

	if(!m_createScriptFunc || !m_destroyScriptFunc){
		UnloadCurrentModule();
		return false;
	}

	auto rawImGuiContextFunction = reinterpret_cast<SetImGuiContextFunc>(
		GetProcAddress(m_scriptModule, "SetImGuiContext")
	);

	if(rawImGuiContextFunction){
		m_setImGuiContextFunc = [rawImGuiContextFunction](void* context){
			rawImGuiContextFunction(context);
		};
	} else {
		m_setImGuiContextFunc = nullptr;
	}

	return true;
}

void ScriptSystem::Initialize(){
#define REGISTER(T, STORAGE) \
    RegisterEngineComponent<T>(#T);
	COMPONENT_LIST(REGISTER)
#undef REGISTER

	ReloadScriptDLL("Script/Script.dll");
}

void ScriptSystem::Finalize(){
	m_commandCommitSystem.CommitNow();
	DestroyAllScriptInstances();
	UnloadCurrentModule();
}

void ScriptSystem::DestroyAllScriptInstances(){
	if(!m_context || !m_context->sceneManager){
		return;
	}

	for(auto& [sceneName, scene] : m_context->sceneManager->GetActiveScenes()){
		if(!scene) continue;

		SceneContext* context = scene->GetSceneContext();
		if(!context || !context->component) continue;

		const auto entities =
			context->component->FindEntitiesWithComponent<ScriptComponent>();

		for(Entity entity : entities){
			ScriptComponent* component =
				context->component->GetComponent<ScriptComponent>(entity);
			if(!component) continue;

			component->DestroyAllScripts();
			component->SetDestroyFunction(nullptr);
		}
	}
}

void ScriptSystem::UnloadCurrentModule(){
	m_createScriptFunc = nullptr;
	m_destroyScriptFunc = nullptr;
	m_setImGuiContextFunc = nullptr;

	if(m_scriptModule){
		FreeLibrary(m_scriptModule);
		m_scriptModule = nullptr;
	}

	if(!m_loadedModulePath.empty()){
		std::error_code error;
		std::filesystem::remove(m_loadedModulePath, error);
		m_loadedModulePath.clear();
	}
}

template<typename Func>
void ScriptSystem::ForEachScript(SystemTaskDomain domain, Func&& func){
	struct Entry {
		IScriptComponent* script = nullptr;
		Entity entity{};
		std::string name;
	};

	std::vector<Entry> entries;

	for(auto& [sceneName, scene] : m_context->sceneManager->GetActiveScenes()){
		auto* context = scene->GetSceneContext();
		auto entities =
			context->component->FindEntitiesWithComponent<ScriptComponent>();

		for(Entity entity : entities){
			auto* component =
				context->component->GetComponent<ScriptComponent>(entity);
			if(!component) continue;

			if(!component->GetDestroyFunction()){
				component->SetDestroyFunction(m_destroyScriptFunc);
			}

			for(auto& [scriptName, script] : component->scripts){
				if(!script) continue;

				if(!script->HasRegistrationOrder()){
					script->SetRegistrationOrder(m_nextScriptRegistrationOrder++);
				}

				script->ref = EntityRef(entity, context);
				entries.push_back({script, entity, scriptName});
			}
		}
	}

	std::sort(
		entries.begin(),
		entries.end(),
		[domain](const Entry& lhs, const Entry& rhs){
			const SystemTaskOrder lhsOrder =
				lhs.script->GetExecutionOrder(domain);
			const SystemTaskOrder rhsOrder =
				rhs.script->GetExecutionOrder(domain);

			if(IsScriptOrderEarlier(lhsOrder, rhsOrder)) return true;
			if(IsScriptOrderEarlier(rhsOrder, lhsOrder)) return false;

			if(lhs.entity.GetPackedValue() != rhs.entity.GetPackedValue()){
				return lhs.entity.GetPackedValue() < rhs.entity.GetPackedValue();
			}
			return lhs.name < rhs.name;
		}
	);

	for(const Entry& entry : entries){
		func(entry.script);
	}
}
