#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"
#include "GameTimeManager.h"
#include "Component/TransformComponent.h"
#include <Component/modelRendererComponent.h>
#include "BallController.h"

class EnemyController: public CustomScriptComponent {
	BEGIN_REFLECT(EnemyController)

		REFLECT_FIELD(float, moveSpeed, 0.0f)
		REFLECT_FIELD(float, dashMultiplier, 2.0f)
		REFLECT_FIELD(float, accel, 10.0f)
		REFLECT_FIELD(float, rotateSpeed, 8.0f)

		// スタミナ関連
		REFLECT_FIELD(float, maxStamina, 5.0f)
		REFLECT_FIELD(float, stamina, 5.0f)
		REFLECT_FIELD(float, staminaConsumeRate, 1.5f)
		REFLECT_FIELD(float, staminaRecoverRate, 1.0f)

		// ゴール座標
		REFLECT_FIELD(Vector3, goalPosition, Vector3(5.5f, 0.0f, 0.0f))

		bool canDash = true;
	float CurrentSpeed = 0.0f;

	TransformComponent* transform = nullptr;
	GameTimeManager* gameTime = nullptr;
	ModelRendererComponent* model = nullptr;

	// 追う対象
	TransformComponent* ballTransform = nullptr;
	BallController* ballController = nullptr;

public:

	EnemyController(): CustomScriptComponent("EnemyController"){}

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
		ImGui::ProgressBar(stamina / maxStamina, ImVec2(0.0f, 0.0f), "Stamina");
	}

	void OnStart() override{
		transform = GetComponent<TransformComponent>();
		model = GetComponent<ModelRendererComponent>();

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
		// アニメーション設定
		model->blendedAnimations.clear();
		if(model->model->m_Animation.find("Run") != model->model->m_Animation.end()){
			model->blendedAnimations.push_back({"Run", 0.0f, 0.0f});
		}
		if(model->model->m_Animation.find("Idle") != model->model->m_Animation.end()){
			model->blendedAnimations.push_back({"Idle", 1.0f, 0.0f});
		}

		if(!transform || !ballTransform || !gameTime || gameTime->CountDownTimer > 0.0f) return;

		// --- ゴール方向 ---
		Vector3 shootDir = (goalPosition - ballTransform->position);
		shootDir.y = 0.0f;
		if(shootDir.length() > 0.01f) shootDir = shootDir.normalize();

		// --- 敵からボール方向 ---
		Vector3 toBall = (ballTransform->position - transform->position);
		toBall.y = 0.0f;
		if(toBall.length() > 0.01f) toBall = toBall.normalize();

		// --- 角度判定 ---
		float dot = toBall.dot(shootDir);
		float angle = acosf(dot); // ラジアン
		float angleThreshold = DirectX::XMConvertToRadians(10.0f); // 10度

		// --- 移動ターゲット ---
		Vector3 moveTarget;
		if(angle > angleThreshold){
			// まだボール→ゴール線の正面にいない → 回り込み
			moveTarget = ballTransform->position - shootDir * 1.0f;
		} else{
			// 正面からボールを押す
			moveTarget = ballTransform->position;
		}

		// --- 移動方向 ---
		Vector3 dir = moveTarget - transform->position;
		dir.y = 0.0f;
		Vector3 move(0, 0, 0);
		if(dir.length() > 0.01f){
			dir = dir.normalize();
			move = dir;

			if(transform->position.y < ballTransform->position.y){
				moveTarget = ballTransform->position;
				dir = moveTarget - transform->position;
				move = dir * -1.0f;
			}
		}

		// ダッシュ判定
		bool isDashing = canDash;

		// スタミナ更新
		if(isDashing){
			stamina -= staminaConsumeRate * dt;
			if(stamina < 0.0f){
				stamina = 0.0f;
				canDash = false;
				isDashing = false;
			}
		} else{
			stamina += staminaRecoverRate * dt;
			if(stamina > maxStamina){
				stamina = maxStamina;
				canDash = true;
			}
		}



		// 入力がなければ減速
		if(move.x == 0 && move.z == 0){
			CurrentSpeed -= moveSpeed * dt;
			if(CurrentSpeed < 0.0f) CurrentSpeed = 0.0f;
			if(CurrentSpeed > moveSpeed) CurrentSpeed = moveSpeed;

			model->blendedAnimations[0].weight = CurrentSpeed / moveSpeed;
			model->blendedAnimations[1].weight = (1.0f - CurrentSpeed / moveSpeed);
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

			model->blendedAnimations[0].weight = CurrentSpeed / targetSpeed;
			model->blendedAnimations[1].weight = (1.0f - CurrentSpeed / targetSpeed);

			// --- 位置更新 ---
			transform->position += dir * (CurrentSpeed * dt);

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

	void OnFixedUpdate(float dt)override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt)override{}
	void OnStop() override{}

private:
	TransformComponent* m_transform = nullptr;
};
