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
#include <utility>
#include <vector>

void ScriptSystem::Start(){
	ForEachScript(SystemTaskDomain::Frame, [](IScriptComponent* script){
		script->Start();
	});
}

void ScriptSystem::Stop(){
	ForEachScript(SystemTaskDomain::Frame, [](IScriptComponent* script){
		script->Stop();
	});
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
	if(m_scriptModule){
		m_scriptBridge = nullptr;
		m_setImGuiContextFunc = nullptr;
		FreeLibrary(m_scriptModule);
		m_scriptModule = nullptr;
	}

	m_scriptModule = LoadLibraryA(originalPath);
	if(!m_scriptModule){
		return false;
	}

	auto createFunc = reinterpret_cast<CreateScriptFunc>(
		GetProcAddress(m_scriptModule, "CreateScript")
	);
	if(!createFunc){
		return false;
	}

	m_scriptBridge = [createFunc](const char* scriptName){
		return createFunc(scriptName);
	};

	auto rawFunc = reinterpret_cast<SetImGuiContextFunc>(
		GetProcAddress(m_scriptModule, "SetImGuiContext")
	);

	m_setImGuiContextFunc = [rawFunc](void* context){
		if(rawFunc){
			rawFunc(context);
		}
	};

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
}

template<typename Func>
void ScriptSystem::ForEachScript(SystemTaskDomain domain, Func&& func){
	struct Entry {
		IScriptComponent* script = nullptr;
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

			for(auto& [scriptName, script] : component->scripts){
				if(!script) continue;

				if(!script->HasRegistrationOrder()){
					script->SetRegistrationOrder(m_nextScriptRegistrationOrder++);
				}

				script->ref = EntityRef(entity, context);
				entries.push_back({script});
			}
		}
	}

	std::sort(
		entries.begin(),
		entries.end(),
		[domain](const Entry& lhs, const Entry& rhs){
			return IsScriptOrderEarlier(
				lhs.script->GetExecutionOrder(domain),
				rhs.script->GetExecutionOrder(domain)
			);
		}
	);

	for(const Entry& entry : entries){
		std::forward<Func>(func)(entry.script);
	}
}
