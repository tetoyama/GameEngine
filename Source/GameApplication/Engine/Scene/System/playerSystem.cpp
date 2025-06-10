#include "playerSystem.h"

#include "Scene.h"
#include "sceneManager.h"
#include "Engine/DebugTools/debugSystem.h"

#include "Registry/entityRegistry.h"
#include "Registry/componentRegistry.h"

#include "Component/playerComponent.h"
#include "Component/transformComponent.h"
#include "Component/cameraComponent.h"

#include "Engine/Graphics/mainRenderer.h"

#include "Engine/Platform/InputSystem/InputSystem.h"

void PlayerSystem::Initialize(){
	m_context->manager->debug->LOG_DEBUG(u8"PlayerSystemを初期化中...");
}

void PlayerSystem::Start() {
	// 入力システムの取得
	m_inputSystem = m_context->manager->input;

	// カメラの取得
	const auto& cameraEntity = m_context->component->FindEntitiesWithComponent<CameraComponent>();
	if (!cameraEntity.empty()) {
		m_cameraComponent = m_context->component->GetComponent<CameraComponent>(cameraEntity[0]);
		m_cameraTransform = m_context->component->GetComponent<TransformComponent>(cameraEntity[0]);
	} else {
		return;
	}
}

void PlayerSystem::Update(float deltaTime) {

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
			m_cameraTransform->rotation.y += deltaTime * player->cameraRotate * (m_inputSystem->IsKey(m_context->manager->hwnd, VK_RIGHT) - m_inputSystem->IsKey(m_context->manager->hwnd, VK_LEFT));
			m_cameraTransform->rotation.x += deltaTime * player->cameraRotate * (m_inputSystem->IsKey(m_context->manager->hwnd, VK_UP) - m_inputSystem->IsKey(m_context->manager->hwnd, VK_DOWN));
			if (m_cameraTransform->rotation.x > -0.2f) {
				m_cameraTransform->rotation.x = -0.2f;
			}
			if (m_cameraTransform->rotation.x < -DirectX::XM_PI * 0.4f) {
				m_cameraTransform->rotation.x = -DirectX::XM_PI * 0.4f;
			}

			// プレイヤーの移動
			Vector3 moveVec{};
			moveVec.x += deltaTime * player->moveSpeed * (m_inputSystem->IsKey(m_context->manager->hwnd, 'D') - m_inputSystem->IsKey(m_context->manager->hwnd, 'A'));
			moveVec.z += deltaTime * player->moveSpeed * (m_inputSystem->IsKey(m_context->manager->hwnd, 'W') - m_inputSystem->IsKey(m_context->manager->hwnd, 'S'));
			Vector3 rotatedMoveVec{};
			rotatedMoveVec.x += moveVec.z * sin(m_cameraTransform->rotation.y) + moveVec.x * cos(m_cameraTransform->rotation.y);
			rotatedMoveVec.z += moveVec.z * cos(m_cameraTransform->rotation.y) - moveVec.x * sin(m_cameraTransform->rotation.y);

			float setRotation = atan2f(rotatedMoveVec.x, rotatedMoveVec.z);
			if (DirectX::XM_PI < setRotation - transform->rotation.y) {
				setRotation -= DirectX::XM_2PI;
			}
			if (-DirectX::XM_PI > setRotation - transform->rotation.y) {
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
		}
	}
}
