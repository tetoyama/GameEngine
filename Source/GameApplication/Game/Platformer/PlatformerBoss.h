#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/ColliderComponent.h"
#include "Engine/Scene/Component/materialComponent.h"
#include "Engine/Scene/Component/audioComponent.h"
#include "Engine/Scene/Component/particleComponent.h"
#include "Game/Platformer/PlatformerCameraController.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerFeedback.h"
#include "Game/Platformer/PlatformerGameManager.h"
#include "Game/Platformer/PlatformerSceneAccess.h"
#include "Game/Platformer/PlatformerSoundLibrary.h"

#include <algorithm>
#include <cmath>

class PlatformerBoss : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerBoss)
		REFLECT_FIELD(int, maxHealth, 3)
		REFLECT_FIELD(float, activationDistance, 13.0f)
		REFLECT_FIELD(float, introDuration, 1.1f)
		REFLECT_FIELD(float, chaseDuration, 2.2f)
		REFLECT_FIELD(float, chaseSpeed, 2.8f)
		REFLECT_FIELD(float, telegraphDuration, 0.9f)
		REFLECT_FIELD(float, chargeSpeed, 10.5f)
		REFLECT_FIELD(float, chargeDuration, 1.35f)
		REFLECT_FIELD(float, chargeMaxDistance, 12.0f)
		REFLECT_FIELD(float, stunDuration, 2.7f)
		REFLECT_FIELD(float, recoverDuration, 0.65f)
		REFLECT_FIELD(float, stompHeightMargin, 0.75f)
		REFLECT_FIELD(float, defeatClearDelay, 1.8f)

public:
	enum class State : int {
		Dormant = 0,
		Intro,
		Chase,
		TelegraphCharge,
		Charge,
		Stunned,
		Recover,
		Defeated
	};

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
		ImGui::Text("Platformer Boss");
		INSPECTOR_FIELDS();
		ImGui::Separator();
		ImGui::Text("State: %d", static_cast<int>(state));
		ImGui::Text("Health: %d / %d", health, (std::max)(1, maxHealth));
		ImGui::Text("Timer: %.2f", stateTimer);
	}

	void OnStart() override {
		transform = GetComponentRef<TransformComponent>();
		collider = GetComponentRef<ColliderComponent>();
		material = GetComponentRef<MaterialComponent>();
		particle = GetComponentRef<ParticleComponent>();
		audio = GetComponentRef<AudioComponent>();
		player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		manager = PlatformerSceneAccess::FindFirst<PlatformerGameManager>(m_ref.GetScene());
		camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		health = (std::max)(1, maxHealth);
		state = State::Dormant;
		stateTimer = 0.0f;
		if(auto* t = transform.TryGet()) {
			baseScale = t->scale;
			arenaCenter = t->position;
		}
		if(auto* mat = material.TryGet()) baseMaterial = mat->Material;
	}

	void OnFixedUpdate(float dt) override {
		if(dt <= 0.0f) return;
		ResolveReferences();
		auto* bossPose = transform.TryGet();
		auto* playerController = player.TryGet();
		ComponentRef<TransformComponent> playerTransform(player.GetEntityRef());
		auto* playerPose = playerTransform.TryGet();
		if(!bossPose || !playerController || !playerPose) return;

		switch(state) {
		case State::Dormant:
			StopHorizontalMotion();
			if(HorizontalDistance(bossPose->position, playerPose->position) <= activationDistance) BeginIntro(*playerController);
			break;
		case State::Intro:
			StopHorizontalMotion();
			TickStateTimer(dt);
			if(stateTimer <= 0.0f) BeginBattle(*playerController);
			break;
		case State::Chase:
			MoveToward(*bossPose, playerPose->position, chaseSpeed);
			TickStateTimer(dt);
			if(stateTimer <= 0.0f) BeginChargeTelegraph(*bossPose, playerPose->position);
			break;
		case State::TelegraphCharge:
			StopHorizontalMotion();
			FaceDirection(*bossPose, chargeDirection, dt, 10.0f);
			TickStateTimer(dt);
			if(stateTimer <= 0.0f) BeginCharge(*bossPose);
			break;
		case State::Charge:
			SetHorizontalVelocity(chargeDirection * chargeSpeed);
			FaceDirection(*bossPose, chargeDirection, dt, 18.0f);
			TickStateTimer(dt);
			if(stateTimer <= 0.0f || HorizontalDistance(chargeStartPosition, bossPose->position) >= chargeMaxDistance) EnterStunned();
			break;
		case State::Stunned:
			StopHorizontalMotion();
			TickStateTimer(dt);
			if(stateTimer <= 0.0f) EnterRecover();
			break;
		case State::Recover:
			StopHorizontalMotion();
			TickStateTimer(dt);
			if(stateTimer <= 0.0f) EnterChase();
			break;
		case State::Defeated:
			StopAllMotion();
			break;
		}
	}

	void OnUpdate(float dt) override {
		presentationTime += (std::max)(0.0f, dt);
		UpdatePresentation(dt);
		if(state != State::Defeated) return;

		defeatTimer += (std::max)(0.0f, dt);
		if(defeatTimer >= defeatClearDelay && !clearRequested) {
			clearRequested = true;
			if(auto* game = manager.TryGet()) game->RequestClear();
			if(auto* cameraController = camera.TryGet()) cameraController->SetProfile(PlatformerCameraController::Profile::Clear);
		}
	}

	void OnCollisionEnter(const HitInfo& hit) override {
		if(HandlePlayerContact(hit.other)) return;
		if(state == State::Charge) EnterStunned();
	}

	void OnCollisionStay(const HitInfo& hit) override {
		HandlePlayerContact(hit.other);
	}

	void OnStop() override {
		if(auto* mat = material.TryGet()) mat->Material = baseMaterial;
		if(auto* t = transform.TryGet()) t->scale = baseScale;
	}

	State GetState() const { return state; }
	int GetHealth() const { return health; }
	int GetMaxHealth() const { return (std::max)(1, maxHealth); }
	bool IsStunned() const { return state == State::Stunned; }
	bool IsDefeated() const { return state == State::Defeated; }
	uint32_t GetStateRevision() const { return stateRevision; }
	uint32_t GetHitRevision() const { return hitRevision; }

private:
	void ResolveReferences() {
		if(!player.IsValid()) player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		if(!manager.IsValid()) manager = PlatformerSceneAccess::FindFirst<PlatformerGameManager>(m_ref.GetScene());
		if(!camera.IsValid()) camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		if(!collider.IsValid()) collider = GetComponentRef<ColliderComponent>();
	}

	void BeginIntro(PlatformerCharacterController& playerController) {
		state = State::Intro;
		stateTimer = (std::max)(0.0f, introDuration);
		StopPlayerHorizontalMotion(playerController);
		playerController.SetControlEnabled(false);
		if(auto* game = manager.TryGet()) game->BeginBossIntro();
		if(auto* cameraController = camera.TryGet()) {
			cameraController->SetBossTarget(GetEntityRef());
			cameraController->SetProfile(PlatformerCameraController::Profile::Boss);
		}
		++stateRevision;
	}

	void BeginBattle(PlatformerCharacterController& playerController) {
		playerController.SetControlEnabled(true);
		if(auto* game = manager.TryGet()) game->BeginBossBattle(health);
		EnterChase();
	}

	void EnterChase() {
		state = State::Chase;
		const int phase = (std::max)(0, maxHealth - health);
		stateTimer = (std::max)(0.65f, chaseDuration - phase * 0.28f);
		++stateRevision;
	}

	void BeginChargeTelegraph(const TransformComponent& bossPose, const Vector3& playerPosition) {
		state = State::TelegraphCharge;
		stateTimer = (std::max)(0.35f, telegraphDuration - (maxHealth - health) * 0.08f);
		chargeDirection = HorizontalDirection(bossPose.position, playerPosition);
		if(chargeDirection.length() <= 0.0001f) chargeDirection = bossPose.front().normalize();
		PlatformerFeedback::Burst(particle.TryGet(), bossPose.position + Vector3(0.0f, 0.5f, 0.0f), 20, 1.8f, 2.2f, stateTimer + 0.2f);
		PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::BossChargePath);
		++stateRevision;
	}

	void BeginCharge(const TransformComponent& bossPose) {
		state = State::Charge;
		stateTimer = chargeDuration;
		chargeStartPosition = bossPose.position;
		++stateRevision;
	}

	void EnterStunned() {
		if(state == State::Stunned || state == State::Defeated) return;
		state = State::Stunned;
		stateTimer = stunDuration;
		StopHorizontalMotion();
		if(auto* t = transform.TryGet()) PlatformerFeedback::Burst(particle.TryGet(), t->position + Vector3(0.0f, 0.8f, 0.0f), 30, 3.8f, 5.0f, 0.9f);
		++stateRevision;
	}

	void EnterRecover() {
		state = State::Recover;
		stateTimer = recoverDuration;
		++stateRevision;
	}

	bool HandlePlayerContact(const EntityRef& other) {
		if(!other.IsValid() || state == State::Dormant || state == State::Intro || state == State::Defeated) return false;
		ComponentRef<PlatformerCharacterController> playerControllerRef(other);
		auto* playerController = playerControllerRef.TryGet();
		if(!playerController) return false;

		ComponentRef<TransformComponent> playerTransform(other);
		auto* playerPose = playerTransform.TryGet();
		auto* bossPose = transform.TryGet();
		if(!playerPose || !bossPose) return true;

		const bool aboveWeakPoint = playerPose->position.y >= bossPose->position.y + stompHeightMargin;
		if(state == State::Stunned && playerController->IsDescending() && aboveWeakPoint) {
			ReceiveStomp(*playerController, bossPose->position);
			return true;
		}

		if(state == State::Charge || state == State::Chase || state == State::TelegraphCharge) {
			playerController->ApplyDamage(bossPose->position);
			if(state == State::Charge) EnterRecover();
		}
		return true;
	}

	void ReceiveStomp(PlatformerCharacterController& playerController, const Vector3& position) {
		if(state != State::Stunned || health <= 0) return;
		--health;
		++hitRevision;
		playerController.ApplyStompBounce(9.0f);
		PlatformerFeedback::Burst(particle.TryGet(), position + Vector3(0.0f, 1.1f, 0.0f), 38, 4.4f, 6.0f, 1.0f);
		PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ImpactPath);
		if(auto* game = manager.TryGet()) game->SetBossHealth(health);

		if(health <= 0) {
			BeginDefeat();
		} else {
			state = State::Recover;
			stateTimer = recoverDuration + 0.45f;
			++stateRevision;
		}
	}

	void BeginDefeat() {
		state = State::Defeated;
		defeatTimer = 0.0f;
		StopAllMotion();
		if(auto* col = collider.TryGet()) {
			if(auto* rigid = col->pRigidbodyDynamic) rigid->setActorFlag(physx::PxActorFlag::eDISABLE_SIMULATION, true);
		}
		if(auto* game = manager.TryGet()) game->NotifyBossDefeated();
		++stateRevision;
	}

	void MoveToward(TransformComponent& bossPose, const Vector3& target, float speed) {
		const Vector3 direction = HorizontalDirection(bossPose.position, target);
		SetHorizontalVelocity(direction * speed);
		FaceDirection(bossPose, direction, 1.0f / 60.0f, 8.0f);
	}

	void SetHorizontalVelocity(const Vector3& horizontal) {
		if(auto* col = collider.TryGet()) {
			if(auto* rigid = col->pRigidbodyDynamic) {
				const physx::PxVec3 current = rigid->getLinearVelocity();
				rigid->setLinearVelocity(physx::PxVec3(horizontal.x, current.y, horizontal.z));
				return;
			}
		}
		if(auto* t = transform.TryGet()) t->position += horizontal * (1.0f / 60.0f);
	}

	void StopHorizontalMotion() {
		if(auto* col = collider.TryGet()) {
			if(auto* rigid = col->pRigidbodyDynamic) {
				const physx::PxVec3 current = rigid->getLinearVelocity();
				rigid->setLinearVelocity(physx::PxVec3(0.0f, current.y, 0.0f));
			}
		}
	}

	static void StopPlayerHorizontalMotion(PlatformerCharacterController& playerController) {
		ComponentRef<ColliderComponent> playerCollider(playerController.GetEntityRef());
		if(auto* col = playerCollider.TryGet()) {
			if(auto* rigid = col->pRigidbodyDynamic) {
				const physx::PxVec3 current = rigid->getLinearVelocity();
				rigid->setLinearVelocity(physx::PxVec3(0.0f, current.y, 0.0f));
			}
		}
	}

	void StopAllMotion() {
		if(auto* col = collider.TryGet()) {
			if(auto* rigid = col->pRigidbodyDynamic) {
				rigid->setLinearVelocity(physx::PxVec3(0.0f));
				rigid->setAngularVelocity(physx::PxVec3(0.0f));
			}
		}
	}

	void UpdatePresentation(float dt) {
		auto* t = transform.TryGet();
		auto* mat = material.TryGet();
		if(!t) return;

		Vector3 targetScale = baseScale;
		MATERIAL targetMaterial = baseMaterial;
		if(state == State::TelegraphCharge) {
			const float pulse = 1.0f + 0.10f * std::sin(presentationTime * 18.0f);
			targetScale = baseScale * pulse;
			targetMaterial.BaseColor = float4(1.0f, 0.22f, 0.12f, baseMaterial.BaseColor.w);
			targetMaterial.EmissiveColor = float3(1.0f, 0.05f, 0.01f);
			targetMaterial.EmissiveIntensity = 2.2f;
		} else if(state == State::Stunned) {
			targetScale = Vector3(baseScale.x * 1.08f, baseScale.y * 0.72f, baseScale.z * 1.08f);
			targetMaterial.BaseColor = float4(0.25f, 0.65f, 1.0f, baseMaterial.BaseColor.w);
			targetMaterial.EmissiveColor = float3(0.05f, 0.25f, 1.0f);
			targetMaterial.EmissiveIntensity = 1.5f;
		} else if(state == State::Defeated) {
			const float normalized = defeatClearDelay > 0.0f ? std::clamp(defeatTimer / defeatClearDelay, 0.0f, 1.0f) : 1.0f;
			targetScale = Vector3(baseScale.x * (1.0f + normalized * 0.8f), baseScale.y * (1.0f - normalized), baseScale.z * (1.0f + normalized * 0.8f));
			t->AddRotationY(dt * (4.0f + normalized * 8.0f));
			targetMaterial.EmissiveColor = float3(1.0f, 0.7f, 0.15f);
			targetMaterial.EmissiveIntensity = 3.0f * (1.0f - normalized);
		}

		const float blend = 1.0f - std::exp(-10.0f * (std::max)(0.0f, dt));
		t->scale = Vec3Lerp(t->scale, targetScale, blend);
		if(mat) {
			mat->Material.BaseColor.x += (targetMaterial.BaseColor.x - mat->Material.BaseColor.x) * blend;
			mat->Material.BaseColor.y += (targetMaterial.BaseColor.y - mat->Material.BaseColor.y) * blend;
			mat->Material.BaseColor.z += (targetMaterial.BaseColor.z - mat->Material.BaseColor.z) * blend;
			mat->Material.EmissiveColor.x += (targetMaterial.EmissiveColor.x - mat->Material.EmissiveColor.x) * blend;
			mat->Material.EmissiveColor.y += (targetMaterial.EmissiveColor.y - mat->Material.EmissiveColor.y) * blend;
			mat->Material.EmissiveColor.z += (targetMaterial.EmissiveColor.z - mat->Material.EmissiveColor.z) * blend;
			mat->Material.EmissiveIntensity += (targetMaterial.EmissiveIntensity - mat->Material.EmissiveIntensity) * blend;
		}
	}

	void TickStateTimer(float dt) {
		stateTimer = (std::max)(0.0f, stateTimer - dt);
	}

	static Vector3 HorizontalDirection(const Vector3& from, const Vector3& to) {
		Vector3 result = to - from;
		result.y = 0.0f;
		return result.length() > 0.0001f ? result.normalize() : Vector3{};
	}

	static float HorizontalDistance(const Vector3& a, const Vector3& b) {
		const float x = a.x - b.x;
		const float z = a.z - b.z;
		return std::sqrt(x * x + z * z);
	}

	static void FaceDirection(TransformComponent& transform, const Vector3& direction, float dt, float sharpness) {
		if(direction.length() <= 0.0001f) return;
		const float yaw = std::atan2(direction.x, direction.z);
		const DirectX::XMVECTOR target = DirectX::XMQuaternionRotationRollPitchYaw(0.0f, yaw, 0.0f);
		const float blend = 1.0f - std::exp(-sharpness * (std::max)(0.0f, dt));
		const DirectX::XMVECTOR result = DirectX::XMQuaternionSlerp(transform.rotationVector(), target, blend);
		DirectX::XMFLOAT4 rotation;
		DirectX::XMStoreFloat4(&rotation, result);
		transform.SetRotation(rotation);
	}

	ComponentRef<TransformComponent> transform;
	ComponentRef<ColliderComponent> collider;
	ComponentRef<MaterialComponent> material;
	ComponentRef<ParticleComponent> particle;
	ComponentRef<AudioComponent> audio;
	ComponentRef<PlatformerCharacterController> player;
	ComponentRef<PlatformerGameManager> manager;
	ComponentRef<PlatformerCameraController> camera;
	State state = State::Dormant;
	Vector3 arenaCenter;
	Vector3 baseScale = Vector3(1.0f, 1.0f, 1.0f);
	Vector3 chargeDirection;
	Vector3 chargeStartPosition;
	MATERIAL baseMaterial{};
	int health = 3;
	float stateTimer = 0.0f;
	float presentationTime = 0.0f;
	float defeatTimer = 0.0f;
	bool clearRequested = false;
	uint32_t stateRevision = 0;
	uint32_t hitRevision = 0;
};
