// =======================================================================
//
// CameraController.h
//
// =======================================================================
#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"
#include <System/Physic/physicSystem.h>
#include <algorithm>
#include <cmath>

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

		// 補間設定
		REFLECT_FIELD(float, targetPosLerpSpeed, 15.0f)
		REFLECT_FIELD(float, cameraDistanceLerpSpeed, 12.0f)
		REFLECT_FIELD(float, targetLookLerpSpeed, 10.0f)

		float yaw = 0.0f;
	float pitch = 0.0f;

	// RayCast 時に除外するレイヤービット（プレイヤーレイヤー固定）
	uint32_t selfLayerBit = 1u << 1;

	ComponentRef<TransformComponent> transform;
	ComponentRef<CameraComponent> CameraBuffer;
	ComponentRef<TransformComponent> playerTransform;
	EntityRef ballEntity;
	ComponentRef<GameTimeManager> gameTime;

	// 補間用
	float currentCameraDistance = 6.0f;
	Vector3 smoothTargetPos = Vector3(0, 0, 0);
	Vector3 currentLookTarget = Vector3(0, 0, 0);

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
		auto charEntities = m_ref.GetScene()->component->FindEntitiesWithComponent<CharacterController>();
		if(!charEntities.empty()){
			playerTransform = GetComponentRefFor<TransformComponent>(charEntities[0]);
		} else{
			auto playerEntities = m_ref.GetScene()->component->FindEntitiesWithComponent<PlayerController>();
			if(!playerEntities.empty()){
				playerTransform = GetComponentRefFor<TransformComponent>(playerEntities[0]);
			}
		}

		auto ballEntities = m_ref.GetScene()->component->FindEntitiesWithComponent<BallController>();
		if(!ballEntities.empty()){
			ballEntity = GetEntityRefFor(ballEntities[0]);
		}

		auto timerEntities = m_ref.GetScene()->component->FindEntitiesWithComponent<GameTimeManager>();
		if(!timerEntities.empty()){
			gameTime = GetComponentRefFor<GameTimeManager>(timerEntities[0]);
		}

		currentCameraDistance = distance;

		if(playerTransform){
			auto* targetTransform = playerTransform.Get();
			if(targetTransform){
				smoothTargetPos = targetTransform->position;
				currentLookTarget = targetTransform->position + Vector3(0.0f, height, 0.0f);
			}
		}
	}

	void OnUpdate(float dt) override{
		if(!transform || !playerTransform) return;

		auto* targetTransform = playerTransform.Get();
		if(!targetTransform) return;

		// プレイヤー位置を補間して追従する
		float targetPosT = 1.0f - std::exp(-targetPosLerpSpeed * dt);
		smoothTargetPos += (targetTransform->position - smoothTargetPos) * targetPosT;

		// --- キーボード入力で yaw/pitch 更新 ---
		if(GetKey(VK_LEFT))  yaw -= rotateSpeed * dt;
		if(GetKey(VK_RIGHT)) yaw += rotateSpeed * dt;
		if(GetKey(VK_UP))    pitch += rotateSpeed * dt;
		if(GetKey(VK_DOWN))  pitch -= rotateSpeed * dt;

		if(minPitch < maxPitch){
			pitch = std::clamp(pitch, minPitch, maxPitch);
		}

		// --- カメラ位置計算 ---
		DirectX::XMVECTOR offset =
			DirectX::XMVectorSet(0, height, -distance, 0);

		DirectX::XMVECTOR rot =
			DirectX::XMQuaternionRotationRollPitchYaw(
				pitch,
				yaw,
				0);

		offset = DirectX::XMVector3Rotate(offset, rot);

		DirectX::XMVECTOR targetPos =
			DirectX::XMVectorSet(
				smoothTargetPos.x,
				smoothTargetPos.y,
				smoothTargetPos.z,
				1.0f);

		// ------------------------------------------------
		// 遮蔽判定
		// ------------------------------------------------

		constexpr float occlusionPadding = 0.1f;
		constexpr float minGroundClearance = 0.3f;

		auto* phys =
			m_ref.GetScene()->manager
			->systemRegistry
			->GetSystem<PhysicSystem>();

		DirectX::XMVECTOR desiredCamPos =
			DirectX::XMVectorAdd(targetPos, offset);

		DirectX::XMVECTOR eyePos =
			DirectX::XMVectorSet(
				smoothTargetPos.x,
				smoothTargetPos.y + height * 0.5f,
				smoothTargetPos.z,
				1.0f);

		DirectX::XMVECTOR toCamera =
			DirectX::XMVectorSubtract(
				desiredCamPos,
				eyePos);

		float freeCamLen =
			DirectX::XMVectorGetX(
				DirectX::XMVector3Length(
					toCamera));

		if(freeCamLen < 0.001f){
			freeCamLen = distance;
		}

		DirectX::XMVECTOR camDir =
			DirectX::XMVector3Normalize(toCamera);

		float targetDistance = freeCamLen;

		if(phys){
			DirectX::XMFLOAT3 eyeF;
			DirectX::XMFLOAT3 dirF;

			DirectX::XMStoreFloat3(&eyeF, eyePos);
			DirectX::XMStoreFloat3(&dirF, camDir);

			RayHit occHit =
				phys->RaycastWithMask(
					physx::PxVec3(
						eyeF.x,
						eyeF.y,
						eyeF.z),
					physx::PxVec3(
						dirF.x,
						dirF.y,
						dirF.z),
					freeCamLen,
					selfLayerBit);

			if(occHit.hit){
				targetDistance =
					(std::max)(
						occHit.distance - occlusionPadding,
						minDistance);
			}
		}

		float maxCameraDistance = (std::max)(minDistance, freeCamLen);

		// 遮蔽距離を補間してガタつきを抑える
		if(targetDistance < currentCameraDistance){
			currentCameraDistance = targetDistance;
		} else{
			float t = 1.0f - std::exp(-cameraDistanceLerpSpeed * dt);
			currentCameraDistance += (targetDistance - currentCameraDistance) * t;
		}

		currentCameraDistance =
			std::clamp(
				currentCameraDistance,
				minDistance,
				maxCameraDistance);

		// 最終カメラ位置
		DirectX::XMVECTOR camPos =
			DirectX::XMVectorAdd(
				eyePos,
				DirectX::XMVectorScale(
					camDir,
					currentCameraDistance));

		DirectX::XMFLOAT3 pos;
		DirectX::XMStoreFloat3(&pos, camPos);

		// 地面めり込み防止
		pos.y =
			(std::max)(
				pos.y,
				smoothTargetPos.y +
				minGroundClearance);

		transform->position =
			Vector3(
				pos.x,
				pos.y,
				pos.z);

		// ------------------------------------------------
		// TPS風ターゲット補正
		// ------------------------------------------------

		CameraBuffer->isLock = true;

		Vector3 lookTarget =
			smoothTargetPos;

		// 通常時は頭付近を見る
		lookTarget.y += height;

		// カメラが近いほど、さらに少し上を見る
		float ratio = 0.0f;
		if(freeCamLen > 0.001f){
			ratio =
				1.0f -
				(currentCameraDistance / freeCamLen);

			ratio =
				std::clamp(
					ratio,
					0.0f,
					1.0f);
		}

		lookTarget.y += ratio * height * 0.5f;

		// 注視点も補間してガタつきを抑える
		float lookT = 1.0f - std::exp(-targetLookLerpSpeed * dt);
		currentLookTarget += (lookTarget - currentLookTarget) * lookT;

		CameraBuffer->Target = currentLookTarget;

		// ------------------------------------------------
		// LookAt
		// ------------------------------------------------

		Vector3 forward =
			(CameraBuffer->Target -
			 transform->position).normalize();

		float forwardLen = forward.length();
		if(forwardLen > 0.0001f){
			float yawLook =
				atan2f(
					forward.x,
					forward.z);

			float pitchLook =
				asinf(
					-forward.y);

			DirectX::XMVECTOR q =
				DirectX::XMQuaternionRotationRollPitchYaw(
					pitchLook,
					yawLook,
					0);

			DirectX::XMFLOAT4 temp;
			DirectX::XMStoreFloat4(&temp, q);
			transform->SetRotation(temp);
		}
	}

	void OnFixedUpdate(float dt) override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt) override{}
	void OnStop() override{}

private:
};