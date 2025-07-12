#include "enemySystem.h"
#include "Component/enemyComponent.h"

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

#include "Component/entityNameComponent.h"
#include "Component/transformComponent.h"
#include "Component/modelRendererComponent.h"
#include "Component/bulletComponent.h"

#include "bulletSystem.h"

#include "Engine/Graphics/mainRenderer.h"

void EnemySystem::Update(float deltaTime){
	// コンポーネントを持つエンティティの検索
	const auto& Entities = m_context->component->FindEntitiesWithComponent<EnemyComponent>();
	if(Entities.empty()){
		return;
	} else{
		for(Entity enemyEntity : Entities){
			EnemyComponent* enemy = m_context->component->GetComponent<EnemyComponent>(enemyEntity);
			if(enemy){
				TransformComponent* transform = m_context->component->GetComponent<TransformComponent>(enemyEntity);
				if(transform){
					transform->position += transform->front().normalize() * deltaTime * enemy->MoveSpeed;

					Vector3 MoveVec = (enemy->TargetPos - transform->position);

					if(enemy->setOriginPos && MoveVec.length() > 0.1f){

						float setRotation = atan2f(MoveVec.x, MoveVec.z);
						if(DirectX::XM_PI < setRotation - transform->rotation.y){
							setRotation -= DirectX::XM_2PI;
						}
						if(-DirectX::XM_PI > setRotation - transform->rotation.y){
							setRotation += DirectX::XM_2PI;
						}
						transform->rotation.y += (setRotation - transform->rotation.y) * deltaTime;
					} else{

						if(!enemy->setOriginPos){
							enemy->OriginPos = transform->position;
							enemy->setOriginPos = true;
						}

						TransformComponent randomDir;
						randomDir.position = Vector3(1.0f, 0.0f, 0.0f);
						randomDir.rotation.y = 0.0001f * (rand() % 10000) * DirectX::XM_2PI;
						enemy->TargetPos = enemy->OriginPos + randomDir.front() * 0.01f * (float)(rand() % 51 + 50) * (float)enemy->maxDistance;
					}
				}
			}
		}
	}
}
