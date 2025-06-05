#include "playerSystem.h"

#include "sceneManager.h"

#include "Entity/entityRegistry.h"

#include "Component/playerComponent.h"
#include "Component/transformComponent.h"
#include "Component/cameraComponent.h"

#include "Engine/Graphics/mainRenderer.h"

#include "Engine/Platform/InputSystem/InputSystem.h"

void PlayerSystem::Update(float deltaTime) {

	// 入力システムの取得
	InputSystem* inputSystem = m_context->input;

	// カメラの取得
	CameraComponent* cameraComponent;
	TransformComponent* cameraTransform;
	const auto& cameraEntity = m_registry->FindEntitiesWithComponent<CameraComponent>();
	if (!cameraEntity.empty()) {
		cameraComponent = m_registry->GetComponent<CameraComponent>(cameraEntity[0]);
		cameraTransform = m_registry->GetComponent<TransformComponent>(cameraEntity[0]);
	} else {
		return;
	}

	// コンポーネントを持つエンティティの検索
	const auto& playerEntity = m_registry->FindEntitiesWithComponent<PlayerComponent>();
	if (playerEntity.empty()) {
		return;
	} else {
		for (Entity entity : playerEntity) {

			// プレイヤコンポーネントの取得
			auto* player = m_registry->GetComponent<PlayerComponent>(entity);
			if (!player) {
				continue;
			}
			// プレイヤーのトランスフォームの取得
			auto* transform = m_registry->GetComponent<TransformComponent>(entity);
			if (!transform) {
				continue;
			}

			// カメラの回転
			cameraTransform->rotation.y += deltaTime * 2.0f * (inputSystem->IsKey(m_context->renderer->GetHWND(), VK_RIGHT) - inputSystem->IsKey(m_context->renderer->GetHWND(), VK_LEFT));
			cameraTransform->rotation.x += deltaTime * 2.0f * (inputSystem->IsKey(m_context->renderer->GetHWND(), VK_UP) - inputSystem->IsKey(m_context->renderer->GetHWND(), VK_DOWN));
			if (cameraTransform->rotation.x > 0.0f) {
				cameraTransform->rotation.x = 0.0f;
			}
			if (cameraTransform->rotation.x < -DirectX::XM_PI * 0.4f) {
				cameraTransform->rotation.x = -DirectX::XM_PI * 0.4f;
			}

			// プレイヤーの移動
			Vector3 moveVec{};
			moveVec.x += deltaTime * player->moveSpeed * (inputSystem->IsKey(m_context->renderer->GetHWND(), 'D') - inputSystem->IsKey(m_context->renderer->GetHWND(), 'A'));
			moveVec.z += deltaTime * player->moveSpeed * (inputSystem->IsKey(m_context->renderer->GetHWND(), 'W') - inputSystem->IsKey(m_context->renderer->GetHWND(), 'S'));
			Vector3 rotatedMoveVec{};
			rotatedMoveVec.x += moveVec.z * sin(cameraTransform->rotation.y) + moveVec.x * cos(cameraTransform->rotation.y);
			rotatedMoveVec.z += moveVec.z * cos(cameraTransform->rotation.y) - moveVec.x * sin(cameraTransform->rotation.y);

			float setRotation = atan2f(rotatedMoveVec.x, rotatedMoveVec.z);
			if (DirectX::XM_PI < setRotation - transform->rotation.y) {
				setRotation -= DirectX::XM_2PI;
			} else if (-DirectX::XM_PI > setRotation - transform->rotation.y) {
				setRotation += DirectX::XM_2PI;
			}
			if (rotatedMoveVec.length() > 0) {
				transform->rotation.y += (setRotation - transform->rotation.y) * deltaTime * 5.0f;
			}
			transform->position += rotatedMoveVec;

			// カメラの位置
			Vector3 setCameraPos = transform->position - cameraTransform->front() * 100.0f;
			cameraTransform->position += (setCameraPos - cameraTransform->position) * deltaTime * 4.0f;
			cameraComponent->isLock = true;
			cameraComponent->Target = transform->position;
		}
	}
}
