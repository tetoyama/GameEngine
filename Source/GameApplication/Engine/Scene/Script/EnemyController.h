// =======================================================================
// 
// EnemyController.h
// 
// =======================================================================
#pragma once
#include "Component/CustomScriptComponent.h"
#include "GameTimeManager.h"
#include "Component/TransformComponent.h"
#include "Component/modelRendererComponent.h"
#include "BallController.h"

// 敵キャラクター制御スクリプト
class EnemyController: public CustomScriptComponent {
	BEGIN_REFLECT(EnemyController)

		REFLECT_FIELD(float, moveSpeed, 2.0f)
		REFLECT_FIELD(float, dashMultiplier, 2.0f)
		REFLECT_FIELD(float, accel, 6.0f)
		REFLECT_FIELD(float, rotateSpeed, 8.0f)

		REFLECT_FIELD(float, maxStamina, 5.0f)
		REFLECT_FIELD(float, stamina, 5.0f)
		REFLECT_FIELD(float, staminaConsumeRate, 1.5f)
		REFLECT_FIELD(float, staminaRecoverRate, 1.0f)

		REFLECT_FIELD(Vector3, goalPosition, Vector3(5.5f, 0.0f, 0.0f))

		enum class m_EnemyState{
		MoveToPushPos,
		PushBall
	};

	EnemyState m_State= EnemyState::MoveToPushPos;

	bool m_CanDash= true;
	float m_CurrentSpeed= 0.0f;

	ComponentRef<TransformComponent> m_Transform;
	ComponentRef<ModelRendererComponent> m_Model;
	ComponentRef<GameTimeManager> m_GameTime;

	ComponentRef<TransformComponent> m_BallTransform;
	ComponentRef<BallController> m_BallController;

public:
	EnemyController(): CustomScriptComponent("EnemyController"){}

	// ---------------- Serialization ----------------
	YAML::Node encode() override{
		YAML::Node node;
		ENCODE_FIELDS(node);
		node["Stamina"] = stamina;
		return node;
	}

	bool decode(SceneContext*, const YAML::Node& node) override{
		DECODE_FIELDS(node);
		if(node["Stamina"]) stamina = node["Stamina"].as<float>();
		return true;
	}

	// ---------------- Inspector ----------------
	void inspector(SceneContext*) override{
		ImGui::Text(scriptName.c_str());
		INSPECTOR_FIELDS();
		ImGui::ProgressBar(stamina / maxStamina, ImVec2(0, 0), "Stamina");

		ImGui::Text("State: %s",
					state == EnemyState::PushBall ? "PushBall" : "MoveToPushPos");
	}

	// ---------------- Lifecycle ----------------
	void OnStart() override{
		transform = GetComponentRef<TransformComponent>();
		model = GetComponentRef<ModelRendererComponent>();

		auto timers = m_ref.GetScene()->pComponent->FindEntitiesWithComponent<GameTimeManager>();
		if(!timers.empty())
			gameTime = GetComponentRefFor<GameTimeManager>(timers[0]);

		auto balls = m_ref.GetScene()->pComponent->FindEntitiesWithComponent<BallController>();
		if(!balls.empty()){
			ballController = GetComponentRefFor<BallController>(balls[0]);
			ballTransform = GetComponentRefFor<TransformComponent>(balls[0]);
		}

		stamina = maxStamina;
		canDash = true;
		CurrentSpeed = 0.0f;
		state = EnemyState::MoveToPushPos;

		model->blendedAnimations.clear();
		model->blendedAnimations.push_back({"Run",  0.0f, 0.0f});
		model->blendedAnimations.push_back({"Idle", 1.0f, 0.0f});
	}

	void OnUpdate(float) override{}

	// ---------------- FixedUpdate ----------------
	void OnFixedUpdate(float dt) override{
		if(!transform || !ballTransform || !gameTime) return;
		if(gameTime->CountDownTimer > 0.0f) return;

		Vector3 enemyPos = transform->position;
		Vector3 ballPos = ballTransform->position;

		// ---------- ゴール方向 ----------
		Vector3 goalDir = goalPosition - ballPos;
		goalDir.y = 0.0f;
		if(goalDir.length() < 0.01f) return;
		goalDir = goalDir.normalize();

		// ---------- 理想押し位置 ----------
		const float pushOffset = 1.0f;
		Vector3 idealPos = ballPos - goalDir * pushOffset;

		Vector3 toIdeal = idealPos - enemyPos;
		toIdeal.y = 0.0f;
		float distToIdeal = toIdeal.length();

		// ---------- 状態遷移（ヒステリシス） ----------
		const float enterPushDist = 0.35f;
		const float exitPushDist = 0.60f;

		if(state == EnemyState::MoveToPushPos){
			if(distToIdeal < enterPushDist)
				state = EnemyState::PushBall;
		} else{ // PushBall
			if(distToIdeal > exitPushDist)
				state = EnemyState::MoveToPushPos;
		}

		// ---------- 移動方向 ----------
		Vector3 moveDir(0, 0, 0);
		if(state == EnemyState::MoveToPushPos){
			if(distToIdeal > 0.01f)
				moveDir = toIdeal.normalize();
		} else{
			moveDir = goalDir;
		}

		// ---------- スタミナ ----------
		bool isDashing = (state == EnemyState::PushBall) && canDash;

		if(isDashing){
			stamina -= staminaConsumeRate * dt;
			if(stamina <= 0.0f){
				stamina = 0.0f;
				canDash = false;
			}
		} else{
			stamina += staminaRecoverRate * dt;
			if(stamina >= maxStamina){
				stamina = maxStamina;
				canDash = true;
			}
		}

		float targetSpeed = moveSpeed * (isDashing ? dashMultiplier : 1.0f);

		// ---------- 速度補間 ----------
		CurrentSpeed += (targetSpeed - CurrentSpeed) * accel * dt;
		if(CurrentSpeed < 0.0f) CurrentSpeed = 0.0f;

		// ---------- 移動 ----------
		transform->position += moveDir * CurrentSpeed * dt;

		// ---------- 回転 ----------
		if(moveDir.length() > 0.01f){
			float yaw = atan2f(moveDir.x, moveDir.z);
			auto targetQ = DirectX::XMQuaternionRotationRollPitchYaw(0, yaw, 0);
			auto currentQ = transform->rotationVector();
			auto newQ = DirectX::XMQuaternionSlerp(currentQ, targetQ, rotateSpeed * dt);

			DirectX::XMFLOAT4 q;
			DirectX::XMStoreFloat4(&q, newQ);
			transform->SetRotation(q);
		}

		// ---------- アニメ ----------
		float speed01 = (targetSpeed > 0.01f) ? (CurrentSpeed / targetSpeed) : 0.0f;
		speed01 = std::clamp(speed01, 0.0f, 1.0f);

		model->blendedAnimations.clear();
		model->blendedAnimations.push_back({"Run",  speed01, 0.0f});
		model->blendedAnimations.push_back({"Idle", 1.0f - speed01, 0.0f});

		// ---------- ボール押し（PushBall のみ） ----------
		if(state == EnemyState::PushBall &&
		   ballController &&
		   (ballPos - enemyPos).length() < 0.6f){
			ballController->ApplyForce(goalDir * 5.0f);
		}
	}

	void OnDraw() override{}
	void OnEditorUpdate(float) override{}
	void OnStop() override{}
};
