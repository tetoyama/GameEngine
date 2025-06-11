#include "bulletSystem.h"
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

#include "Component/entityNameComponent.h"
#include "Component/transformComponent.h"
#include "Component/modelRendererComponent.h"
#include "Component/bulletComponent.h"
#include "Component/enemyComponent.h"

#include "Engine/Graphics/mainRenderer.h"

#include "Engine/Platform/InputSystem/InputSystem.h"

void BulletSystem::Update(float deltaTime){
	// コンポーネントを持つエンティティの検索
	const auto& bulletEntities = m_context->component->FindEntitiesWithComponent<BulletComponent>();
	if(bulletEntities.empty()){
		return;
	} else{
		for(Entity bulletEntity : bulletEntities){
			BulletComponent* bullet = m_context->component->GetComponent<BulletComponent>(bulletEntity);
			if(bullet){
				TransformComponent* transform = m_context->component->GetComponent<TransformComponent>(bulletEntity);
				if(transform){
					transform->position += transform->front().normalize() * deltaTime * bullet->bulletSpeed;
				}

				bullet->currentLifeTime += deltaTime;
				if(bullet->maxLifeTime <= bullet->currentLifeTime){
					m_context->entity->Destroy(bulletEntity);
					m_context->component->OnEntityDestroyed(bulletEntity);
					continue;
				}

				const auto& enemyEntities = m_context->component->FindEntitiesWithComponent<EnemyComponent>();
				if(!enemyEntities.empty()){

					for(Entity enemyEntity : enemyEntities){

						TransformComponent* enemyTransform = m_context->component->GetComponent<TransformComponent>(enemyEntity);
						if(transform && enemyTransform){

							if((enemyTransform->position - transform->position).length() < 2.0f){
								m_context->entity->Destroy(bulletEntity);
								m_context->component->OnEntityDestroyed(bulletEntity);

								m_context->entity->Destroy(enemyEntity);
								m_context->component->OnEntityDestroyed(enemyEntity);
								break;
								continue;
							}
						}
					}
				}
			}
		}
	}
}
