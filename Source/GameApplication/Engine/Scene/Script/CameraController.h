#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"


class CameraController: public CustomScriptComponent {
public:
	BEGIN_REFLECT(CameraController)
		REFLECT_FIELD(float, distance, 6.0f)
		REFLECT_FIELD(float, height, 2.0f)
		REFLECT_FIELD(float, rotateSpeed, 1.5f)
		REFLECT_FIELD(float, minPitch, 0)
		REFLECT_FIELD(float, maxPitch, DirectX::XM_PIDIV2 * 0.5f)

	float yaw = 0.0f;
	float pitch = 0.0f;

	ComponentRef<TransformComponent> transform;
	ComponentRef<CameraComponent> CameraBuffer;
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
		auto playerEntities = m_ref.GetScene()->component->FindEntitiesWithComponent<PlayerController>();
		if(!playerEntities.empty()){
			playerTransform = GetComponentRefFor<TransformComponent>(playerEntities[0]);
		}

		auto ballEntities = m_ref.GetScene()->component->FindEntitiesWithComponent<BallController>();
		if(!ballEntities.empty()){
			ballEntity = GetEntityRefFor(ballEntities[0]);
		}

		auto timerEntities = m_ref.GetScene()->component->FindEntitiesWithComponent<GameTimeManager>();
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

		DirectX::XMVECTOR camPos = DirectX::XMVectorAdd(targetPos, offset);
		DirectX::XMFLOAT3 pos;
		DirectX::XMStoreFloat3(&pos, camPos);
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
