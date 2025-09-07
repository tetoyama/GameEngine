#include "playerSystem.h"

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

#include "Component/playerComponent.h"
#include "Component/entityNameComponent.h"
#include "Component/transformComponent.h"
#include "Component/cameraComponent.h"
#include "Component/modelRendererComponent.h"
#include "Component/bulletComponent.h"
#include "Component/textureComponent.h"
#include "Component/outlineComponent.h"

#include "Engine/Graphics/mainRenderer.h"

#include "Engine/Platform/InputSystem/InputSystem.h"
#include <Component/audioComponent.h>

void PlayerSystem::Initialize(){
	m_context->manager->debug->LOG_DEBUG("PlayerSystemを初期化中...");
}

void PlayerSystem::Start() {
}

void PlayerSystem::Update(float deltaTime) {
	CameraComponent* m_cameraComponent = nullptr;
	TransformComponent * m_cameraTransform = nullptr;
	// カメラの取得
	const auto& cameraEntity = m_context->component->FindEntitiesWithComponent<CameraComponent>();
	if(!cameraEntity.empty()){
		m_cameraComponent = m_context->component->GetComponent<CameraComponent>(cameraEntity[0]);
		m_cameraTransform = m_context->component->GetComponent<TransformComponent>(cameraEntity[0]);
	} else{
		return;
	}

	auto input = m_context->manager->input;

	// コンポーネントを持つエンティティの検索
	const auto& playerEntity = m_context->component->FindEntitiesWithComponent<PlayerComponent>();
	if (playerEntity.empty()) {
		return;
	} else {
		for (Entity entity : playerEntity) {

			// プレイヤコンポーネントの取得
			PlayerComponent* player = m_context->component->GetComponent<PlayerComponent>(entity);
			if (!player) {
				continue;
			}
			// プレイヤーのトランスフォームの取得
			TransformComponent* transform = m_context->component->GetComponent<TransformComponent>(entity);
			if (!transform) {
				continue;
			}

			// カメラの回転
			m_cameraTransform->rotation.y += deltaTime * player->cameraRotate * (input->IsKey(m_context->manager->hwnd, VK_RIGHT) - input->IsKey(m_context->manager->hwnd, VK_LEFT));
			m_cameraTransform->rotation.x += deltaTime * player->cameraRotate * (input->IsKey(m_context->manager->hwnd, VK_UP) - input->IsKey(m_context->manager->hwnd, VK_DOWN));
			if (m_cameraTransform->rotation.x > -0.2f) {
				m_cameraTransform->rotation.x = -0.2f;
			}
			if (m_cameraTransform->rotation.x < -DirectX::XM_PI * 0.4f) {
				m_cameraTransform->rotation.x = -DirectX::XM_PI * 0.4f;
			}

			// プレイヤーの移動
			Vector3 moveVec{};
			moveVec.x += deltaTime * player->moveSpeed * (input->IsKey(m_context->manager->hwnd, 'D') - input->IsKey(m_context->manager->hwnd, 'A'));
			moveVec.z += deltaTime * player->moveSpeed * (input->IsKey(m_context->manager->hwnd, 'W') - input->IsKey(m_context->manager->hwnd, 'S'));
			Vector3 rotatedMoveVec{};
			rotatedMoveVec.x += moveVec.z * sin(m_cameraTransform->rotation.y) + moveVec.x * cos(m_cameraTransform->rotation.y);
			rotatedMoveVec.z += moveVec.z * cos(m_cameraTransform->rotation.y) - moveVec.x * sin(m_cameraTransform->rotation.y);

			float setRotation = atan2f(rotatedMoveVec.x, rotatedMoveVec.z);
			while (DirectX::XM_PI < setRotation - transform->rotation.y) {
				setRotation -= DirectX::XM_2PI;
			}
			while(-DirectX::XM_PI > setRotation - transform->rotation.y) {
				setRotation += DirectX::XM_2PI;
			}
			if (rotatedMoveVec.length() > 0) {
				transform->rotation.y += (setRotation - transform->rotation.y) * deltaTime * player->cameraLerp;
			}
			transform->position += rotatedMoveVec;

			// カメラの位置
			Vector3 setCameraPos = transform->position - m_cameraTransform->front() * player->cameraDistance;
			m_cameraTransform->position += (setCameraPos - m_cameraTransform->position) * deltaTime * player->cameraLerp;
			m_cameraComponent->isLock = true;
			m_cameraComponent->Target = transform->position;

			if(input->IsKeyDown(m_context->manager->hwnd, VK_SPACE)){
				
				// 効果音の再生
				auto Audio = m_context->component->GetComponent<AudioComponent>(entity);
				if(Audio){
					Audio->Play(m_context->manager->audio);
				}

				Entity bulletEntity = m_context->entity->Create();
				NameComponent* name = m_context->component->AddComponent<NameComponent>(bulletEntity);
				name->name = "Bullet";

				TransformComponent* bulletTransform = m_context->component->AddComponent<TransformComponent>(bulletEntity);
				transform = m_context->component->GetComponent<TransformComponent>(entity); //Addしたタイミングでメモリコピーが行われると本来の参照元の位置と違う場所になるので再取得...アーキタイプで管理しない方がいいのかも
				bulletTransform->position = transform->position + Vector3(0.0f,1.0f,0.0f);
				bulletTransform->rotation = transform->rotation;
				bulletTransform->scale = transform->scale * 2.0f;

				auto* model = m_context->component->GetComponent<ModelRendererComponent>(entity);

				ModelRendererComponent* bulletModelRenderer = m_context->component->AddComponent<ModelRendererComponent>(bulletEntity);
				bulletModelRenderer->model = m_context->manager->resource->Load<ModelData>("Asset\\Model\\ball.fbx");
				bulletModelRenderer->vertexShader = m_context->manager->resource->Load<VertexShaderData>(model->vertexShader->FilePath);
				bulletModelRenderer->pixelShader = m_context->manager->resource->Load<PixelShaderData>(model->pixelShader->FilePath);

				TextureComponent* texture = m_context->component->AddComponent<TextureComponent>(entity);
				if (texture) {
					TextureComponent* bulletTexture = m_context->component->AddComponent<TextureComponent>(bulletEntity);
					bulletTexture->Material = texture->Material;
					bulletTexture->m_TextureData = m_context->manager->resource->Load<TextureData>("Asset\\Texture\\white.tga");
				}

				BulletComponent* bullet = m_context->component->AddComponent<BulletComponent>(bulletEntity);
				//OutlineComponent* outline = m_context->component->AddComponent<OutlineComponent>(bulletEntity);
				//bullet->lifeTime = 1.0f;
			}
		}
	}
}
