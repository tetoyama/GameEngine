#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"
#include "GameTimeManager.h"
#include "BallController.h"
#include "Component/TransformComponent.h"
#include "Component/ColliderComponent.h"

class PlayerController: public CustomScriptComponent {
	BEGIN_REFLECT(PlayerController)

		REFLECT_FIELD(float, moveSpeed, 6.0f)
		REFLECT_FIELD(float, health, 0.0f)
		REFLECT_FIELD(bool, isInvincible, false)

		// ダッシュ用パラメータ
		REFLECT_FIELD(float, dashMultiplier, 2.0f)
		REFLECT_FIELD(float, accel, 10.0f)
		REFLECT_FIELD(float, rotateSpeed, 8.0f)

		// スタミナ関連
		REFLECT_FIELD(float, maxStamina, 5.0f)
		REFLECT_FIELD(float, stamina, 5.0f)
		REFLECT_FIELD(float, staminaConsumeRate, 1.5f)
		REFLECT_FIELD(float, staminaRecoverRate, 1.0f)

		REFLECT_FIELD(float, jumpPower, 5.0f)


	bool canDash = true;
	float CurrentSpeed = 0.0f;
	bool isJumpPressed = false;

	TransformComponent* transform = nullptr;
	TransformComponent* cameraTransform = nullptr;
	GameTimeManager* gameTime = nullptr;
	ModelRendererComponent* model = nullptr;
	TransformComponent* ballTransform = nullptr;
	BallController* ballController = nullptr;
public:

	PlayerController(): CustomScriptComponent("PlayerController"){}

	YAML::Node encode() override{
		YAML::Node node;
		ENCODE_FIELDS(node);
		node["Stamina"] = stamina;
		return node;
	}
	bool decode(SceneContext* context, const YAML::Node& node) override{
		DECODE_FIELDS(node);
		if(node["Stamina"]) stamina = node["Stamina"].as<float>();
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text(scriptName.c_str());
		INSPECTOR_FIELDS();

		// スタミナ表示
		ImGui::ProgressBar(stamina / maxStamina, ImVec2(0.0f, 0.0f), "Stamina");
	}

	void OnStart() override{
		transform = GetComponent<TransformComponent>();
		model = GetComponent<ModelRendererComponent>();
		auto cameraEntities = m_context->component->FindEntitiesWithComponent<CameraComponent>();
		if(!cameraEntities.empty()){
			cameraTransform = m_context->component->GetComponent<TransformComponent>(cameraEntities[0]);
		}

		auto timerEntities = m_context->component->FindEntitiesWithComponent<GameTimeManager>();
		if(!timerEntities.empty()){
			gameTime = m_context->component->GetComponent<GameTimeManager>(timerEntities[0]);
		}

		auto ballEntities = m_context->component->FindEntitiesWithComponent<BallController>();
		if(!ballEntities.empty()){
			ballController = m_context->component->GetComponent<BallController>(ballEntities[0]);
			ballTransform = m_context->component->GetComponent<TransformComponent>(ballEntities[0]);
		}

		canDash = true;
		CurrentSpeed = 0.0f;
		stamina = maxStamina;
	}

	void OnUpdate(float dt) override{
		
	}

	void OnFixedUpdate(float dt)override{
		model->model->blendedAnimations.clear();

		// Run アニメーションを追加
		if(model->model->m_Animation.find("Run") != model->model->m_Animation.end()){
			model->model->blendedAnimations.push_back({"Run", 0.0f, 0.0f});
			// name = "Run", weight = 1.0, startTime = 0.0
		}

		// Idle アニメーションを追加
		if(model->model->m_Animation.find("Idle") != model->model->m_Animation.end()){
			model->model->blendedAnimations.push_back({"Idle", 1.0f, 0.0f});
			// name = "Idle", weight = 0.0, startTime = 0.0
		}
		if(!transform || !cameraTransform) return;
		if(gameTime && gameTime->CountDownTimer <= 0.0f)return;


		Vector3 move(0, 0, 0);

		if(GetKey('W')) move.z += 1.0f;
		if(GetKey('S')) move.z -= 1.0f;
		if(GetKey('A')) move.x -= 1.0f;
		if(GetKey('D')) move.x += 1.0f;

		// ダッシュ判定
		bool dashKey = (GetKey(VK_LSHIFT));
		bool isDashing = dashKey && canDash;

		// スタミナ更新
		if(isDashing){
			stamina -= staminaConsumeRate * dt;
			if(stamina < 0.0f){
				stamina = 0.0f;
				canDash = false;
			}
		} else{
			stamina += staminaRecoverRate * dt;
			if(stamina > maxStamina){
				stamina = maxStamina;
				canDash = true;
			}
		}

		// --- カメラ基準の移動 ---
		Vector3 camForward = cameraTransform->front();
		Vector3 camRight = cameraTransform->right();
		camForward.y = 0; camRight.y = 0;
		camForward = camForward.normalize();
		camRight = camRight.normalize();
		Vector3 dir = camForward * move.z + camRight * move.x;
		dir = dir.normalize();

		ColliderComponent* collider = GetComponent<ColliderComponent>();

		if (!collider) {
			transform->position += dir * (CurrentSpeed * dt);
		} else {

			float Y = collider->pRigidbodyDynamic->getLinearVelocity().y;

			if (GetKey(VK_SPACE) && !isJumpPressed) {
				Y = jumpPower;
			}
			isJumpPressed = GetKey(VK_SPACE);

			collider->pRigidbodyDynamic->setLinearVelocity(physx::PxVec3(dir.x * CurrentSpeed, Y, dir.z * CurrentSpeed));
		}

		// 入力がなければ減速
		if(move.x == 0 && move.z == 0){
			CurrentSpeed -= moveSpeed * dt;
			if(CurrentSpeed < 0.0f) CurrentSpeed = 0.0f;
			if(CurrentSpeed > moveSpeed) CurrentSpeed = moveSpeed;
			model->model->blendedAnimations[0].weight = CurrentSpeed / (moveSpeed * dashMultiplier);
			model->model->blendedAnimations[1].weight = (1.0f - CurrentSpeed / (moveSpeed * dashMultiplier));

			// --- 回転補間 ---
			DirectX::XMVECTOR targetQ = DirectX::XMQuaternionRotationRollPitchYaw(
				0.0f,
				transform->GetRotationEuler().y,
				0.0f
			);

		} else{
			// 目標速度
			float targetSpeed = isDashing ? moveSpeed * dashMultiplier : moveSpeed;

			// 加速/減速補間
			if(CurrentSpeed < targetSpeed){
				CurrentSpeed += accel * dt;
				if(CurrentSpeed > targetSpeed) CurrentSpeed = targetSpeed;
			} else if(CurrentSpeed > targetSpeed){
				CurrentSpeed = targetSpeed;
			}

			model->model->blendedAnimations[0].weight = CurrentSpeed / (moveSpeed * dashMultiplier);
			model->model->blendedAnimations[1].weight = (1.0f - CurrentSpeed / (moveSpeed * dashMultiplier));

			// --- 回転補間 ---
			DirectX::XMVECTOR targetQ = DirectX::XMQuaternionRotationRollPitchYaw(
				0.0f,
				atan2f(dir.x, dir.z),
				0.0f
			);

			DirectX::XMVECTOR currentQ = transform->rotationVector();
			DirectX::XMVECTOR newQ = DirectX::XMQuaternionSlerp(currentQ, targetQ, rotateSpeed * dt);
			DirectX::XMFLOAT4 FloatQ;
			DirectX::XMStoreFloat4(&FloatQ, newQ);
			transform->SetRotation(FloatQ);
		}

		if(ballController && (ballTransform->position - transform->position).length() < 0.5f){
			Vector3 dir = ballTransform->position - transform->position;
			ballController->ApplyForce(dir); // ボールを吹っ飛ばす
		}
	}
	void OnDraw() override{}
	void OnEditorUpdate(float dt)override{}
	void OnStop() override{}

private:
	TransformComponent* m_transform = nullptr;


};
