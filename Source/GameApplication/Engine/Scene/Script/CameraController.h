#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"


class CameraController: public CustomScriptComponent {
public:
	BEGIN_REFLECT(CameraController)
		REFLECT_FIELD(float, distance, 6.0f)
		REFLECT_FIELD(float, height, 2.0f)
		REFLECT_FIELD(float, rotateSpeed, 1.5f)
		REFLECT_FIELD(float, minPitch, -DirectX::XM_PIDIV2 + 0.1f)
		REFLECT_FIELD(float, maxPitch, DirectX::XM_PIDIV2 - 0.1f)

	float yaw = 0.0f;
	float pitch = 0.0f;

	TransformComponent* transform = nullptr;
	CameraComponent* camera = nullptr;
	Entity playerEntity = -1;
	Entity ballEntity = -1;
	GameTimeManager* gameTime = nullptr;

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
		transform = GetComponent<TransformComponent>();
		camera = GetComponent<CameraComponent>();
		auto playerEntities = m_context->component->FindEntitiesWithComponent<PlayerController>();
		if(playerEntities.empty()){
			return;
		} else{
			playerEntity = playerEntities[0];
		}

		auto ballEntities = m_context->component->FindEntitiesWithComponent<BallController>();
		if(playerEntities.empty()){
			return;
		} else{
			playerEntity = playerEntities[0];
		}

		auto timerEntities = m_context->component->FindEntitiesWithComponent<GameTimeManager>();
		if(timerEntities.empty()){
			return;
		} else{
			gameTime = m_context->component->GetComponent<GameTimeManager>(timerEntities[0]);
		}
	}

	void OnUpdate(float dt) override{
	
		if(!transform || playerEntity < 0) return;

		auto* targetTransform = m_context->component->GetComponent<TransformComponent>(playerEntity);
		if(!targetTransform) return;

		// --- キーボード入力で yaw/pitch 更新 ---
		if(GetKey(VK_LEFT))  yaw -= rotateSpeed * dt;
		if(GetKey(VK_RIGHT)) yaw += rotateSpeed * dt;
		if(GetKey(VK_UP))    pitch += rotateSpeed * dt;
		if(GetKey(VK_DOWN))  pitch -= rotateSpeed * dt;

		pitch = std::clamp(pitch, minPitch, maxPitch);

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

		// --- プレイヤー方向を向く ---
		Vector3 forward = (targetTransform->position - transform->position).normalize();
		float yawLook = atan2f(forward.x, forward.z);
		float pitchLook = asinf(-forward.y);

		DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(pitchLook, yawLook, 0);
		DirectX::XMFLOAT4 temp;
		DirectX::XMStoreFloat4(&temp, q);
		transform->SetRotation(temp);
	}
	void OnFixedUpdate(float dt)override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt)override{}
	void OnStop() override{}

private:
};
