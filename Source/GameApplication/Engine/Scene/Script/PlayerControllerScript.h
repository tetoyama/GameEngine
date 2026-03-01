#pragma once
#include "Component/CustomScriptComponent.h"
#include "Component/TransformComponent.h"
#include "Component/modelRendererComponent.h"
#include "Component/ColliderComponent.h"
#include <System/Physic/physicSystem.h>

// ============================================================
// PlayerControllerScript
// CharacterController を実装したプレイヤースクリプト
//   - WASD 移動
//   - Space ジャンプ
//   - Ctrl スタミナ無し即時ダッシュ
//   - スキニングアニメーション (Run / Idle / JumpingUp / JumpingDown)
//   - 坂道に強いレイキャスト判定＋法線ベクトル補正
// ============================================================
class PlayerControllerScript : public CustomScriptComponent {

	BEGIN_REFLECT(PlayerControllerScript)

		REFLECT_FIELD(float, moveSpeed,      5.0f)
		REFLECT_FIELD(float, dashMultiplier, 2.0f)
		REFLECT_FIELD(float, accel,          10.0f)
		REFLECT_FIELD(float, rotateSpeed,    8.0f)
		REFLECT_FIELD(float, jumpPower,      5.0f)
		REFLECT_FIELD(float, groundRayLen,    0.2f)  // 地面判定レイ長さ
		REFLECT_FIELD(float, gravity,         9.0f)  // 重力加速度
		REFLECT_FIELD(int,   groundLayerMask, -1)    // 地面レイヤーマスク (-1 = 全レイヤー)

	float currentSpeed  = 0.0f;
	float velY          = 0.0f;
	bool  isJumpPressed = false;

	ComponentRef<TransformComponent>     transform;
	ComponentRef<TransformComponent>     cameraTransform;
	ComponentRef<ModelRendererComponent> model;

public:
	PlayerControllerScript() : CustomScriptComponent("PlayerControllerScript") {}

	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		DECODE_FIELDS(node);
		return true;
	}

	void inspector(SceneContext* context) override {
		ImGui::Text(scriptName.c_str());
		INSPECTOR_FIELDS();
	}

	void OnStart() override {
		velY          = 0.0f;
		currentSpeed  = 0.0f;
		isJumpPressed = false;

		transform = GetComponentRef<TransformComponent>();
		model     = GetComponentRef<ModelRendererComponent>();

		auto camEntities = m_ref.GetScene()->component->FindEntitiesWithComponent<CameraComponent>();
		if (!camEntities.empty()) {
			cameraTransform = GetComponentRefFor<TransformComponent>(camEntities[0]);
		}
	}

	void OnFixedUpdate(float dt) override {
		if (!transform) return;

		// ---- アニメーションセットアップ（4種揃うまで毎フレーム確認）----
		if (model && model->model && model->blendedAnimations.size() < 4) {
			model->blendedAnimations.clear();
			auto& anims = model->model->m_Animation;
			if (anims.find("Run")        == anims.end()) model->model->LoadAnimation("Asset/Model/Akai_Run.fbx",       "Run");
			if (anims.find("Idle")       == anims.end()) model->model->LoadAnimation("Asset/Model/Akai_Idle.fbx",      "Idle");
			if (anims.find("JumpingUp")  == anims.end()) model->model->LoadAnimation("Asset/Model/Jumping Up.fbx",    "JumpingUp");
			if (anims.find("JumpingDown")== anims.end()) model->model->LoadAnimation("Asset/Model/Jumping Down.fbx",  "JumpingDown");
			model->blendedAnimations.push_back({"Run",         0.0f, 0.0f});
			model->blendedAnimations.push_back({"Idle",        1.0f, 0.0f});
			model->blendedAnimations.push_back({"JumpingUp",   0.0f, 0.0f});
			model->blendedAnimations.push_back({"JumpingDown", 0.0f, 0.0f});
			model->blendedAnimations[0].isLoop = true;
			model->blendedAnimations[1].isLoop = true;
			model->blendedAnimations[2].isLoop = false;
			model->blendedAnimations[3].isLoop = false;
		}

		// ---- 入力 ----
		Vector3 moveInput(0, 0, 0);
		if (GetKey('W')) moveInput.z += 1.0f;
		if (GetKey('S')) moveInput.z -= 1.0f;
		if (GetKey('A')) moveInput.x -= 1.0f;
		if (GetKey('D')) moveInput.x += 1.0f;

		// Ctrl キー = スタミナ消費なしダッシュ
		bool isDashing = GetKey(VK_LCONTROL) || GetKey(VK_RCONTROL);

		// ---- カメラ基準の移動方向計算 ----
		Vector3 dir(0, 0, 0);
		if (moveInput.x != 0.0f || moveInput.z != 0.0f) {
			if (cameraTransform) {
				Vector3 camF = cameraTransform->front(); camF.y = 0.0f; camF = camF.normalize();
				Vector3 camR = cameraTransform->right(); camR.y = 0.0f; camR = camR.normalize();
				dir = (camF * moveInput.z + camR * moveInput.x).normalize();
			} else {
				dir = moveInput.normalize();
			}
		}

		// ---- 速度更新 ----
		float targetSpeed = isDashing ? moveSpeed * dashMultiplier : moveSpeed;
		if (moveInput.x != 0.0f || moveInput.z != 0.0f) {
			currentSpeed += accel * dt;
			if (currentSpeed > targetSpeed) currentSpeed = targetSpeed;
		} else {
			currentSpeed -= moveSpeed * dt;
			if (currentSpeed < 0.0f) currentSpeed = 0.0f;
		}

		ColliderComponent* collider = GetComponent<ColliderComponent>();
		bool isGround  = false;
		bool isJumping = false;

		if (!collider || !collider->pRigidbodyDynamic) {
			// コライダーなし：トランスフォームを直接移動
			transform->position += dir * (currentSpeed * dt);
		} else {
			physx::PxRigidDynamic* rigid = collider->pRigidbodyDynamic;
			physx::PxVec3 velocity       = rigid->getLinearVelocity();

			// 地面レイキャスト（原点をわずかに上げて HeightField も正確に検出）
			physx::PxVec3 rayOrigin(
				transform->position.x,
				transform->position.y + 0.05f,
				transform->position.z);
			physx::PxVec3 rayDown(0.0f, -1.0f, 0.0f);
			physx::PxU32  mask = static_cast<physx::PxU32>(groundLayerMask);

			RayHit hit = m_ref.GetScene()->manager
				->systemRegistry
				->GetSystem<PhysicSystem>()
				->RaycastWithMask(rayOrigin, rayDown, groundRayLen + 0.15f, mask);

			isGround = hit.hit && hit.distance <= (groundRayLen + 0.05f);

			// 水平速度ベクトル（坂道補正前）
			physx::PxVec3 wishDir(dir.x, 0.0f, dir.z);
			physx::PxVec3 hVel = wishDir * currentSpeed;

			if (isGround) {
				// 坂道補正：法線に垂直な速度成分を除去してサーフェスに沿って移動
				physx::PxVec3 n = hit.normal;
				hVel -= n * hVel.dot(n);

				if (GetKey(VK_SPACE) && !isJumpPressed) {
					velY = jumpPower;
					transform->position.y += 0.1f;
				} else {
					velY = 0.0f;
				}
			} else {
				velY -= gravity * dt;
			}

			velocity.x = hVel.x;
			velocity.y = velY;
			velocity.z = hVel.z;
			rigid->setLinearVelocity(velocity);

			isJumping     = velocity.y > 0.0f;
			isJumpPressed = GetKey(VK_SPACE);
		}

		// ---- 回転補間（移動方向を向く）----
		if (dir.x != 0.0f || dir.z != 0.0f) {
			DirectX::XMVECTOR targetQ = DirectX::XMQuaternionRotationRollPitchYaw(
				0.0f, atan2f(dir.x, dir.z), 0.0f);
			DirectX::XMVECTOR currentQ = transform->rotationVector();
			DirectX::XMVECTOR newQ     = DirectX::XMQuaternionSlerp(currentQ, targetQ, rotateSpeed * dt);
			DirectX::XMFLOAT4 q;
			DirectX::XMStoreFloat4(&q, newQ);
			transform->SetRotation(q);
		}

		// ---- アニメーションブレンド ----
		if (!model || model->blendedAnimations.size() < 4) return;

		float maxSpeed    = moveSpeed * dashMultiplier;
		float runWeight   = (maxSpeed > 0.0f) ? currentSpeed / maxSpeed : 0.0f;
		float idleWeight  = 1.0f - runWeight;
		float jumpUpW     = model->blendedAnimations[2].weight;
		float jumpDownW   = model->blendedAnimations[3].weight;

		if (!isGround) {
			if (isJumping) {
				if (jumpUpW <= 0.0f)
					model->blendedAnimations[2].animationStartTime = -model->animationTime + 25.0f;
				model->blendedAnimations[2].weight = std::min(jumpUpW + 0.15f, 1.0f);
				model->blendedAnimations[3].weight = 0.0f;
			} else {
				if (jumpDownW <= 0.0f)
					model->blendedAnimations[3].animationStartTime = -model->animationTime;
				model->blendedAnimations[2].weight = std::max(jumpUpW  - 0.01f,  0.0f);
				model->blendedAnimations[3].weight = std::min(
					jumpDownW + 0.15f, 1.0f - model->blendedAnimations[2].weight);
			}
			float airW = model->blendedAnimations[2].weight + model->blendedAnimations[3].weight;
			model->blendedAnimations[0].weight = runWeight  * (1.0f - airW);
			model->blendedAnimations[1].weight = idleWeight * (1.0f - airW);
		} else {
			model->blendedAnimations[2].weight = std::max(jumpUpW   - 0.125f, 0.0f);
			model->blendedAnimations[3].weight = std::max(jumpDownW - 0.2f,   0.0f);
			float airW = model->blendedAnimations[2].weight + model->blendedAnimations[3].weight;
			model->blendedAnimations[0].weight = runWeight  * (1.0f - airW);
			model->blendedAnimations[1].weight = idleWeight * (1.0f - airW);
		}
	}

	void OnDraw() override {}
	void OnEditorUpdate(float dt) override {}
	void OnStop() override {}

private:
};
