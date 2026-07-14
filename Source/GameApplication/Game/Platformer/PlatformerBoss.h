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
		REFLECT_FIELD(float, effectCenterHeight, 1.0f)
		REFLECT_FIELD(float, telegraphPulseInterval, 0.14f)
		REFLECT_FIELD(float, defeatBurstInterval, 0.22f)

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
		ValidatePresentationSettings();
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
		ValidatePresentationSettings();
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
		presentationTime = 0.0f;
		defeatTimer = 0.0f;
		telegraphPulseTimer = 0.0f;
		chargeSparkTimer = 0.0f;
		defeatBurstTimer = 0.0f;
		hitFlashTimer = 0.0f;
		impactPulse = 0.0f;
		defeatBurstIndex = 0;
		clearRequested = false;
		if(auto* t = transform.TryGet()) {
			baseScale = t->scale;
			arenaCenter = t->position;
		}
		if(auto* mat = material.TryGet()) baseMaterial = mat->Material;
		ClearParticles();
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
			if(HorizontalDistance(bossPose->position, playerPose->position) <= activationDistance) {
				BeginIntro(*playerController);
			}
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
			FaceDirection(*bossPose, chargeDirection, dt, 12.0f);
			TickTelegraphFeedback(dt);
			TickStateTimer(dt);
			if(stateTimer <= 0.0f) BeginCharge(*bossPose);
			break;
		case State::Charge:
			SetHorizontalVelocity(chargeDirection * chargeSpeed);
			FaceDirection(*bossPose, chargeDirection, dt, 22.0f);
			TickChargeFeedback(dt);
			TickStateTimer(dt);
			if(stateTimer <= 0.0f ||
			   HorizontalDistance(chargeStartPosition, bossPose->position) >= chargeMaxDistance) {
				EnterStunned();
			}
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
		const float safeDt = (std::max)(0.0f, dt);
		presentationTime += safeDt;
		hitFlashTimer = (std::max)(0.0f, hitFlashTimer - safeDt);
		impactPulse = (std::max)(0.0f, impactPulse - safeDt * 5.5f);
		UpdatePresentation(safeDt);

		if(state != State::Defeated) return;

		defeatTimer += safeDt;
		TickDefeatFeedback(safeDt);
		if(defeatTimer >= defeatClearDelay && !clearRequested) {
			clearRequested = true;
			if(auto* game = manager.TryGet()) game->RequestClear();
			if(auto* cameraController = camera.TryGet()) {
				cameraController->SetProfile(PlatformerCameraController::Profile::Clear);
			}
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
		ClearParticles();
	}

	State GetState() const { return state; }
	int GetHealth() const { return health; }
	int GetMaxHealth() const { return (std::max)(1, maxHealth); }
	bool IsStunned() const { return state == State::Stunned; }
	bool IsDefeated() const { return state == State::Defeated; }
	uint32_t GetStateRevision() const { return stateRevision; }
	uint32_t GetHitRevision() const { return hitRevision; }

private:
	void ValidatePresentationSettings() {
		effectCenterHeight = (std::max)(0.0f, effectCenterHeight);
		telegraphPulseInterval = (std::max)(0.06f, telegraphPulseInterval);
		defeatBurstInterval = (std::max)(0.10f, defeatBurstInterval);
	}

	void ResolveReferences() {
		if(!player.IsValid()) {
			player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		}
		if(!manager.IsValid()) {
			manager = PlatformerSceneAccess::FindFirst<PlatformerGameManager>(m_ref.GetScene());
		}
		if(!camera.IsValid()) {
			camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		}
		if(!collider.IsValid()) collider = GetComponentRef<ColliderComponent>();
	}

	void ConfigureBossCamera() {
		auto* cameraController = camera.TryGet();
		if(!cameraController) return;

		cameraController->comfortCamera = false;
		cameraController->impulsePositionScale =
			(std::max)(cameraController->impulsePositionScale, 0.16f);
		cameraController->impulseFrequency =
			(std::max)(cameraController->impulseFrequency, 9.0f);
		cameraController->impulseResponseSharpness =
			(std::max)(cameraController->impulseResponseSharpness, 14.0f);
		cameraController->maxImpulseFovKick =
			(std::max)(cameraController->maxImpulseFovKick, 0.040f);
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
		ConfigureBossCamera();
		EmitBossBurst(Vector3(0.0f, effectCenterHeight, 0.0f),
			64, 5.8f, 7.2f, 0.95f, 0.24f, 1.35f);
		BossImpulse(0.72f, 0.38f, 0.022f, Vector3(0.0f, 1.0f, 0.0f));
		PlatformerFeedback::Play(
			audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::BossChargePath);
		impactPulse = 0.85f;
		hitFlashTimer = 0.16f;
		++stateRevision;
	}

	void BeginBattle(PlatformerCharacterController& playerController) {
		playerController.SetControlEnabled(true);
		if(auto* game = manager.TryGet()) game->BeginBossBattle(health);
		BossImpulse(0.42f, 0.24f, 0.012f, Vector3(0.0f, -1.0f, 0.0f));
		EnterChase();
	}

	void EnterChase() {
		state = State::Chase;
		const int phase = (std::max)(0, maxHealth - health);
		stateTimer = (std::max)(0.65f, chaseDuration - phase * 0.28f);
		++stateRevision;
	}

	void BeginChargeTelegraph(
		const TransformComponent& bossPose,
		const Vector3& playerPosition
	) {
		state = State::TelegraphCharge;
		stateTimer = (std::max)(0.35f, telegraphDuration - (maxHealth - health) * 0.08f);
		chargeDirection = HorizontalDirection(bossPose.position, playerPosition);
		if(chargeDirection.length() <= 0.0001f) chargeDirection = bossPose.front().normalize();
		telegraphPulseTimer = 0.0f;
		EmitBossBurst(Vector3(0.0f, effectCenterHeight, 0.0f),
			72, 6.4f, 4.8f, stateTimer + 0.15f, 0.20f, 1.55f);
		BossImpulse(0.40f, 0.25f, -0.014f, chargeDirection * -1.0f);
		PlatformerFeedback::Play(
			audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::BossChargePath);
		impactPulse = 0.65f;
		++stateRevision;
	}

	void BeginCharge(const TransformComponent& bossPose) {
		state = State::Charge;
		stateTimer = chargeDuration;
		chargeStartPosition = bossPose.position;
		chargeSparkTimer = 0.0f;
		EmitBossBurst(Vector3(0.0f, effectCenterHeight * 0.72f, 0.0f),
			84, 8.5f, 3.2f, 0.52f, 0.24f, 1.75f);
		BossImpulse(0.92f, 0.32f, 0.030f, chargeDirection);
		impactPulse = 1.0f;
		hitFlashTimer = 0.10f;
		++stateRevision;
	}

	void EnterStunned() {
		if(state == State::Stunned || state == State::Defeated) return;
		state = State::Stunned;
		stateTimer = stunDuration;
		StopHorizontalMotion();
		EmitBossBurst(Vector3(0.0f, effectCenterHeight * 0.65f, 0.0f),
			96, 10.5f, 9.0f, 1.15f, 0.28f, 2.0f);
		BossImpulse(1.12f, 0.42f, -0.030f, chargeDirection * -1.0f);
		PlatformerFeedback::Play(
			audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ImpactPath);
		impactPulse = 1.0f;
		hitFlashTimer = 0.20f;
		++stateRevision;
	}

	void EnterRecover() {
		state = State::Recover;
		stateTimer = recoverDuration;
		EmitBossBurst(Vector3(0.0f, effectCenterHeight, 0.0f),
			42, 4.0f, 5.5f, 0.48f, 0.16f, 1.25f);
		BossImpulse(0.30f, 0.18f, 0.008f, Vector3(0.0f, 1.0f, 0.0f));
		++stateRevision;
	}

	bool HandlePlayerContact(const EntityRef& other) {
		if(!other.IsValid() ||
		   state == State::Dormant ||
		   state == State::Intro ||
		   state == State::Defeated) {
			return false;
		}

		ComponentRef<PlatformerCharacterController> playerControllerRef(other);
		auto* playerController = playerControllerRef.TryGet();
		if(!playerController) return false;

		ComponentRef<TransformComponent> playerTransform(other);
		auto* playerPose = playerTransform.TryGet();
		auto* bossPose = transform.TryGet();
		if(!playerPose || !bossPose) return true;

		const bool aboveWeakPoint =
			playerPose->position.y >= bossPose->position.y + stompHeightMargin;
		if(state == State::Stunned && playerController->IsDescending() && aboveWeakPoint) {
			ReceiveStomp(*playerController, bossPose->position);
			return true;
		}

		if(state == State::Charge ||
		   state == State::Chase ||
		   state == State::TelegraphCharge) {
			const bool damaged = playerController->ApplyDamage(bossPose->position);
			if(damaged) {
				BossImpulse(0.62f, 0.28f, -0.016f, chargeDirection);
			}
			if(state == State::Charge) EnterRecover();
		}
		return true;
	}

	void ReceiveStomp(
		PlatformerCharacterController& playerController,
		const Vector3& position
	) {
		if(state != State::Stunned || health <= 0) return;
		(void)position;
		--health;
		++hitRevision;
		playerController.ApplyStompBounce(10.4f);

		EmitBossBurst(Vector3(0.0f, effectCenterHeight, 0.0f),
			MAXPARTICLE, 13.5f, 12.0f, 1.30f, 0.32f, 2.35f);
		BossImpulse(1.42f, 0.48f, 0.040f, Vector3(0.0f, -1.0f, 0.0f));
		PlatformerFeedback::Play(
			audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ImpactPath);
		impactPulse = 1.0f;
		hitFlashTimer = 0.28f;

		if(auto* game = manager.TryGet()) game->SetBossHealth(health);

		if(health <= 0) {
			BeginDefeat();
		} else {
			state = State::Recover;
			stateTimer = recoverDuration + 0.55f;
			++stateRevision;
		}
	}

	void BeginDefeat() {
		state = State::Defeated;
		defeatTimer = 0.0f;
		defeatBurstTimer = 0.0f;
		defeatBurstIndex = 0;
		StopAllMotion();
		if(auto* col = collider.TryGet()) {
			if(auto* rigid = col->pRigidbodyDynamic) {
				rigid->setActorFlag(physx::PxActorFlag::eDISABLE_SIMULATION, true);
			}
		}

		EmitBossBurst(Vector3(0.0f, effectCenterHeight, 0.0f),
			MAXPARTICLE, 16.0f, 15.0f, 1.55f, 0.38f, 2.75f);
		BossImpulse(1.50f, 0.50f, 0.040f, Vector3(0.0f, 1.0f, 0.0f));
		PlatformerFeedback::Play(
			audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ClearPath);
		hitFlashTimer = 0.42f;
		impactPulse = 1.0f;

		if(auto* game = manager.TryGet()) game->NotifyBossDefeated();
		++stateRevision;
	}

	void TickTelegraphFeedback(float dt) {
		telegraphPulseTimer -= dt;
		if(telegraphPulseTimer > 0.0f) return;

		const float normalized = telegraphDuration > 0.0f
			? std::clamp(1.0f - stateTimer / telegraphDuration, 0.0f, 1.0f)
			: 1.0f;
		const int count = 28 + static_cast<int>(normalized * 20.0f);
		EmitBossBurst(Vector3(0.0f, effectCenterHeight, 0.0f),
			count,
			3.5f + normalized * 4.5f,
			2.5f + normalized * 3.0f,
			0.24f,
			0.12f + normalized * 0.06f,
			1.2f + normalized * 0.5f);
		BossImpulse(
			0.14f + normalized * 0.18f,
			0.10f,
			-0.003f - normalized * 0.004f,
			chargeDirection * -1.0f);
		telegraphPulseTimer =
			telegraphPulseInterval * (1.0f - normalized * 0.42f);
	}

	void TickChargeFeedback(float dt) {
		chargeSparkTimer -= dt;
		if(chargeSparkTimer > 0.0f) return;

		EmitBossBurst(Vector3(0.0f, effectCenterHeight * 0.55f, 0.0f),
			34, 7.5f, 2.2f, 0.18f, 0.13f, 1.6f);
		BossImpulse(0.16f, 0.08f, 0.003f, chargeDirection);
		chargeSparkTimer = 0.09f;
	}

	void TickDefeatFeedback(float dt) {
		defeatBurstTimer -= dt;
		if(defeatBurstTimer > 0.0f || defeatTimer >= defeatClearDelay * 0.88f) return;

		const float normalized = defeatClearDelay > 0.0f
			? std::clamp(defeatTimer / defeatClearDelay, 0.0f, 1.0f)
			: 1.0f;
		const float angle = static_cast<float>(defeatBurstIndex) * 2.39996323f;
		const float radius = 0.30f + 0.55f * static_cast<float>(defeatBurstIndex % 3);
		const Vector3 offset(
			std::cos(angle) * radius,
			effectCenterHeight * (0.45f + 0.18f * static_cast<float>(defeatBurstIndex % 4)),
			std::sin(angle) * radius);

		EmitBossBurst(
			offset,
			54 + static_cast<int>(normalized * 34.0f),
			8.0f + normalized * 7.0f,
			8.5f + normalized * 6.0f,
			0.55f,
			0.20f + normalized * 0.09f,
			1.6f + normalized * 0.9f);
		BossImpulse(
			0.30f + normalized * 0.32f,
			0.18f,
			0.008f + normalized * 0.010f,
			Vector3(std::cos(angle), 0.35f, std::sin(angle)));
		++defeatBurstIndex;
		defeatBurstTimer =
			defeatBurstInterval * (1.0f - normalized * 0.48f);
	}

	void EmitBossBurst(
		const Vector3& worldOffsetFromBoss,
		int count,
		float horizontalWorldSpeed,
		float upwardWorldSpeed,
		float lifetime,
		float particleSize,
		float spikeMultiplier
	) {
		auto* p = particle.TryGet();
		if(!p) return;

		const float safeSize = (std::max)(0.02f, particleSize);
		const float inverseSize = 1.0f / safeSize;
		for(auto& state : p->Particle) state.LifeTime = 0.0f;
		p->particleSize = safeSize;

		PlatformerFeedback::Burst(
			p,
			worldOffsetFromBoss * inverseSize,
			count,
			horizontalWorldSpeed * inverseSize,
			upwardWorldSpeed * inverseSize,
			lifetime);

		const int safeCount = std::clamp(count, 1, MAXPARTICLE);
		for(int i = 0; i < safeCount; ++i) {
			const float layer = (i % 6 == 0)
				? spikeMultiplier
				: ((i % 3 == 0) ? 1.25f : 1.0f);
			p->Particle[i].Speed = p->Particle[i].Speed * layer;
			if(i % 2 == 0) p->Particle[i].Speed.y *= 0.72f;
		}
	}

	void ClearParticles() {
		if(auto* p = particle.TryGet()) {
			for(auto& state : p->Particle) state.LifeTime = 0.0f;
			p->SpawnCount = 0;
			p->SpawnTimer = 0.0f;
		}
	}

	void BossImpulse(
		float strength,
		float duration,
		float fovKick,
		const Vector3& direction = Vector3()
	) {
		if(!camera.IsValid()) {
			camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		}
		ConfigureBossCamera();
		if(auto* cameraController = camera.TryGet()) {
			cameraController->AddImpulse(strength, duration, fovKick, direction);
		}
	}

	void MoveToward(
		TransformComponent& bossPose,
		const Vector3& target,
		float speed
	) {
		const Vector3 direction = HorizontalDirection(bossPose.position, target);
		SetHorizontalVelocity(direction * speed);
		FaceDirection(bossPose, direction, 1.0f / 60.0f, 8.0f);
	}

	void SetHorizontalVelocity(const Vector3& horizontal) {
		if(auto* col = collider.TryGet()) {
			if(auto* rigid = col->pRigidbodyDynamic) {
				const physx::PxVec3 current = rigid->getLinearVelocity();
				rigid->setLinearVelocity(
					physx::PxVec3(horizontal.x, current.y, horizontal.z));
				return;
			}
		}
		if(auto* t = transform.TryGet()) {
			t->position += horizontal * (1.0f / 60.0f);
		}
	}

	void StopHorizontalMotion() {
		if(auto* col = collider.TryGet()) {
			if(auto* rigid = col->pRigidbodyDynamic) {
				const physx::PxVec3 current = rigid->getLinearVelocity();
				rigid->setLinearVelocity(physx::PxVec3(0.0f, current.y, 0.0f));
			}
		}
	}

	static void StopPlayerHorizontalMotion(
		PlatformerCharacterController& playerController
	) {
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

		if(state == State::Intro) {
			const float pulse = 0.5f + 0.5f * std::sin(presentationTime * 12.0f);
			targetScale = Vector3(
				baseScale.x * (1.04f + pulse * 0.10f),
				baseScale.y * (0.96f + pulse * 0.15f),
				baseScale.z * (1.04f + pulse * 0.10f));
			targetMaterial.BaseColor =
				float4(0.82f, 0.28f, 1.0f, baseMaterial.BaseColor.w);
			targetMaterial.EmissiveColor = float3(0.55f, 0.08f, 1.0f);
			targetMaterial.EmissiveIntensity = 3.8f + pulse * 2.0f;
		} else if(state == State::TelegraphCharge) {
			const float pulse = 0.5f + 0.5f * std::sin(presentationTime * 24.0f);
			targetScale = Vector3(
				baseScale.x * (1.06f + pulse * 0.22f),
				baseScale.y * (0.84f + pulse * 0.24f),
				baseScale.z * (1.06f + pulse * 0.22f));
			targetMaterial.BaseColor =
				float4(1.0f, 0.16f + pulse * 0.16f, 0.05f, baseMaterial.BaseColor.w);
			targetMaterial.EmissiveColor = float3(1.0f, 0.03f, 0.005f);
			targetMaterial.EmissiveIntensity = 4.8f + pulse * 3.2f;
		} else if(state == State::Charge) {
			const float flicker = 0.5f + 0.5f * std::sin(presentationTime * 38.0f);
			targetScale = Vector3(
				baseScale.x * 0.78f,
				baseScale.y * (0.86f + flicker * 0.06f),
				baseScale.z * 1.38f);
			targetMaterial.BaseColor =
				float4(1.0f, 0.32f, 0.04f, baseMaterial.BaseColor.w);
			targetMaterial.EmissiveColor = float3(1.0f, 0.08f, 0.005f);
			targetMaterial.EmissiveIntensity = 6.0f + flicker * 2.0f;
		} else if(state == State::Stunned) {
			const float wobble = 0.5f + 0.5f * std::sin(presentationTime * 15.0f);
			targetScale = Vector3(
				baseScale.x * (1.20f + wobble * 0.10f),
				baseScale.y * (0.52f + wobble * 0.07f),
				baseScale.z * (1.20f + wobble * 0.10f));
			targetMaterial.BaseColor =
				float4(0.20f, 0.72f, 1.0f, baseMaterial.BaseColor.w);
			targetMaterial.EmissiveColor = float3(0.03f, 0.35f, 1.0f);
			targetMaterial.EmissiveIntensity = 3.8f + wobble * 1.6f;
		} else if(state == State::Recover) {
			const float bounce = std::sin(
				std::clamp(stateTimer / (std::max)(0.01f, recoverDuration), 0.0f, 1.0f) *
				DirectX::XM_PI);
			targetScale = Vector3(
				baseScale.x * (1.0f - bounce * 0.10f),
				baseScale.y * (1.0f + bounce * 0.22f),
				baseScale.z * (1.0f - bounce * 0.10f));
			targetMaterial.EmissiveColor = float3(0.55f, 0.10f, 0.9f);
			targetMaterial.EmissiveIntensity = 1.2f + bounce * 1.8f;
		} else if(state == State::Defeated) {
			const float normalized = defeatClearDelay > 0.0f
				? std::clamp(defeatTimer / defeatClearDelay, 0.0f, 1.0f)
				: 1.0f;
			const float swell = std::sin(
				std::clamp(normalized / 0.62f, 0.0f, 1.0f) *
				DirectX::XM_PI * 0.5f);
			const float collapse = normalized > 0.62f
				? std::clamp((normalized - 0.62f) / 0.38f, 0.0f, 1.0f)
				: 0.0f;
			const float remaining = 1.0f - collapse;
			targetScale = Vector3(
				baseScale.x * (1.0f + swell * 1.30f) * remaining,
				baseScale.y * (1.0f + swell * 0.65f) * remaining,
				baseScale.z * (1.0f + swell * 1.30f) * remaining);
			t->AddRotationY(dt * (8.0f + normalized * 22.0f));
			targetMaterial.BaseColor =
				float4(1.0f, 0.78f, 0.25f, baseMaterial.BaseColor.w);
			targetMaterial.EmissiveColor = float3(1.0f, 0.55f, 0.08f);
			targetMaterial.EmissiveIntensity =
				(8.5f + swell * 5.0f) * remaining;
		}

		if(impactPulse > 0.0f) {
			const float pulse = impactPulse * impactPulse;
			targetScale.x *= 1.0f + pulse * 0.26f;
			targetScale.y *= 1.0f - pulse * 0.30f;
			targetScale.z *= 1.0f + pulse * 0.26f;
		}

		if(hitFlashTimer > 0.0f) {
			targetMaterial.BaseColor =
				float4(1.0f, 1.0f, 1.0f, baseMaterial.BaseColor.w);
			targetMaterial.EmissiveColor = float3(1.0f, 0.92f, 0.62f);
			targetMaterial.EmissiveIntensity = 10.0f;
		}

		const float blend = 1.0f - std::exp(-14.0f * dt);
		t->scale = Vec3Lerp(t->scale, targetScale, blend);
		if(mat) {
			mat->Material.BaseColor.x +=
				(targetMaterial.BaseColor.x - mat->Material.BaseColor.x) * blend;
			mat->Material.BaseColor.y +=
				(targetMaterial.BaseColor.y - mat->Material.BaseColor.y) * blend;
			mat->Material.BaseColor.z +=
				(targetMaterial.BaseColor.z - mat->Material.BaseColor.z) * blend;
			mat->Material.EmissiveColor.x +=
				(targetMaterial.EmissiveColor.x - mat->Material.EmissiveColor.x) * blend;
			mat->Material.EmissiveColor.y +=
				(targetMaterial.EmissiveColor.y - mat->Material.EmissiveColor.y) * blend;
			mat->Material.EmissiveColor.z +=
				(targetMaterial.EmissiveColor.z - mat->Material.EmissiveColor.z) * blend;
			mat->Material.EmissiveIntensity +=
				(targetMaterial.EmissiveIntensity - mat->Material.EmissiveIntensity) * blend;
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

	static void FaceDirection(
		TransformComponent& transform,
		const Vector3& direction,
		float dt,
		float sharpness
	) {
		if(direction.length() <= 0.0001f) return;
		const float yaw = std::atan2(direction.x, direction.z);
		const DirectX::XMVECTOR target =
			DirectX::XMQuaternionRotationRollPitchYaw(0.0f, yaw, 0.0f);
		const float blend =
			1.0f - std::exp(-sharpness * (std::max)(0.0f, dt));
		const DirectX::XMVECTOR result =
			DirectX::XMQuaternionSlerp(transform.rotationVector(), target, blend);
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
	int defeatBurstIndex = 0;
	float stateTimer = 0.0f;
	float presentationTime = 0.0f;
	float defeatTimer = 0.0f;
	float telegraphPulseTimer = 0.0f;
	float chargeSparkTimer = 0.0f;
	float defeatBurstTimer = 0.0f;
	float hitFlashTimer = 0.0f;
	float impactPulse = 0.0f;
	bool clearRequested = false;
	uint32_t stateRevision = 0;
	uint32_t hitRevision = 0;
};
