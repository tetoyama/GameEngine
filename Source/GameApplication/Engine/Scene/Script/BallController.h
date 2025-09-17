#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"
#pragma once
#include "Component/CustomScriptComponent.h"
#include "Component/TransformComponent.h"
#include "ScoreManager.h"

class BallController: public CustomScriptComponent {
public:
	BEGIN_REFLECT(BallController)

		REFLECT_FIELD(float, friction, 0.95f)              // 摩擦減衰
		REFLECT_FIELD(float, knockbackPower, 5.0f)         // 吹っ飛びの強さ
		Vector3 velocity = Vector3(0, 0, 0); // ボールの速度
		TransformComponent* transform = nullptr;
	ScoreManager* scoreManager = nullptr;

	BallController(): CustomScriptComponent("BallController"){}

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
		auto entity = m_context->component->FindEntitiesWithComponent<ScoreManager>();
		if(!entity.empty()){
			scoreManager = m_context->component->GetComponent<ScoreManager>(entity[0]);
		}
	}

	void OnUpdate(float dt) override{
		// --- 移動 ---
		transform->position += velocity * dt;

		// 摩擦による減速
		velocity *= powf(friction, dt * 60.0f); // フレームレート補正付き

		// ゴール判定
		if(transform->position.x > 5.5f && abs(transform->position.z) < 0.7f){
			transform->position = Vector3(0.0f, 1.0f, 0.0f);
			velocity = Vector3(0, 0, 0);
			if(scoreManager) scoreManager->RedScore++;
		}

		if(transform->position.x < -5.4f && abs(transform->position.z) < 0.7f){
			transform->position = Vector3(0.0f, 1.0f, 0.0f);
			velocity = Vector3(0, 0, 0);
			if(scoreManager) scoreManager->BlueScore++;
		}

		if(transform->position.y < -10.0f){
			transform->position = Vector3(0.0f, 1.0f, 0.0f);

		}
	}

	void OnFixedUpdate(float dt) override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt) override{}
	void OnStop() override{}

	// --- エネミーやプレイヤーから呼ばれる: ボールに力を加える ---
	void ApplyForce(const Vector3& dir){
		velocity += dir.normalize() * knockbackPower;
	}

private:
};

