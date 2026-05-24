// =======================================================================
// 
// CameraController.h
// 
// =======================================================================
#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"
#include <System/Physic/physicSystem.h>


// カメラ制御スクリプト
class CameraController: public CustomScriptComponent {
public:
	BEGIN_REFLECT(CameraController)
		REFLECT_FIELD(float, distance, 6.0f)
		REFLECT_FIELD(float, height, 2.0f)
		REFLECT_FIELD(float, rotateSpeed, 1.5f)
		REFLECT_FIELD(float, minPitch, DirectX::XM_PIDIV2 * -0.5f)
		REFLECT_FIELD(float, maxPitch, DirectX::XM_PIDIV2 * 0.5f)
		REFLECT_FIELD(float, minDistance, 1.0f)

	float yaw = 0.0f;
	float pitch = 0.0f;

	// RayCast 時に除外するレイヤービット（プレイヤーレイヤー固定）
	uint32_t selfLayerBit = 1u << 1;

	ComponentRef<TransformComponent> transform;
	ComponentRef<CameraComponent> cameraBuffer;
	ComponentRef<TransformComponent> playerTransform;
	EntityRef ballEntity;
	ComponentRef<GameTimeManager> gameTime;

		CameraController(): CustomScriptComponent("CameraController"){}

	YAML::Node encode() override{
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}
	bool decode(SceneContext* context, const YAML::Node& node) override{
		DECODE_FIELDS(node);
		return true;
	}

	void inspector(SceneContext* context) override{
		INSPECTOR_FIELDS();
	}

	void OnStart() override{
		transform = GetComponentRef<TransformComponent>();
		CameraBuffer = GetComponentRef<CameraComponent>();

		// CharacterController を優先して探し、なければ PlayerController を使う
		auto charEntities = m_ref.GetScene()->pComponent->FindEntitiesWithComponent<CharacterController>();
		if (!charEntities.empty()) {
			playerTransform = GetComponentRefFor<TransformComponent>(charEntities[0]);
		} else {
			auto playerEntities = m_ref.GetScene()->pComponent->FindEntitiesWithComponent<PlayerController>();
			if (!playerEntities.empty()) {
				playerTransform = GetComponentRefFor<TransformComponent>(playerEntities[0]);
			}
		}

		auto ballEntities = m_ref.GetScene()->pComponent->FindEntitiesWithComponent<BallController>();
		if(!ballEntities.empty()){
			ballEntity = GetEntityRefFor(ballEntities[0]);
		}

		auto timerEntities = m_ref.GetScene()->pComponent->FindEntitiesWithComponent<GameTimeManager>();
		if(!timerEntities.empty()){
			gameTime = GetComponentRefFor<GameTimeManager>(timerEntities[0]);
		}
	}

	void OnUpdate(float dt) override{

	}
	void OnFixedUpdate(float dt)override{

		if(!transform || !playerTransform) return;

		auto* targetTransform = playerTransform.Get();
		if(!targetTransform) return;

		// --- キーボード入力で yaw/pitch 更新 ---
		if(GetKey(VK_LEFT))  yaw -= rotateSpeed * dt;
		if(GetKey(VK_RIGHT)) yaw += rotateSpeed * dt;
		if(GetKey(VK_UP))    pitch += rotateSpeed * dt;
		if(GetKey(VK_DOWN))  pitch -= rotateSpeed * dt;

		if (minPitch < maxPitch) {
			pitch = std::clamp(pitch, minPitch, maxPitch);
		}
		// --- カメラ位置計算 ---
		DirectX::XMVECTOR offset = DirectX::XMVectorSet(0, height, -distance, 0);
		DirectX::XMVECTOR rot = DirectX::XMQuaternionRotationRollPitchYaw(pitch, yaw, 0);
		offset = DirectX::XMVector3Rotate(offset, rot);

		DirectX::XMVECTOR targetPos = DirectX::XMVectorSet(
			targetTransform->position.x,
			targetTransform->position.y,
			targetTransform->position.z,
			1.0f
		);

		// --- カメラ遮蔽判定（遮蔽物があればカメラを近づける） ---
		constexpr float occlusionPadding   = 0.1f;  // 遮蔽面からの安全マージン
		constexpr float minGroundClearance = 0.3f;  // ターゲット足元からの最低高さ
		auto* phys = m_ref.GetScene()->pManager
			->pSystemRegistry
			->GetSystem<PhysicSystem>();

		DirectX::XMFLOAT3 pos;
		DirectX::XMVECTOR camPos = DirectX::XMVectorAdd(targetPos, offset);
		if (phys) {
			// 視点位置: ターゲットの中心付近（足元ヒットを避けるため高さ半分）
			DirectX::XMVECTOR eyePos = DirectX::XMVectorSet(
				targetTransform->position.x,
				targetTransform->position.y + height * 0.5f,
				targetTransform->position.z,
				1.0f);

			DirectX::XMVECTOR toCamera = DirectX::XMVectorSubtract(camPos, eyePos);
			float toCamLen = DirectX::XMVectorGetX(DirectX::XMVector3Length(toCamera));

			if (toCamLen > 0.001f) {
				DirectX::XMFLOAT3 eyeF, dirF;
				DirectX::XMStoreFloat3(&eyeF, eyePos);
				DirectX::XMStoreFloat3(&dirF, DirectX::XMVector3Normalize(toCamera));

				RayHit occHit = phys->RaycastWithMask(
					physx::PxVec3(eyeF.x, eyeF.y, eyeF.z),
					physx::PxVec3(dirF.x, dirF.y, dirF.z),
					toCamLen,
					selfLayerBit);

				if (occHit.hit) {
					float safeLen = (std::max)(occHit.distance - occlusionPadding, minDistance);
					camPos = DirectX::XMVectorSet(
						eyeF.x + dirF.x * safeLen,
						eyeF.y + dirF.y * safeLen,
						eyeF.z + dirF.z * safeLen,
						1.0f);
				}
			}
		}

		DirectX::XMStoreFloat3(&pos, camPos);
		// 地面めり込み防止: カメラ Y がターゲット足元を下回らないようにする
		pos.y = (std::max)(pos.y, targetTransform->position.y + minGroundClearance);
		transform->position = Vector3(pos.x, pos.y, pos.z);
		transform->SetRotationEuler(Vector3(pitch, yaw, 0));

		CameraBuffer->isLock = true;
		CameraBuffer->Target = targetTransform->position;

		// --- プレイヤー方向を向く ---
		Vector3 forward = (targetTransform->position - transform->position - Vector3(0,-height,0)).normalize();
		float yawLook = atan2f(forward.x, forward.z);
		float pitchLook = asinf(-forward.y);
		DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(pitchLook, yawLook, 0);
		DirectX::XMFLOAT4 temp;
		DirectX::XMStoreFloat4(&temp, q);
		transform->SetRotation(temp);
	}
	void OnDraw() override{}
	void OnEditorUpdate(float dt)override{}
	void OnStop() override{}

private:
};
