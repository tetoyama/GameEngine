#include "CustomScriptSystem.h"
#include "Component/CustomScriptComponent.h"

#include "C#ScriptSystem.h"
#include "Scene.h"
#include "sceneManager.h"
#include "DebugTools/debugSystem.h"

#include "Registry/entityRegistry.h"
#include "Registry/componentRegistry.h"

void CustomScriptSystem::Initialize(){
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		auto scripts = context->component->GetAllBaseComponents<CustomScriptComponent>();
		for (auto& [entity, script] : scripts) {
			if (script && !script->IsInitialized()) {
				script->SetContext(context, entity);
				script->Initialize();
			}
		}
	}
}

void CustomScriptSystem::Finalize() {
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		auto scripts = context->component->GetAllBaseComponents<CustomScriptComponent>();
		for (auto& [entity, script] : scripts) {
			if (script && !script->IsInitialized()) {
				script->SetContext(context, entity);
				script->Initialize();
			}
			script->Stop();
		}
	}
}

void CustomScriptSystem::Start(){
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		auto scripts = context->component->GetAllBaseComponents<CustomScriptComponent>();
		for (auto& [entity, script] : scripts) {
			if (script && !script->IsInitialized()) {
				script->SetContext(context, entity);
				script->Initialize();
			}
			script->Start();
		}
	}
}

void CustomScriptSystem::Update(float deltaTime){
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		auto scripts = context->component->GetAllBaseComponents<CustomScriptComponent>();
		for (auto& [entity, script] : scripts) {
			if (script) {
				if (!script->IsInitialized()) {
					script->SetContext(context, entity);
					script->Initialize();
					script->Start();
				}

				script->Update(deltaTime);
			}
		}
	}
}

void CustomScriptSystem::FixedUpdate(float fixedDeltaTime){
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		auto scripts = context->component->GetAllBaseComponents<CustomScriptComponent>();
		for (auto& [entity, script] : scripts) {
			if (script) {
				if (!script->IsInitialized()) {
					script->SetContext(context, entity);
					script->Initialize();
				}
				script->FixedUpdate(fixedDeltaTime);
			}
		}
	}
}

void CustomScriptSystem::Draw(){
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		auto scripts = context->component->GetAllBaseComponents<CustomScriptComponent>();
		for (auto& [entity, script] : scripts) {
			if (script) {
				if (!script->IsInitialized()) {
					script->SetContext(context, entity);
					script->Initialize();
				}
				script->Draw();
			}
		}
	}
}

void CustomScriptSystem::EditorUpdate(float deltaTime){
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		auto scripts = context->component->GetAllBaseComponents<CustomScriptComponent>();
		for (auto& [entity, script] : scripts) {
			if (script) {
				if (!script->IsInitialized()) {
					script->SetContext(context, entity);
					script->Initialize();
				}
				script->EditorUpdate(deltaTime);
			}
		}
	}
}
