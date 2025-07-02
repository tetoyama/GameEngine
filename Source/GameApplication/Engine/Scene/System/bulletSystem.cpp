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
#include "Component/textureComponent.h"
#include "Component/BillBoardRendererComponent.h"
#include "Component/meshRendererComponent.h"
#include "Component/explosionEffectComponent.h"

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

				TextureComponent* texture = m_context->component->GetComponent<TextureComponent>(bulletEntity);

				bullet->currentLifeTime += deltaTime;
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
						effectTexture->m_TextureData = m_context->manager->resource->GetTextureLoader()->LoadTexture("Asset\\Texture\\explosion.png");
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

				const auto& enemyEntities = m_context->component->FindEntitiesWithComponent<EnemyComponent>();
				if(!enemyEntities.empty()){

					for(Entity enemyEntity : enemyEntities){

						TransformComponent* enemyTransform = m_context->component->GetComponent<TransformComponent>(enemyEntity);
						if(transform && enemyTransform){

							if((enemyTransform->position - transform->position).length() < 2.0f){



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
									effectTexture->m_TextureData = m_context->manager->resource->GetTextureLoader()->LoadTexture("Asset\\Texture\\explosion.png");
									//texture->m_TextureData = m_SceneManagerContext->resource->GetTextureLoader()->LoadTexture("Asset\\Texture\\texture.jpg");

									if (enemyTexture) {
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

void BulletSystem::FixedUpdate(float fidedDeltaTime){
	// コンポーネントを持つエンティティの検索
	const auto& bulletEntities = m_context->component->FindEntitiesWithComponent<BulletComponent>();
	if(bulletEntities.empty()){
		return;
	} else{
		for(Entity bulletEntity : bulletEntities){
			BulletComponent* bullet = m_context->component->GetComponent<BulletComponent>(bulletEntity);
			bullet->particleCount = (bullet->particleCount + 1) % 3; // パーティクルのカウントを更新
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
					effectTexture->m_TextureData = m_context->manager->resource->GetTextureLoader()->LoadTexture("Asset\\Texture\\explosion.png");

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
