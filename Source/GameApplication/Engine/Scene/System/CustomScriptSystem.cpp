#include "CustomScriptSystem.h"
#include "Component/CustomScriptComponent.h"

#include "C#ScriptSystem.h"
#include "Scene.h"
#include "sceneManager.h"
#include "Engine/DebugTools/debugSystem.h"

#include "Engine/Resources/resourceSystem.h"
#include "Engine/Resources/Data/modelData.h"
#include "Engine/Resources/Loader/modelLoader.h"
#include "Engine/Resources/Loader/shaderLoader.h"
#include "Engine/Resources/Loader/textureLoader.h"
#include "Engine/Resources/Data/vertexShaderData.h"
#include "Engine/Resources/Data/pixelShaderData.h"

#include "Registry/entityRegistry.h"
#include "Registry/componentRegistry.h"

void CustomScriptSystem::Start(){
	auto scripts = m_context->component->GetAllBaseComponents<CustomScriptComponent>();
	for(auto& [entity, script] : scripts){
		if(script && !script->IsInitialized()){
			script->SetContext(m_context,entity);
			script->Initialize();
		}
	}
}

void CustomScriptSystem::Update(float deltaTime){
	auto scripts = m_context->component->GetAllBaseComponents<CustomScriptComponent>();
	for(auto& [entity, script] : scripts){
		if(script){
			if(!script->IsInitialized()){
				script->SetContext(m_context, entity);
				script->Initialize();
			}

			script->Update(deltaTime);
		}
	}
}

void CustomScriptSystem::FixedUpdate(float fixedDeltaTime){
	auto scripts = m_context->component->GetAllBaseComponents<CustomScriptComponent>();
	for(auto& [entity, script] : scripts){
		if(script){
			if(!script->IsInitialized()){
				script->SetContext(m_context, entity);
				script->Initialize();
			}
			script->FixedUpdate(fixedDeltaTime);
		}
	}
}

void CustomScriptSystem::Draw(){
	auto scripts = m_context->component->GetAllBaseComponents<CustomScriptComponent>();
	for(auto& [entity, script] : scripts){
		if(script){
			if(!script->IsInitialized()){
				script->SetContext(m_context, entity);
				script->Initialize();
			}
			script->Draw();
		}
	}
}

void CustomScriptSystem::EditorUpdate(float deltaTime){
	auto scripts = m_context->component->GetAllBaseComponents<CustomScriptComponent>();
	for(auto& [entity, script] : scripts){
		if(script){
			if(!script->IsInitialized()){
				script->SetContext(m_context, entity);
				script->Initialize();
			}
			script->EditorUpdate(deltaTime);
		}
	}
}
