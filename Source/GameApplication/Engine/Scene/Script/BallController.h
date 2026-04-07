// =======================================================================
// 
// BallController.h
// 
// =======================================================================
#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"
#include "Component/TransformComponent.h"
#include "Component/ColliderComponent.h"
#include "ScoreManager.h"

// ボール制御スクリプト
class BallController: public CustomScriptComponent {
public:
	BEGIN_REFLECT(BallController)

		REFLECT_FIELD(float, friction, 0.95f)			// 摩擦減衰
		REFLECT_FIELD(float, knockbackPower, 5.0f)		// 吹っ飛びの強さ

	Vector3 velocity = Vector3(0, 0, 0); // ボールの速度
	ComponentRef<TransformComponent> transform;
	ComponentRef<ColliderComponent> collider;
	ComponentRef<ScoreManager> scoreManager;

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
		transform = GetComponentRef<TransformComponent>();
		collider = GetComponentRef<ColliderComponent>();
		auto entity = m_ref.GetScene()->component->FindEntitiesWithComponent<ScoreManager>();
		if(!entity.empty()){
			scoreManager = GetComponentRefFor<ScoreManager>(entity[0]);
		}
	}

	void OnUpdate(float dt) override{

	}

	void OnFixedUpdate(float dt) override{
		physx::PxVec3 pxVelocity;
		physx::PxRigidDynamic* rigid = collider->pRigidbodyDynamic;

		if(rigid){
			physx::PxVec3 pxVelocity = rigid->getLinearVelocity();
			velocity = Vector3(pxVelocity.x, pxVelocity.y, pxVelocity.z);
		}

		// ゴール判定
		if(transform->position.x > 5.5f && abs(transform->position.z) < 0.7f){
			transform->position = Vector3(0.0f, 1.0f, 0.0f);
			velocity = Vector3(0, 0, 0);
			if(scoreManager) scoreManager->RedScore++;

			pxVelocity.x = velocity.x;
			pxVelocity.y = velocity.y;
			pxVelocity.z = velocity.z;

			rigid->setLinearVelocity(pxVelocity);
		}

		if(transform->position.x < -5.4f && abs(transform->position.z) < 0.7f){
			transform->position = Vector3(0.0f, 1.0f, 0.0f);
			velocity = Vector3(0, 0, 0);
			if(scoreManager) scoreManager->BlueScore++;

			pxVelocity.x = velocity.x;
			pxVelocity.y = velocity.y;
			pxVelocity.z = velocity.z;

			rigid->setLinearVelocity(pxVelocity);
		}

		if(transform->position.y < -10.0f){
			transform->position = Vector3(0.0f, 1.0f, 0.0f);

		}
	}
	void OnDraw() override{}
	void OnEditorUpdate(float dt) override{}
	void OnStop() override{}

	// --- エネミーやプレイヤーから呼ばれる: ボールに力を加える ---
	void ApplyForce(const Vector3& dir){
		physx::PxRigidDynamic* rigid = collider->pRigidbodyDynamic;

		physx::PxVec3 pxVelocity;

		velocity += dir.normalize() * knockbackPower;

		pxVelocity.x = velocity.x;
		pxVelocity.y = velocity.y;
		pxVelocity.z = velocity.z;

		rigid->setLinearVelocity(pxVelocity);

	}

private:
};

