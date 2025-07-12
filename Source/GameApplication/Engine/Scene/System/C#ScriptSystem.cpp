#include "C#ScriptSystem.h"
#include "Scene.h"
#include "sceneManager.h"
#include "Engine/DebugTools/debugSystem.h"

#include "Engine/Resources/resourceService.h"
#include "Engine/Resources/Data/modelData.h"
#include "Engine/Resources/Loader/modelLoader.h"
#include "Engine/Resources/Loader/shaderLoader.h"
#include "Engine/Resources/Loader/textureLoader.h"
#include "Engine/Resources/Data/vertexShaderData.h"
#include "Engine/Resources/Data/pixelShaderData.h"

#include "Registry/entityRegistry.h"
#include "Registry/componentRegistry.h"
#include "Component/C#ScriptComponent.h"

void CSharpScriptSystem::Start(){
	// コンポーネントを持つエンティティの検索
	const auto& Entities = m_context->component->FindEntitiesWithComponent<CSharpScriptComponent>();
	if(Entities.empty()){
		return;
	} else{
		for(Entity entity : Entities){
			CSharpScriptComponent* script = m_context->component->GetComponent<CSharpScriptComponent>(entity);
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

void CSharpScriptSystem::Update(float deltaTime){
	// コンポーネントを持つエンティティの検索
	const auto& Entities = m_context->component->FindEntitiesWithComponent<CSharpScriptComponent>();
	if(Entities.empty()){
		return;
	} else{
		for(Entity entity : Entities){
			CSharpScriptComponent* script = m_context->component->GetComponent<CSharpScriptComponent>(entity);
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

void CSharpScriptSystem::FixedUpdate(float fidedDeltaTime){
	// コンポーネントを持つエンティティの検索
	const auto& Entities = m_context->component->FindEntitiesWithComponent<CSharpScriptComponent>();
	if(Entities.empty()){
		return;
	} else{
		for(Entity entity : Entities){
			CSharpScriptComponent* script = m_context->component->GetComponent<CSharpScriptComponent>(entity);
			if(script){
				// スクリプトの更新
				script->OnFixedUpdate(fidedDeltaTime);
			} else{
				m_context->manager->debug->LOG_ERROR("ScriptComponent not found for entity: " + std::to_string(entity));
			}
		}
	}
}

void CSharpScriptSystem::Draw(){}

void CSharpScriptSystem::EditorUpdate(float deltaTime){}
