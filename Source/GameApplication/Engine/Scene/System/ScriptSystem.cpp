#include "ScriptSystem.h"
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
#include "Component/ScriptComponent.h"

void ScriptSystem::Start(){
	// コンポーネントを持つエンティティの検索
	const auto& Entities = m_context->component->FindEntitiesWithComponent<ScriptComponent>();
	if(Entities.empty()){
		return;
	} else{
		for(Entity entity : Entities){
			ScriptComponent* script = m_context->component->GetComponent<ScriptComponent>(entity);
			if(script){
				// スクリプトの初期化
				if(!script->IsInitialized()){
					script->OnStart();
				} else{
					m_context->manager->debug->LOG_ERROR("ScriptComponent already initialized for entity: " + std::to_string(entity));
				}
			} else{
				m_context->manager->debug->LOG_ERROR("ScriptComponent not found for entity: " + std::to_string(entity));
			}
		}
	}
}

void ScriptSystem::Update(float deltaTime){
	// コンポーネントを持つエンティティの検索
	const auto& Entities = m_context->component->FindEntitiesWithComponent<ScriptComponent>();
	if(Entities.empty()){
		return;
	} else{
		for(Entity entity : Entities){
			ScriptComponent* script = m_context->component->GetComponent<ScriptComponent>(entity);
			if(script){
				// スクリプトの初期化
				if(!script->IsInitialized()){
					script->OnStart();
				}
				// スクリプトの更新
				script->OnUpdate(deltaTime);
			} else{
				m_context->manager->debug->LOG_ERROR("ScriptComponent not found for entity: " + std::to_string(entity));
			}
		}
	}
}

void ScriptSystem::FixedUpdate(float fidedDeltaTime){
	// コンポーネントを持つエンティティの検索
	const auto& Entities = m_context->component->FindEntitiesWithComponent<ScriptComponent>();
	if(Entities.empty()){
		return;
	} else{
		for(Entity entity : Entities){
			ScriptComponent* script = m_context->component->GetComponent<ScriptComponent>(entity);
			if(script){
				// スクリプトの更新
				script->OnFixedUpdate(fidedDeltaTime);
			} else{
				m_context->manager->debug->LOG_ERROR("ScriptComponent not found for entity: " + std::to_string(entity));
			}
		}
	}
}

void ScriptSystem::Draw(){}

void ScriptSystem::EditorUpdate(float deltaTime){}
