#include "playerSystem.h"

#include "sceneManager.h"

#include "Entity/entityRegistry.h"

#include "Component/playerComponent.h"
#include "Component/transformComponent.h"
#include "Component/cameraComponent.h"

#include "Engine/Graphics/mainRenderer.h"

#include "Engine/Platform/InputSystem/InputSystem.h"

void PlayerSystem::Start() {
	// 入力システムの取得
	m_inputSystem = m_context->input;

	// カメラの取得
	const auto& cameraEntity = m_registry->FindEntitiesWithComponent<CameraComponent>();
	if (!cameraEntity.empty()) {
		m_cameraComponent = m_registry->GetComponent<CameraComponent>(cameraEntity[0]);
		m_cameraTransform = m_registry->GetComponent<TransformComponent>(cameraEntity[0]);
	} else {
		return;
	}
}

void PlayerSystem::Update(float deltaTime) {

	// コンポーネントを持つエンティティの検索
	const auto& playerEntity = m_registry->FindEntitiesWithComponent<PlayerComponent>();
	if (playerEntity.empty()) {
		return;
	} else {
		for (Entity entity : playerEntity) {

			// プレイヤコンポーネントの取得
			PlayerComponent* player = m_registry->GetComponent<PlayerComponent>(entity);
			if (!player) {
				continue;
			}
			// プレイヤーのトランスフォームの取得
			TransformComponent* transform = m_registry->GetComponent<TransformComponent>(entity);
			if (!transform) {
				continue;
			}

			// カメラの回転
			m_cameraTransform->rotation.y += deltaTime * player->cameraRotate * (m_inputSystem->IsKey(m_context->renderer->GetHWND(), VK_RIGHT) - m_inputSystem->IsKey(m_context->renderer->GetHWND(), VK_LEFT));
			m_cameraTransform->rotation.x += deltaTime * player->cameraRotate * (m_inputSystem->IsKey(m_context->renderer->GetHWND(), VK_UP) - m_inputSystem->IsKey(m_context->renderer->GetHWND(), VK_DOWN));
			if (m_cameraTransform->rotation.x > 0.0f) {
				m_cameraTransform->rotation.x = 0.0f;
			}
			if (m_cameraTransform->rotation.x < -DirectX::XM_PI * 0.4f) {
				m_cameraTransform->rotation.x = -DirectX::XM_PI * 0.4f;
			}

			// プレイヤーの移動
			Vector3 moveVec{};
			moveVec.x += deltaTime * player->moveSpeed * (m_inputSystem->IsKey(m_context->renderer->GetHWND(), 'D') - m_inputSystem->IsKey(m_context->renderer->GetHWND(), 'A'));
			moveVec.z += deltaTime * player->moveSpeed * (m_inputSystem->IsKey(m_context->renderer->GetHWND(), 'W') - m_inputSystem->IsKey(m_context->renderer->GetHWND(), 'S'));
			Vector3 rotatedMoveVec{};
			rotatedMoveVec.x += moveVec.z * sin(m_cameraTransform->rotation.y) + moveVec.x * cos(m_cameraTransform->rotation.y);
			rotatedMoveVec.z += moveVec.z * cos(m_cameraTransform->rotation.y) - moveVec.x * sin(m_cameraTransform->rotation.y);

			float setRotation = atan2f(rotatedMoveVec.x, rotatedMoveVec.z);
			if (DirectX::XM_PI < setRotation - transform->rotation.y) {
				setRotation -= DirectX::XM_2PI;
			} else if (-DirectX::XM_PI > setRotation - transform->rotation.y) {
				setRotation += DirectX::XM_2PI;
			}
			if (rotatedMoveVec.length() > 0) {
				transform->rotation.y += (setRotation - transform->rotation.y) * deltaTime * player->cameraLerp;
			}
			transform->position += rotatedMoveVec;

			// カメラの位置
			Vector3 setCameraPos = transform->position - m_cameraTransform->front() * 100.0f;
			m_cameraTransform->position += (setCameraPos - m_cameraTransform->position) * deltaTime * player->cameraLerp;
			m_cameraComponent->isLock = true;
			m_cameraComponent->Target = transform->position;
		}
	}
}
