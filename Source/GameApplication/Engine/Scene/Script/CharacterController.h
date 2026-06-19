// =======================================================================
// 
// CharacterController.h
// 
// =======================================================================
#pragma once
#include "Component/CustomScriptComponent.h"
#include "Component/TransformComponent.h"
#include "Component/modelRendererComponent.h"
#include "Component/ColliderComponent.h"
#include "Component/CameraComponent.h"
#include <System/Physic/physicSystem.h>
#include <cmath>

// ============================================================
// CharacterController
//
// WASD移動・スペースジャンプ・LSHIFTスタミナ無しダッシュ・
// モーションブレンドスキニングアニメーション・坂道対応
// ============================================================
// キャラクター制御スクリプト
class CharacterController : public CustomScriptComponent {
	BEGIN_REFLECT(CharacterController)

		// 移動パラメータ
		REFLECT_FIELD(float, moveSpeed, 5.0f)
		REFLECT_FIELD(float, dashMultiplier, 2.5f)
		REFLECT_FIELD(float, jumpPower, 5.0f)
		REFLECT_FIELD(float, accel, 12.0f)
		REFLECT_FIELD(float, rotateSpeed, 8.0f)

		// 接地・坂道判定
		REFLECT_FIELD(float, groundRayLength, 0.3f)
		REFLECT_FIELD(float, slopeLimit, 45.0f)    // 歩行可能な最大斜面角度（度）
		REFLECT_FIELD(float, gravity, 9.0f)         // 重力加速度

		// アニメーションファイルパス
		REFLECT_FIELD(std::string, idleAnimFile,     std::string("Asset/Model/Akai_Idle.fbx"))
		REFLECT_FIELD(std::string, runAnimFile,      std::string("Asset/Model/Akai_Run.fbx"))
		REFLECT_FIELD(std::string, jumpUpAnimFile,   std::string("Asset/Model/Jumping Up.fbx"))
		REFLECT_FIELD(std::string, jumpDownAnimFile, std::string("Asset/Model/Jumping Down.fbx"))

	// ランタイム状態（非シリアライズ）
	float  currentSpeed   = 0.0f;
	float  velY           = 0.0f;
	bool   isJumpPressed  = false;
	bool   isGrounded     = false;
	bool   isInJump       = false;   // ジャンプ中フラグ（坂道登りの上向き速度と区別するため）
	bool   animsReady     = false;

	// RayCast 時に除外する自身のレイヤービット（インスペクターで設定）
	// デフォルトはデフォルトのプレイヤーレイヤー (1u << 1)
	uint32_t selfLayerBit = 1u << 1;

	ComponentRef<TransformComponent>    transform;
	ComponentRef<ModelRendererComponent> model;
	ComponentRef<TransformComponent>    cameraTransform;

public:
	CharacterController() : CustomScriptComponent("CharacterController") {}

	// =====================================================
	// YAML Encode / Decode
	// =====================================================
	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		node["SelfLayerBit"] = selfLayerBit;
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		DECODE_FIELDS(node);
		if(node["SelfLayerBit"]) selfLayerBit = node["SelfLayerBit"].as<uint32_t>();
		return true;
	}

	// =====================================================
	// ImGui Inspector
	// =====================================================
	void inspector(SceneContext* context) override {
		ImGui::Text(scriptName.c_str());
		INSPECTOR_FIELDS();

		// 自身のレイヤー選択（RayCast 除外用）
		auto* phys = context->system->GetSystem<PhysicSystem>();
		if (phys) {
			const auto& layers = phys->GetLayers();
			const char* previewLabel = "(none)";
			for (const auto& layer : layers) {
				if (selfLayerBit == layer.bit) { previewLabel = layer.name.c_str(); break; }
			}
			ImGui::Text("Self Layer (RayCast exclude)");
			if (ImGui::BeginCombo("##SelfLayer", previewLabel)) {
				if (ImGui::Selectable("(none)", selfLayerBit == 0)) selfLayerBit = 0;
				if (selfLayerBit == 0) ImGui::SetItemDefaultFocus();
				for (const auto& layer : layers) {
					bool sel = (selfLayerBit == layer.bit);
					if (ImGui::Selectable(layer.name.c_str(), sel)) selfLayerBit = layer.bit;
					if (sel) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}
	}

	// =====================================================
	// ライフサイクル
	// =====================================================
	void OnStart() override {
		velY          = 0.0f;
		currentSpeed  = 0.0f;
		isJumpPressed = false;
		isGrounded    = false;
		isInJump      = false;
		animsReady    = false;

		transform = GetComponentRef<TransformComponent>();
		model     = GetComponentRef<ModelRendererComponent>();

		// シーン内のカメラを探して方向基準に使う
		auto cameraEntities = m_ref.GetScene()->component->FindEntitiesWithComponent<CameraComponent>();
		if (!cameraEntities.empty()) {
			cameraTransform = GetComponentRefFor<TransformComponent>(cameraEntities[0]);
		}
	}

	void OnFixedUpdate(float dt) override {
		if (!model || !model->model) return;

		// アニメーション初期化（モデルロード後に一度だけ実行）
		if (!animsReady) {
			InitAnimations();
		}

		if (!transform) return;

		// --------------------------------------------------
		// 入力
		// --------------------------------------------------
		Vector3 moveInput(0, 0, 0);
		if (GetKey('W')) moveInput.z += 1.0f;
		if (GetKey('S')) moveInput.z -= 1.0f;
		if (GetKey('A')) moveInput.x -= 1.0f;
		if (GetKey('D')) moveInput.x += 1.0f;

		// LSHIFT でスタミナ無しダッシュ
		bool isDashing = GetKey(VK_LSHIFT);

		// --------------------------------------------------
		// 移動方向（カメラ基準 / 無ければキャラクター基準）
		// --------------------------------------------------
		Vector3 dir(0, 0, 0);
		if (cameraTransform) {
			Vector3 camForward = cameraTransform->front();
			Vector3 camRight   = cameraTransform->right();
			camForward.y = 0; camRight.y = 0;
			camForward = camForward.normalize();
			camRight   = camRight.normalize();
			dir = camForward * moveInput.z + camRight * moveInput.x;
		} else {
			Vector3 charForward = transform->front();
			Vector3 charRight   = transform->right();
			charForward.y = 0; charRight.y = 0;
			charForward = charForward.normalize();
			charRight   = charRight.normalize();
			dir = charForward * moveInput.z + charRight * moveInput.x;
		}

		bool hasInput = (dir.x != 0.0f || dir.z != 0.0f);
		if (hasInput) dir = dir.normalize();

		// --------------------------------------------------
		// 物理移動
		// --------------------------------------------------
		ColliderComponent* collider = GetComponent<ColliderComponent>();
		bool isJumping = false;

		if (!collider) {
			// コライダー無し：単純位置移動
			float targetSpeed = isDashing ? moveSpeed * dashMultiplier : moveSpeed;
			currentSpeed = targetSpeed;
			transform->position += dir * (currentSpeed * dt);
		} else {
			physx::PxRigidDynamic* rigid = collider->pRigidbodyDynamic;
			if (!rigid) return;

			physx::PxVec3 velocity = rigid->getLinearVelocity();

			// ------------------------------------------------
			// 足元レイキャストで接地・坂道判定
			// ------------------------------------------------
			physx::PxVec3 rayPos(
				transform->position.x,
				transform->position.y + 0.1f,   // 若干上から発射して HeightField でも確実にヒット
				transform->position.z
			);
			physx::PxVec3 rayDir(0.0f, -1.0f, 0.0f);

			// 全レイヤーを対象に、自身のレイヤーだけ除外してレイキャスト
			RayHit hit = m_ref.GetScene()->manager
				->systemRegistry
				->GetSystem<PhysicSystem>()
				->RaycastWithMask(rayPos, rayDir, groundRayLength + 0.1f, selfLayerBit);

			isGrounded = hit.hit && (hit.distance < (groundRayLength + 0.05f)) && !isInJump;

			// 坂道角度チェック
			bool isWalkable = true;
			physx::PxVec3 surfaceNormal(0.0f, 1.0f, 0.0f);
			if (isGrounded) {
				surfaceNormal = hit.normal;
				float cosAngle    = surfaceNormal.dot(physx::PxVec3(0.0f, 1.0f, 0.0f));
				float slopeAngle = acosf((std::max)(-1.0f, (std::min)(1.0f, cosAngle)))
				                    * (180.0f / 3.14159265f);
				isWalkable = (slopeAngle <= slopeLimit);
			}

			// 水平速度計算
			physx::PxVec3 wishDir(dir.x, 0.0f, dir.z);
			physx::PxVec3 horizontalVel = wishDir * currentSpeed;

			if (isGrounded && isWalkable) {
				// 坂面に沿った移動（法線成分を除去）
				horizontalVel -= surfaceNormal * horizontalVel.dot(surfaceNormal);

				// ジャンプ（スペースキー：押した瞬間のみ）
				if (GetKey(VK_SPACE) && !isJumpPressed) {
					velY = jumpPower;
					transform->position.y += 0.05f;
					isInJump = true;
				} else {
					// 着地確認：ジャンプフラグを確実にリセット
					isInJump = false;
					// 坂面に沿った Y 速度を使い、坂道では接地スナップを加えて
					// 上り坂・下り坂どちらも滑らかに追従する。
					// 平坦面（surfaceNormal.y ≒ 1）ではスナップ不要なのでゼロにする。
					float slopeSnap = (surfaceNormal.y < 0.999f) ? -gravity * dt : 0.0f;
					velY = horizontalVel.y + slopeSnap;
				}
			} else {
				// 空中 or 急斜面：重力適用
				if (!isWalkable && isGrounded) {
					// 急斜面では横移動を無効にして滑落を防ぐ
					horizontalVel = physx::PxVec3(0.0f, 0.0f, 0.0f);
				}
				velY -= gravity * dt;
				// 頂点を過ぎて下降に転じたらジャンプフラグを解除
				if (velY <= 0.0f) isInJump = false;
			}

			// 最終速度合成
			velocity.x = horizontalVel.x;
			velocity.y = velY;
			velocity.z = horizontalVel.z;
			rigid->setLinearVelocity(velocity);

			isJumping     = (velocity.y > 0.0f);
			isJumpPressed = GetKey(VK_SPACE);
		}

		// --------------------------------------------------
		// 速度更新
		// --------------------------------------------------
		float targetSpeed = isDashing ? moveSpeed * dashMultiplier : moveSpeed;
		if (hasInput) {
			currentSpeed += accel * dt;
			if (currentSpeed > targetSpeed) currentSpeed = targetSpeed;
		} else {
			currentSpeed -= accel * dt;
			if (currentSpeed < 0.0f) currentSpeed = 0.0f;
		}

		// --------------------------------------------------
		// キャラクター回転（移動方向へ補間）
		// --------------------------------------------------
		if (hasInput) {
			DirectX::XMVECTOR targetQ = DirectX::XMQuaternionRotationRollPitchYaw(
				0.0f, atan2f(dir.x, dir.z), 0.0f
			);
			DirectX::XMVECTOR currentQ = transform->rotationVector();
			DirectX::XMVECTOR newQ     = DirectX::XMQuaternionSlerp(currentQ, targetQ, rotateSpeed * dt);
			DirectX::XMFLOAT4 fq;
			DirectX::XMStoreFloat4(&fq, newQ);
			transform->SetRotation(fq);
		}

		// --------------------------------------------------
		// アニメーションブレンド更新
		// --------------------------------------------------
		UpdateAnimations(isJumping, isGrounded);
	}

	void OnStop() override {}

private:

	// =====================================================
	// アニメーションの初期化（OnStart 後に一度だけ呼ぶ）
	// =====================================================
	void InitAnimations() {
		if (!model->model) return;

		auto& anim = model->model->m_Animation;

		if (anim.find("Idle")        == anim.end()) model->model->LoadAnimation(idleAnimFile.c_str(),     "Idle");
		if (anim.find("Run")         == anim.end()) model->model->LoadAnimation(runAnimFile.c_str(),      "Run");
		if (anim.find("JumpingUp")   == anim.end()) model->model->LoadAnimation(jumpUpAnimFile.c_str(),   "JumpingUp");
		if (anim.find("JumpingDown") == anim.end()) model->model->LoadAnimation(jumpDownAnimFile.c_str(), "JumpingDown");

		model->blendedAnimations.clear();
		model->blendedAnimations.push_back({"Run",         0.0f, 0.0f, true});   // [0] Run
		model->blendedAnimations.push_back({"Idle",        1.0f, 0.0f, true});   // [1] Idle
		model->blendedAnimations.push_back({"JumpingUp",   0.0f, 0.0f, false});  // [2] JumpUp
		model->blendedAnimations.push_back({"JumpingDown", 0.0f, 0.0f, false});  // [3] JumpDown

		animsReady = true;
	}

	// =====================================================
	// アニメーションブレンドウェイト更新
	// =====================================================
	void UpdateAnimations(bool isJumping, bool grounded) {
		if (model->blendedAnimations.size() < 4) return;

		float maxSpeed  = moveSpeed * dashMultiplier;
		float runWeight = (maxSpeed > 0.0f) ? (currentSpeed / maxSpeed) : 0.0f;
		runWeight = (std::max)(0.0f, (std::min)(1.0f, runWeight));
		float idleWeight = 1.0f - runWeight;

		if (grounded) {
			// 着地：ジャンプアニメーションをフェードアウト
			model->blendedAnimations[2].weight -= 0.1f;
			if (model->blendedAnimations[2].weight < 0.0f) model->blendedAnimations[2].weight = 0.0f;

			model->blendedAnimations[3].weight -= 0.1f;
			if (model->blendedAnimations[3].weight < 0.0f) model->blendedAnimations[3].weight = 0.0f;

			float groundBlend = 1.0f - (model->blendedAnimations[2].weight + model->blendedAnimations[3].weight);
			model->blendedAnimations[0].weight = runWeight  * groundBlend;
			model->blendedAnimations[1].weight = idleWeight * groundBlend;
		} else {
			// 空中
			if (isJumping) {
				// 上昇中：JumpingUp をフェードイン
				// animationStartTime のオフセット 25.0f はジャンプ開始フレームを
				// アニメーション開始付近に合わせるための調整値
				if (model->blendedAnimations[2].weight <= 0.0f)
					model->blendedAnimations[2].animationStartTime = -model->animationTime + 25.0f;

				model->blendedAnimations[2].weight += 0.05f;
				if (model->blendedAnimations[2].weight > 1.0f) model->blendedAnimations[2].weight = 1.0f;
				model->blendedAnimations[3].weight = 0.0f;
			} else {
				// 下降中：JumpingDown をフェードイン
				if (model->blendedAnimations[3].weight <= 0.0f)
					model->blendedAnimations[3].animationStartTime = -model->animationTime;

				model->blendedAnimations[2].weight -= 0.01f;
				if (model->blendedAnimations[2].weight < 0.0f) model->blendedAnimations[2].weight = 0.0f;

				model->blendedAnimations[3].weight += 0.05f;
				float totalJump = model->blendedAnimations[2].weight + model->blendedAnimations[3].weight;
				if (totalJump > 1.0f)
					model->blendedAnimations[3].weight = 1.0f - model->blendedAnimations[2].weight;
			}

			float totalJump   = model->blendedAnimations[2].weight + model->blendedAnimations[3].weight;
			float groundBlend = 1.0f - totalJump;
			model->blendedAnimations[0].weight = runWeight  * groundBlend;
			model->blendedAnimations[1].weight = idleWeight * groundBlend;
		}
	}
};
