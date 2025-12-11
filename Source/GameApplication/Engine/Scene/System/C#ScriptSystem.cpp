#include "C#ScriptSystem.h"
#include "Scene.h"
#include "sceneManager.h"
#include "DebugTools/debugSystem.h"

#include "Resources/resourceService.h"
#include "Resources/Data/modelData.h"
#include "Resources/Loader/modelLoader.h"
#include "Resources/Loader/shaderLoader.h"
#include "Resources/Loader/textureLoader.h"
#include "Resources/Data/vertexShaderData.h"
#include "Resources/Data/pixelShaderData.h"

#include "Registry/entityRegistry.h"
#include "Registry/componentRegistry.h"
#include "Component/C#ScriptComponent.h"

void CSharpScriptSystem::Start(){
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
	// コンポーネントを持つエンティティの検索
		const auto& Entities = context->component->FindEntitiesWithComponent<CSharpScriptComponent>();
		if (Entities.empty()) {
			return;
		} else {
			for (Entity entity : Entities) {
				CSharpScriptComponent* script = context->component->GetComponent<CSharpScriptComponent>(entity);
				if (script) {
					// スクリプトの初期化
					if (!script->IsInitialized()) {
						script->OnStart();
					} else {
						m_context->debug->LOG_ERROR("ScriptComponent already initialized for entity: " + std::to_string(entity));
					}
				} else {
					m_context->debug->LOG_ERROR("ScriptComponent not found for entity: " + std::to_string(entity));
				}
			}

		}
	}
}

void CSharpScriptSystem::Update(float deltaTime){
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
	// コンポーネントを持つエンティティの検索
		const auto& Entities = context->component->FindEntitiesWithComponent<CSharpScriptComponent>();
		if (Entities.empty()) {
			return;
		} else {
			for (Entity entity : Entities) {
				CSharpScriptComponent* script = context->component->GetComponent<CSharpScriptComponent>(entity);
				if (script) {
					// スクリプトの初期化
					if (!script->IsInitialized()) {
						script->OnStart();
					}
					// スクリプトの更新
					script->OnUpdate(deltaTime);
				} else {
					m_context->debug->LOG_ERROR("ScriptComponent not found for entity: " + std::to_string(entity));
				}
			}
		}
	}
}

void CSharpScriptSystem::FixedUpdate(float fidedDeltaTime){
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
	// コンポーネントを持つエンティティの検索
		const auto& Entities = context->component->FindEntitiesWithComponent<CSharpScriptComponent>();
		if (Entities.empty()) {
			return;
		} else {
			for (Entity entity : Entities) {
				CSharpScriptComponent* script = context->component->GetComponent<CSharpScriptComponent>(entity);
				if (script) {
					// スクリプトの更新
					script->OnFixedUpdate(fidedDeltaTime);
				} else {
					m_context->debug->LOG_ERROR("ScriptComponent not found for entity: " + std::to_string(entity));
				}
			}
		}
	}
}

void CSharpScriptSystem::Draw(){}

void CSharpScriptSystem::EditorUpdate(float deltaTime){}
