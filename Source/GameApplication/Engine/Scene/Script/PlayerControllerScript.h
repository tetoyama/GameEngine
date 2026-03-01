#pragma once
#include "Component/CustomScriptComponent.h"
#include "Component/transformComponent.h"
#include "Component/modelRendererComponent.h"
#include "Component/CharacterControllerComponent.h"
#include "Component/CameraComponent.h"
#include <DirectXMath.h>
#include <cmath>

// =============================================================================
// PlayerControllerScript
//
// 機能:
//   - WASD 移動
//   - Space でジャンプ
//   - Ctrl でスタミナ無しダッシュ
//   - スキニングアニメーション (Idle / Run / JumpingUp / JumpingDown)
//   - CharacterControllerComponent（PhysX CCT）による坂道に強い判定
// =============================================================================
class PlayerControllerScript : public CustomScriptComponent {
	BEGIN_REFLECT(PlayerControllerScript)

		REFLECT_FIELD(float, moveSpeed,   5.0f)
		REFLECT_FIELD(float, dashSpeed,  10.0f)
		REFLECT_FIELD(float, rotateSpeed, 8.0f)
		REFLECT_FIELD(float, accel,      12.0f)

	bool m_animsReady   = false;
	float m_currentSpeed = 0.0f;

	CharacterControllerComponent* m_cct             = nullptr;
	ModelRendererComponent*        m_model           = nullptr;
	TransformComponent*            m_transform       = nullptr;
	TransformComponent*            m_cameraTransform = nullptr;

	static constexpr int ANIM_RUN  = 0;
	static constexpr int ANIM_IDLE = 1;
	static constexpr int ANIM_UP   = 2;
	static constexpr int ANIM_DOWN = 3;
	// ジャンプ上昇アニメーションの開始を現在の再生位置に合わせるためのオフセット
	static constexpr float JUMP_UP_ANIM_OFFSET = 25.0f;

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
		if (m_cct) {
			ImGui::Separator();
			ImGui::Text("isGrounded: %s",       m_cct->isGrounded ? "true" : "false");
			ImGui::Text("verticalVelocity: %.2f", m_cct->verticalVelocity);
			ImGui::Text("currentSpeed: %.2f",    m_currentSpeed);
		}
	}

	void OnStart() override {
		m_cct       = GetComponent<CharacterControllerComponent>();
		m_model     = GetComponent<ModelRendererComponent>();
		m_transform = GetComponent<TransformComponent>();

		// カメラを探す
		auto camEntities = m_context->component->FindEntitiesWithComponent<CameraComponent>();
		if (!camEntities.empty()) {
			m_cameraTransform = m_context->component->GetComponent<TransformComponent>(camEntities[0]);
		}

		m_currentSpeed = 0.0f;
		m_animsReady   = false;
	}

	void OnFixedUpdate(float dt) override {
		if (!m_cct || !m_transform) return;

		// ------------------------------------------------------------------
		// 1) キー入力取得
		// ------------------------------------------------------------------
		float moveX = 0.0f, moveZ = 0.0f;
		if (GetKey('W')) moveZ += 1.0f;
		if (GetKey('S')) moveZ -= 1.0f;
		if (GetKey('A')) moveX -= 1.0f;
		if (GetKey('D')) moveX += 1.0f;

		bool isDash  = GetKey(VK_LCONTROL) || GetKey(VK_RCONTROL);
		bool jumpKey = GetKey(VK_SPACE);

		bool isMoving = (moveX != 0.0f || moveZ != 0.0f);

		// ------------------------------------------------------------------
		// 2) 移動方向（カメラ基準 or キャラクター前方基準）
		// ------------------------------------------------------------------
		float dirX = 0.0f, dirZ = 0.0f;
		if (isMoving) {
			float fwdX, fwdZ, rgtX, rgtZ;

			if (m_cameraTransform) {
				Vector3 camFwd = m_cameraTransform->front();
				camFwd.y = 0.0f;
				float len = sqrtf(camFwd.x * camFwd.x + camFwd.z * camFwd.z);
				if (len > 1e-5f) { camFwd.x /= len; camFwd.z /= len; }
				fwdX = camFwd.x; fwdZ = camFwd.z;
				// カメラ右ベクトル
				Vector3 camRight = m_cameraTransform->right();
				camRight.y = 0.0f;
				float rlen = sqrtf(camRight.x * camRight.x + camRight.z * camRight.z);
				if (rlen > 1e-5f) { camRight.x /= rlen; camRight.z /= rlen; }
				rgtX = camRight.x; rgtZ = camRight.z;
			} else {
				Vector3 fwd = m_transform->front();
				fwd.y = 0.0f;
				float len = sqrtf(fwd.x * fwd.x + fwd.z * fwd.z);
				if (len > 1e-5f) { fwd.x /= len; fwd.z /= len; }
				fwdX = fwd.x; fwdZ = fwd.z;
				Vector3 right = m_transform->right();
				right.y = 0.0f;
				float rlen = sqrtf(right.x * right.x + right.z * right.z);
				if (rlen > 1e-5f) { right.x /= rlen; right.z /= rlen; }
				rgtX = right.x; rgtZ = right.z;
			}

			dirX = rgtX * moveX + fwdX * moveZ;
			dirZ = rgtZ * moveX + fwdZ * moveZ;
			float dlen = sqrtf(dirX * dirX + dirZ * dirZ);
			if (dlen > 1e-5f) { dirX /= dlen; dirZ /= dlen; }
		}

		// ------------------------------------------------------------------
		// 3) 速度補間（加速・ダッシュ）
		// ------------------------------------------------------------------
		float targetSpeed = isMoving ? (isDash ? dashSpeed : moveSpeed) : 0.0f;

		if (m_currentSpeed < targetSpeed) {
			m_currentSpeed += accel * dt;
			if (m_currentSpeed > targetSpeed) m_currentSpeed = targetSpeed;
		} else {
			m_currentSpeed -= accel * dt;
			if (m_currentSpeed < 0.0f) m_currentSpeed = 0.0f;
		}

		// ------------------------------------------------------------------
		// 4) CharacterControllerComponent に速度を渡す
		// ------------------------------------------------------------------
		m_cct->inputVelocityX = dirX * m_currentSpeed;
		m_cct->inputVelocityZ = dirZ * m_currentSpeed;
		m_cct->jumpPressed    = jumpKey;

		// ------------------------------------------------------------------
		// 5) キャラクター回転補間（移動方向に向ける）
		// ------------------------------------------------------------------
		if (isMoving && m_currentSpeed > 0.1f) {
			float targetYaw = atan2f(dirX, dirZ);
			DirectX::XMVECTOR targetQ  = DirectX::XMQuaternionRotationRollPitchYaw(0.0f, targetYaw, 0.0f);
			DirectX::XMVECTOR currentQ = m_transform->rotationVector();
			DirectX::XMVECTOR newQ     = DirectX::XMQuaternionSlerp(currentQ, targetQ, rotateSpeed * dt);
			DirectX::XMFLOAT4 floatQ;
			DirectX::XMStoreFloat4(&floatQ, newQ);
			m_transform->SetRotation(floatQ);
		}

		// ------------------------------------------------------------------
		// 6) アニメーション更新
		// ------------------------------------------------------------------
		bool isJumping = m_cct->verticalVelocity > 0.0f;
		UpdateAnimations(isMoving, m_cct->isGrounded, isJumping, dt);
	}

	void OnDraw()           override {}
	void OnEditorUpdate(float) override {}
	void OnStop()           override {
		m_cct             = nullptr;
		m_model           = nullptr;
		m_transform       = nullptr;
		m_cameraTransform = nullptr;
		m_animsReady      = false;
		m_currentSpeed    = 0.0f;
	}

private:
	// -----------------------------------------------------------------------
	// EnsureAnimations – 必要なアニメーションを読み込みブレンドリストを初期化
	// -----------------------------------------------------------------------
	void EnsureAnimations() {
		if (!m_model || !m_model->model) return;
		auto& anims = m_model->model->m_Animation;

		if (anims.find("Run") == anims.end())
			m_model->model->LoadAnimation("Asset/Model/Akai_Run.fbx",    "Run");
		if (anims.find("Idle") == anims.end())
			m_model->model->LoadAnimation("Asset/Model/Akai_Idle.fbx",   "Idle");
		if (anims.find("JumpingUp") == anims.end())
			m_model->model->LoadAnimation("Asset/Model/Jumping Up.fbx",  "JumpingUp");
		if (anims.find("JumpingDown") == anims.end())
			m_model->model->LoadAnimation("Asset/Model/Jumping Down.fbx","JumpingDown");

		if (m_model->blendedAnimations.size() < 4) {
			m_model->blendedAnimations.clear();
			m_model->blendedAnimations.push_back({"Run",         0.0f, 0.0f, true });
			m_model->blendedAnimations.push_back({"Idle",        1.0f, 0.0f, true });
			m_model->blendedAnimations.push_back({"JumpingUp",   0.0f, 0.0f, false});
			m_model->blendedAnimations.push_back({"JumpingDown", 0.0f, 0.0f, false});
		}
		m_animsReady = true;
	}

	// -----------------------------------------------------------------------
	// UpdateAnimations – アニメーションブレンド重みを更新
	// -----------------------------------------------------------------------
	void UpdateAnimations(bool isMoving, bool isGrounded, bool isJumping, float /*dt*/) {
		if (!m_model || !m_model->model) return;
		if (!m_animsReady) {
			EnsureAnimations();
			if (!m_animsReady) return;
		}

		auto& ba = m_model->blendedAnimations;
		if (ba.size() < 4) return;

		float upWeight   = ba[ANIM_UP  ].weight;
		float downWeight = ba[ANIM_DOWN].weight;
		float runWeight  = 0.0f;
		float idleWeight = 0.0f;

		if (isGrounded) {
			// 着地: ジャンプ重みをフェードアウト
			upWeight   -= 0.125f; if (upWeight   < 0.0f) upWeight   = 0.0f;
			downWeight -= 0.2f;   if (downWeight < 0.0f) downWeight = 0.0f;

			float groundBlend = 1.0f - (upWeight + downWeight);
			float moveRatio   = (dashSpeed > 0.0f)
				? (m_currentSpeed / dashSpeed) : 0.0f;
			if (moveRatio > 1.0f) moveRatio = 1.0f;

			runWeight  = groundBlend * moveRatio;
			idleWeight = groundBlend * (1.0f - moveRatio);
		} else {
			if (isJumping) {
				if (upWeight <= 0.0f)
					ba[ANIM_UP].animationStartTime = -m_model->animationTime + JUMP_UP_ANIM_OFFSET;
				upWeight   += 0.15f; if (upWeight   > 1.0f) upWeight   = 1.0f;
				downWeight -= 0.01f; if (downWeight < 0.0f) downWeight = 0.0f;
			} else {
				if (downWeight <= 0.0f)
					ba[ANIM_DOWN].animationStartTime = -m_model->animationTime;
				downWeight += 0.15f; if (downWeight > 1.0f) downWeight = 1.0f;
				upWeight   -= 0.01f; if (upWeight   < 0.0f) upWeight   = 0.0f;
			}
			float airBlend = 1.0f - (upWeight + downWeight);
			if (airBlend < 0.0f) airBlend = 0.0f;
			runWeight  = airBlend * 0.5f;
			idleWeight = airBlend * 0.5f;
		}

		ba[ANIM_RUN ].weight = runWeight;
		ba[ANIM_IDLE].weight = idleWeight;
		ba[ANIM_UP  ].weight = upWeight;
		ba[ANIM_DOWN].weight = downWeight;
	}
};
