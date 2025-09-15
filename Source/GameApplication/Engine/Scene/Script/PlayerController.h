#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"

#include "GameTimeManager.h"

#include "Component/TransformComponent.h"


class PlayerController: public CustomScriptComponent {
	BEGIN_REFLECT(PlayerController)

		REFLECT_FIELD(float, moveSpeed, 0.0f)
		REFLECT_FIELD(float, health, 0.0f)
		REFLECT_FIELD(bool, isInvincible, false)

		float CurrentSpeed = 0.0f;
	TransformComponent* transform = nullptr;
	TransformComponent* cameraTransform = nullptr;
	GameTimeManager* gameTime = nullptr;

public:

	PlayerController(): CustomScriptComponent("PlayerController"){}

	YAML::Node encode() override{
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}
	bool decode(const YAML::Node& node) override{
		DECODE_FIELDS(node);
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text(scriptName.c_str());

		INSPECTOR_FIELDS();
	}

	void OnStart() override{
		transform = GetComponent<TransformComponent>();

		auto cameraEntities = m_context->component->FindEntitiesWithComponent<CameraComponent>();
		if(cameraEntities.empty()){
			return;
		} else{
			cameraTransform = m_context->component->GetComponent<TransformComponent>(cameraEntities[0]);
		}
		CurrentSpeed = 0.0f;


		auto timerEntities = m_context->component->FindEntitiesWithComponent<GameTimeManager>();
		if(timerEntities.empty()){
			return;
		} else{
			gameTime = m_context->component->GetComponent<GameTimeManager>(timerEntities[0]);
		}
	}
	void OnUpdate(float dt) override{
		if(!transform || !cameraTransform || gameTime->CountDownTimer > 0.0f) return;

		Vector3 move(0, 0, 0);

		if(GetKey('W')) move.z += 1.0f;
		if(GetKey('S')) move.z -= 1.0f;
		if(GetKey('A')) move.x -= 1.0f;
		if(GetKey('D')) move.x += 1.0f;

		// 入力がなければ移動なし
		if(move.x == 0 && move.z == 0){
			CurrentSpeed -= moveSpeed * dt;
			if(0.0f < CurrentSpeed){
				CurrentSpeed = 0.0f;
			}
		} else{
			CurrentSpeed += moveSpeed * dt;
			if(CurrentSpeed > moveSpeed){
				CurrentSpeed = moveSpeed;
			}
			// --- カメラの forward/right を使う ---
			Vector3 camForward = cameraTransform->front();
			Vector3 camRight = cameraTransform->right();

			// Y方向成分を落としてXZ平面に制限
			camForward.y = 0;
			camRight.y = 0;

			// 正規化
			camForward = camForward.normalize();
			camRight = camRight.normalize();

			// 移動方向 = forward/backward + strafe
			Vector3 dir = camForward * move.z + camRight * move.x;
			dir = dir.normalize();

			// --- 位置更新 ---
			float speed = moveSpeed;
			transform->position += dir * (speed * dt);

			// --- 回転補間 ---
			DirectX::XMVECTOR targetQ = DirectX::XMQuaternionRotationRollPitchYaw(
				0.0f,
				atan2f(-dir.x, -dir.z), // Y軸回転
				0.0f
			);

			DirectX::XMVECTOR currentQ = transform->rotationVector();
			float rotateSpeed = 8.0f; // 補間速度
			DirectX::XMVECTOR newQ = DirectX::XMQuaternionSlerp(currentQ, targetQ, rotateSpeed * dt);
			DirectX::XMFLOAT4 FloatQ;

			DirectX::XMStoreFloat4(&FloatQ, newQ);
			transform->SetRotation(FloatQ);
		}
	}
	void OnFixedUpdate(float dt)override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt)override{}
	void OnStop() override{}

private:

	TransformComponent* m_transform = nullptr;

};
