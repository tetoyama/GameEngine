#include "bulletSystem.h"
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
#include "Component/enemyComponent.h"
#include "Component/textureComponent.h"
#include "Component/BillBoardRendererComponent.h"
#include "Component/meshRendererComponent.h"
#include "Component/explosionEffectComponent.h"

#include "Engine/Graphics/mainRenderer.h"

#include "Engine/Platform/InputSystem/InputSystem.h"
#include <Script/ScoreManager.h>

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
					if(bullet->currentLifeTime == 0.0f){
						bullet->StartPos = transform->position;
					}

					transform->position += transform->front().normalize() * deltaTime * bullet->bulletSpeed;

				}

				TextureComponent* texture = m_context->component->GetComponent<TextureComponent>(bulletEntity);


				if(bullet->maxLifeTime <= bullet->currentLifeTime){

					if (transform) {
						Entity entity = m_context->entity->Create();

						auto* effectName = m_context->component->AddComponent<NameComponent>(entity);
						effectName->name = "BillBoardEffect";

						auto* effect = m_context->component->AddComponent<ExplosionEffectComponent>(entity);
						auto* bill = m_context->component->AddComponent<BillBoardRendererComponent>(entity);

						auto* effectTransform = m_context->component->AddComponent<TransformComponent>(entity);
						effectTransform->position = transform->position;
						effectTransform->scale = Vector3(1.0f, 1.0f, 1.0f);

						auto* effectTexture = m_context->component->AddComponent<TextureComponent>(entity);
						effectTexture->m_TextureData = m_context->manager->resource->Load<TextureData>("Asset\\Texture\\explosion.png");
						//texture->m_TextureData = m_SceneManagerContext->resource->GetTextureLoader()->LoadTexture(L"Asset\\Texture\\texture.jpg");

						if (texture) {
							effectTexture->Material = texture->Material;
						}

						effectTexture->UV_Slice_X = 4;
						effectTexture->UV_Slice_Y = 4;
						effectTexture->AnimationNum = 0;

					}

					m_context->entity->Destroy(bulletEntity);
					m_context->component->OnEntityDestroyed(bulletEntity);

					continue;
				}
				if(transform){
					TransformComponent* nearEnemy = nullptr;
					Entity nearID = 0;
					float MinDistance = 0.0f;

					const auto& enemyEntities = m_context->component->FindEntitiesWithComponent<EnemyComponent>();
					if(!enemyEntities.empty()){

						for(Entity enemyEntity : enemyEntities){

							TransformComponent* enemyTransform = m_context->component->GetComponent<TransformComponent>(enemyEntity);
							if(enemyTransform){

								float Distance = (enemyTransform->position - transform->position).length();

								if(Distance < 2.0f){

									{
										Entity entity = m_context->entity->Create();

										auto* effectName = m_context->component->AddComponent<NameComponent>(entity);
										effectName->name = "BillBoardEffect";

										auto* effect = m_context->component->AddComponent<ExplosionEffectComponent>(entity);
										auto* bill = m_context->component->AddComponent<BillBoardRendererComponent>(entity);

										auto* effectTransform = m_context->component->AddComponent<TransformComponent>(entity);
										effectTransform->position = (transform->position + enemyTransform->position) * 0.5f;
										effectTransform->scale = Vector3(2.5f, 2.5f, 2.5f);

										auto* effectTexture = m_context->component->AddComponent<TextureComponent>(entity);
										auto* enemyTexture = m_context->component->AddComponent<TextureComponent>(enemyEntity);
										effectTexture->m_TextureData = m_context->manager->resource->Load<TextureData>("Asset\\Texture\\explosion.png");
										//texture->m_TextureData = m_SceneManagerContext->resource->GetTextureLoader()->LoadTexture("Asset\\Texture\\texture.jpg");

										if(enemyTexture){
											effectTexture->Material = enemyTexture->Material;
										}

										effectTexture->UV_Slice_X = 4;
										effectTexture->UV_Slice_Y = 4;
										effectTexture->AnimationNum = 0;
									}



									m_context->entity->Destroy(bulletEntity);
									m_context->component->OnEntityDestroyed(bulletEntity);

									m_context->entity->Destroy(enemyEntity);
									m_context->component->OnEntityDestroyed(enemyEntity);

									auto entity = m_context->component->FindEntitiesWithComponent<ScoreManager>();
									if(entity.empty()){
										return;
									} else{
										m_context->component->GetComponent<ScoreManager>(entity[0])->Score += 1;
									}
									break;
									continue;
								} else if(!nearEnemy || Distance < MinDistance){

									if(Distance < 20.0f){
										MinDistance = Distance;
										nearEnemy = enemyTransform;
										nearID = enemyEntity;
									}
								}
							}
						}
						if(bullet->currentLifeTime == 0 && bullet->Target == 0){
							bullet->Target = nearID;
							bullet->maxLifeTime -= bullet->currentLifeTime;
							bullet->currentLifeTime = deltaTime;
							bullet->StartPos = transform->position;
						}
						if(nearEnemy && bullet->Target == nearID){
							bullet->TargetPos = nearEnemy->position;
						}
						if(bullet->Target != 0){
							Vector3 P0 = bullet->StartPos;
							Vector3 P2 = bullet->TargetPos;
							Vector3 P1 = (P0 + P2) * 0.5f + Vector3(0, 10, 0);

							float t = bullet->bulletSpeed * bullet->currentLifeTime / ((P0 - P1).length() + (P1 - P2).length());
							if(bullet->currentLifeTime / bullet->maxLifeTime * 1.1f > t){
								t = bullet->currentLifeTime / bullet->maxLifeTime * 1.1f;
							}
							t = std::clamp(t, 0.0f, 1.0f);
							if(t < 1.0f){
								Vector3 A = Vec3Lerp(P0, P1, t);
								Vector3 B = Vec3Lerp(P1, P2, t);
								Vector3 bezierPos = Vec3Lerp(A, B, t);

								Vector3 direction = (bezierPos - transform->position).normalize();

								float roll = 0.0f;
								float pitch = atan2f(direction.y, sqrtf(direction.x * direction.x + direction.z * direction.z));
								float yaw = atan2f(direction.x, direction.z);

								transform->rotation = Vector3(pitch, yaw, roll);
							}
						}
					}
				}
				bullet->currentLifeTime += deltaTime;

			}
		}
	}
}

void BulletSystem::FixedUpdate(float fidedDeltaTime){
	// コンポーネントを持つエンティティの検索
	const auto& bulletEntities = m_context->component->FindEntitiesWithComponent<BulletComponent>();
	if(bulletEntities.empty()){
		return;
	} else{
		for(Entity bulletEntity : bulletEntities){
			BulletComponent* bullet = m_context->component->GetComponent<BulletComponent>(bulletEntity);
			bullet->particleCount = (bullet->particleCount + 1) % 3;
			if(bullet){
				if(bullet->particleCount == 0) {
					Entity entity = m_context->entity->Create();
					TransformComponent* transform = m_context->component->GetComponent<TransformComponent>(bulletEntity);

					auto* effectName = m_context->component->AddComponent<NameComponent>(entity);
					effectName->name = "BillBoardEffect";

					auto* effect = m_context->component->AddComponent<ExplosionEffectComponent>(entity);
					effect->LifeTime = 0.5f;

					auto* bill = m_context->component->AddComponent<BillBoardRendererComponent>(entity);

					auto* effectTransform = m_context->component->AddComponent<TransformComponent>(entity);
					effectTransform->position = transform->position;
					effectTransform->scale = Vector3(1.0f, 1.0f, 1.0f);

					auto* effectTexture = m_context->component->AddComponent<TextureComponent>(entity);
					effectTexture->m_TextureData = m_context->manager->resource->Load<TextureData>("Asset\\Texture\\explosion.png");

					TextureComponent* texture = m_context->component->GetComponent<TextureComponent>(bulletEntity);
					if(texture){
						effectTexture->Material = texture->Material;
					}
					effectTexture->UV_Slice_X = 4;
					effectTexture->UV_Slice_Y = 4;
					effectTexture->AnimationNum = 0;


				}
			}
		}
	}
}
