// =======================================================================
// PlayerController.h (smooth animation transition version)
// =======================================================================
#pragma once
#include "Component/CustomScriptComponent.h"
#include "GameTimeManager.h"
#include "BallController.h"
#include "Component/TransformComponent.h"
#include "Component/modelRendererComponent.h"
#include "Component/ColliderComponent.h"
#include <System/Physic/physicSystem.h>

class PlayerController: public CustomScriptComponent {
	BEGIN_REFLECT(PlayerController)

		REFLECT_FIELD(float, moveSpeed, 6.0f)
		REFLECT_FIELD(float, dashMultiplier, 2.0f)
		REFLECT_FIELD(float, accel, 10.0f)
		REFLECT_FIELD(float, rotateSpeed, 8.0f)

		REFLECT_FIELD(float, maxStamina, 5.0f)
		REFLECT_FIELD(float, stamina, 5.0f)
		REFLECT_FIELD(float, staminaConsumeRate, 1.5f)
		REFLECT_FIELD(float, staminaRecoverRate, 1.0f)

		REFLECT_FIELD(float, jumpPower, 4.0f)

		bool canDash = true;

	float CurrentSpeed = 0.0f;
	float velY = 0.0f;

	bool isJumpPressed = false;

	uint32_t selfLayerBit = 1u << 1;

	// animation blend state
	float moveBlend = 0.0f;

	float moveBlendTime = 0.0f;
	float moveBlendPrev = 0.0f;
	float moveBlendTarget = 0.0f;
	float moveBlendDuration = 0.25f;

	float airBlend = 0.0f;
	float airBlendTime = 0.0f;
	float airBlendDuration = 0.20f;

	ComponentRef<TransformComponent> transform;
	ComponentRef<TransformComponent> cameraTransform;
	ComponentRef<GameTimeManager> gameTime;
	ComponentRef<ModelRendererComponent> model;

public:
	PlayerController(): CustomScriptComponent("PlayerController"){}

	static float EaseInOut(float t){
		return t * t * (3.0f - 2.0f * t);
	}

	static float AirEase(float t){
		return (t < 0.5f)
			? 2.0f * t * t
			: 1.0f - powf(-2.0f * t + 2.0f, 2.0f) * 0.5f;
	}

	void OnStart() override{
		transform = GetComponentRef<TransformComponent>();
		model = GetComponentRef<ModelRendererComponent>();

		auto cams = m_ref.GetScene()->component->FindEntitiesWithComponent<CameraComponent>();
		if(!cams.empty())
			cameraTransform = GetComponentRefFor<TransformComponent>(cams[0]);

		stamina = maxStamina;
	}

	void OnUpdate(float dt) override{
		if(!model || !model->model) return;
		if(!transform || !cameraTransform) return;

		// =========================
		// input
		// =========================
		Vector3 move(0, 0, 0);
		if(GetKey('W')) move.z += 1;
		if(GetKey('S')) move.z -= 1;
		if(GetKey('A')) move.x -= 1;
		if(GetKey('D')) move.x += 1;

		bool hasMove = (move.x != 0 || move.z != 0);
		bool dash = GetKey(VK_LSHIFT) && canDash;

		// =========================
		// stamina
		// =========================
		if(dash){
			stamina -= staminaConsumeRate * dt;
			if(stamina < 0){
				stamina = 0; canDash = false;
			}
		} else{
			stamina += staminaRecoverRate * dt;
			if(stamina > maxStamina){
				stamina = maxStamina; canDash = true;
			}
		}

		// =========================
		// speed smoothing
		// =========================
		float targetSpeed = hasMove
			? moveSpeed * (dash ? dashMultiplier : 1.0f)
			: 0.0f;

		CurrentSpeed += (targetSpeed - CurrentSpeed) * (std::min)(1.0f, accel * dt);

		// =========================
		// camera move dir
		// =========================
		Vector3 f = cameraTransform->front();
		Vector3 r = cameraTransform->right();
		f.y = r.y = 0;
		f = f.normalize();
		r = r.normalize();

		Vector3 dir = hasMove ? (f * move.z + r * move.x).normalize() : Vector3(0, 0, 0);

		// =========================
		// physics
		// =========================
		auto* collider = GetComponent<ColliderComponent>();
		bool grounded = false;
		bool jumping = false;

		if(collider){
			auto* rigid = collider->pRigidbodyDynamic;
			if(!rigid) return;

			auto vel = rigid->getLinearVelocity();

			physx::PxVec3 rayPos(
				transform->position.x,
				transform->position.y + 0.05f,
				transform->position.z
			);

			RayHit hit = m_ref.GetScene()->manager->systemRegistry
				->GetSystem<PhysicSystem>()
				->RaycastWithMask(rayPos, physx::PxVec3(0, -1, 0), 0.3f, selfLayerBit);

			grounded = hit.hit && hit.distance < 0.06f && velY <= 0.0f;

			physx::PxVec3 wish(dir.x, 0, dir.z);
			physx::PxVec3 hVel = wish * CurrentSpeed;

			if(grounded){
				hVel -= hit.normal * hVel.dot(hit.normal);

				if(GetKey(VK_SPACE) && !isJumpPressed){
					velY = jumpPower;
					jumping = true;

					// jump blend reset
					airBlendTime = 0.0f;
				} else{
					velY = vel.y;
				}
			} else{
				velY -= 9.8f * dt;
			}

			vel.x = hVel.x;
			vel.y = velY;
			vel.z = hVel.z;

			rigid->setLinearVelocity(vel);

			isJumpPressed = GetKey(VK_SPACE);
		}

		// =========================
		// MOVE BLEND (time-based)
		// =========================
		moveBlendTarget = hasMove ? 1.0f : 0.0f;

		if(moveBlendTarget != moveBlendPrev){
			moveBlendTime = 0.0f;
			moveBlendPrev = moveBlendTarget;
		}

		moveBlendTime += dt;
		float t = std::clamp(moveBlendTime / moveBlendDuration, 0.0f, 1.0f);
		float eased = EaseInOut(t);

		moveBlend = moveBlendPrev + (moveBlendTarget - moveBlendPrev) * eased;

		// =========================
		// AIR BLEND (jump/land smooth)
		// =========================
		float airTarget = grounded ? 0.0f : 1.0f;

		if(airTarget > airBlend){
			// rising (jump start)
			airBlendTime += dt;
		} else{
			// landing reset
			airBlendTime += dt;
		}

		float at = std::clamp(airBlendTime / airBlendDuration, 0.0f, 1.0f);
		float aEase = AirEase(at);

		airBlend = airTarget > airBlend
			? aEase
			: (1.0f - aEase);

		airBlend = std::clamp(airBlend, 0.0f, 1.0f);

		// =========================
		// animation weights
		// =========================
		if(model->blendedAnimations.size() < 4) return;

		float groundMask = 1.0f - airBlend;

		model->blendedAnimations[0].weight = moveBlend * groundMask;        // Run
		model->blendedAnimations[1].weight = (1.0f - moveBlend) * groundMask; // Idle
		model->blendedAnimations[2].weight = airBlend;                       // JumpUp/Down shared

		// =========================
		// rotation
		// =========================
		if(hasMove){
			auto targetQ = DirectX::XMQuaternionRotationRollPitchYaw(
				0, atan2f(dir.x, dir.z), 0);

			auto curQ = transform->rotationVector();

			float tRot = (std::min)(1.0f, rotateSpeed * dt);
			auto q = DirectX::XMQuaternionSlerp(curQ, targetQ, tRot);

			DirectX::XMFLOAT4 f;
			DirectX::XMStoreFloat4(&f, q);
			transform->SetRotation(f);
		}
	}

	void OnFixedUpdate(float dt) override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt) override{}
	void OnStop() override{}
};