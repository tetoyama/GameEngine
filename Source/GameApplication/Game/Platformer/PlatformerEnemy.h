#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/ColliderComponent.h"
#include "Engine/Scene/Component/entityNameComponent.h"
#include "Engine/Scene/Component/materialComponent.h"
#include "Engine/Scene/Component/audioComponent.h"
#include "Engine/Scene/Component/particleComponent.h"
#include "Game/Platformer/PlatformerCameraController.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerFeedback.h"
#include "Game/Platformer/PlatformerSceneAccess.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

class PlatformerEnemy : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerEnemy)
		REFLECT_FIELD(Vector3, patrolAxis, Vector3(1.0f, 0.0f, 0.0f))
		REFLECT_FIELD(float, patrolDistance, 3.0f)
		REFLECT_FIELD(float, patrolSpeed, 1.6f)
		// -1: auto by authored enemy name, 0: smooth patrol,
		// 1: periodic burst patrol, 2: player-reactive guard.
		REFLECT_FIELD(int, movementStyle, -1)
		REFLECT_FIELD(float, movementAcceleration, 7.5f)
		REFLECT_FIELD(float, turnApproachDistance, 0.62f)
		REFLECT_FIELD(float, turnPauseSeconds, 0.22f)
		REFLECT_FIELD(float, burstInterval, 2.7f)
		REFLECT_FIELD(float, burstWindupSeconds, 0.30f)
		REFLECT_FIELD(float, burstDuration, 0.34f)
		REFLECT_FIELD(float, burstSpeedMultiplier, 2.35f)
		REFLECT_FIELD(float, awarenessDistance, 6.0f)
		REFLECT_FIELD(float, awarenessCorridorWidth, 2.6f)
		REFLECT_FIELD(float, reactionCooldown, 1.65f)
		REFLECT_FIELD(bool, motionSquashEnabled, true)
		REFLECT_FIELD(bool, movementDustEnabled, true)
		REFLECT_FIELD(float, stompHeightMargin, 0.12f)
		REFLECT_FIELD(float, defeatDuration, 0.42f)
		REFLECT_FIELD(bool, turnDustEnabled, true)

public:
	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		(void)context;
		DECODE_FIELDS(node);
		ValidateSettings();
		return true;
	}

	void inspector(SceneContext* context) override {
		(void)context;
		ImGui::Text("Platformer Enemy");
		INSPECTOR_FIELDS();
		ImGui::Separator();
		ImGui::Text("Resolved Style: %s", MovementStyleName(resolvedMovementStyle));
		ImGui::Text("Motion State: %s", MotionStateName(motionState));
		ImGui::Text("Speed: %.2f", currentPlanarSpeed);
		ImGui::Text("Defeated: %s", defeated ? "true" : "false");
	}

	void OnStart() override {
		ValidateSettings();
		transform = GetComponentRef<TransformComponent>();
		collider = GetComponentRef<ColliderComponent>();
		material = GetComponentRef<MaterialComponent>();
		particle = GetComponentRef<ParticleComponent>();
		audio = GetComponentRef<AudioComponent>();
		if(!particle.IsValid()) {
			QueueAddComponent<ParticleComponent>(m_ref.GetEntityID());
		}
		camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());

		AlignColliderToModelCenter();

		if(auto* t = transform.TryGet()) {
			origin = t->position;
			baseScale = t->scale;
		}
		if(auto* mat = material.TryGet()) baseMaterial = mat->Material;
		if(patrolAxis.length() <= 0.0001f) patrolAxis = Vector3(1.0f, 0.0f, 0.0f);
		patrolAxis.y = 0.0f;
		patrolAxis = patrolAxis.normalize();

		resolvedMovementStyle = ResolveMovementStyle();
		const float phase = DeterministicPhase();
		burstTimer = burstInterval * (0.55f + phase * 0.45f);
		reactionTimer = reactionCooldown * phase;
		motionState = MotionState::Patrol;
	}

	void OnFixedUpdate(float dt) override {
		if(defeated || dt <= 0.0f) return;
		auto* t = transform.TryGet();
		if(!t) return;

		const float safeDt = std::clamp(dt, 0.0f, 0.05f);
		turnDustCooldown = (std::max)(0.0f, turnDustCooldown - safeDt);
		stepDustTimer = (std::max)(0.0f, stepDustTimer - safeDt);
		burstTimer = (std::max)(0.0f, burstTimer - safeDt);
		reactionTimer = (std::max)(0.0f, reactionTimer - safeDt);

		EnsureParticleEmitter();
		const float signedDistance = (t->position - origin).dot(patrolAxis);
		switch(motionState) {
		case MotionState::Patrol:
			TickPatrol(safeDt, signedDistance);
			break;
		case MotionState::TurnPause:
			TickTurnPause(safeDt);
			break;
		case MotionState::Windup:
			TickWindup(safeDt, signedDistance);
			break;
		case MotionState::Burst:
			TickBurst(safeDt, signedDistance);
			break;
		case MotionState::Recover:
			TickRecover(safeDt);
			break;
		}

		if(movementDustEnabled && currentPlanarSpeed > patrolSpeed * 0.55f &&
		   stepDustTimer <= 0.0f && motionState != MotionState::Windup &&
		   motionState != MotionState::TurnPause) {
			EmitStepDust();
			const float speedFactor = std::clamp(currentPlanarSpeed /
				(std::max)(0.01f, patrolSpeed * burstSpeedMultiplier), 0.0f, 1.0f);
			stepDustTimer = 0.30f - speedFactor * 0.15f;
		}
	}

	void OnUpdate(float dt) override {
		const float safeDt = (std::max)(0.0f, dt);
		if(!defeated) {
			motionElapsed += safeDt;
			UpdateMotionPose();
			return;
		}

		defeatTimer += safeDt;
		const float normalized = defeatDuration > 0.0f
			? std::clamp(defeatTimer / defeatDuration, 0.0f, 1.0f)
			: 1.0f;
		const float impactPhase = std::clamp(normalized / 0.32f, 0.0f, 1.0f);
		const float impactPop = std::sin(impactPhase * DirectX::XM_PI);

		if(auto* t = transform.TryGet()) {
			t->scale = Vector3(
				baseScale.x * (1.0f + impactPop * 0.62f + normalized * 0.62f),
				baseScale.y * (std::max)(0.0f, 1.0f - impactPop * 0.68f - normalized),
				baseScale.z * (1.0f + impactPop * 0.62f + normalized * 0.62f));
			t->AddRotationY(safeDt * (4.5f + normalized * 14.0f));
		}
		if(auto* mat = material.TryGet()) {
			const float flash = (1.0f - normalized) * (1.0f - normalized);
			mat->Material.BaseColor.x = baseMaterial.BaseColor.x + (1.0f - baseMaterial.BaseColor.x) * flash;
			mat->Material.BaseColor.y = baseMaterial.BaseColor.y + (0.48f - baseMaterial.BaseColor.y) * flash;
			mat->Material.BaseColor.z = baseMaterial.BaseColor.z + (0.06f - baseMaterial.BaseColor.z) * flash;
			mat->Material.EmissiveColor = float3(1.0f, 0.12f, 0.01f);
			mat->Material.EmissiveIntensity = baseMaterial.EmissiveIntensity + flash * 7.2f;
		}
		if(defeatTimer >= defeatDuration && !destroyQueued) destroyQueued = QueueDestroySelf();
	}

	void OnStop() override {
		if(auto* t = transform.TryGet()) t->scale = baseScale;
		if(auto* mat = material.TryGet()) mat->Material = baseMaterial;
	}

	void OnCollisionEnter(const HitInfo& hit) override {
		HandleContact(hit.other, true);
	}

	void OnCollisionStay(const HitInfo& hit) override {
		HandleContact(hit.other, false);
	}

private:
	enum class MotionState {
		Patrol,
		TurnPause,
		Windup,
		Burst,
		Recover
	};

	void ValidateSettings() {
		patrolDistance = (std::max)(0.35f, patrolDistance);
		patrolSpeed = (std::max)(0.10f, patrolSpeed);
		movementStyle = std::clamp(movementStyle, -1, 2);
		movementAcceleration = std::clamp(movementAcceleration, 1.0f, 40.0f);
		turnApproachDistance = std::clamp(turnApproachDistance, 0.10f, patrolDistance);
		turnPauseSeconds = std::clamp(turnPauseSeconds, 0.05f, 0.80f);
		burstInterval = std::clamp(burstInterval, 0.60f, 8.0f);
		burstWindupSeconds = std::clamp(burstWindupSeconds, 0.10f, 0.90f);
		burstDuration = std::clamp(burstDuration, 0.10f, 0.80f);
		burstSpeedMultiplier = std::clamp(burstSpeedMultiplier, 1.20f, 4.0f);
		awarenessDistance = std::clamp(awarenessDistance, 1.0f, 14.0f);
		awarenessCorridorWidth = std::clamp(awarenessCorridorWidth, 0.5f, 6.0f);
		reactionCooldown = std::clamp(reactionCooldown, 0.5f, 5.0f);
		stompHeightMargin = std::clamp(stompHeightMargin, 0.08f, 0.18f);
		defeatDuration = (std::max)(0.20f, defeatDuration);
	}

	int ResolveMovementStyle() const {
		if(movementStyle >= 0) return movementStyle;
		ComponentRef<NameComponent> name(m_ref.GetEntityID(), m_ref.GetScene());
		if(const auto* component = name.TryGet()) {
			if(component->name.find("Meadow") != std::string::npos) return 0;
			if(component->name.find("Orchard") != std::string::npos) return 1;
			if(component->name.find("Ruins") != std::string::npos) return 2;

			uint32_t hash = 2166136261u;
			for(unsigned char value : component->name) {
				hash ^= static_cast<uint32_t>(value);
				hash *= 16777619u;
			}
			return static_cast<int>(hash % 3u);
		}
		return 0;
	}

	float DeterministicPhase() const {
		const uint32_t index = m_ref.GetEntityID().GetIndex();
		return static_cast<float>((index * 37u) % 101u) / 100.0f;
	}

	void TickPatrol(float dt, float signedDistance) {
		const float distanceToEdge = DistanceAvailable(direction, signedDistance);
		if(distanceToEdge <= 0.025f) {
			BeginTurnPause();
			ApplyPlanarVelocity(Vector3(), movementAcceleration * 2.4f, dt);
			return;
		}

		if(resolvedMovementStyle == 1 && burstTimer <= 0.0f &&
		   DistanceAvailable(direction, signedDistance) >= turnApproachDistance + 0.75f) {
			BeginWindup(direction);
			ApplyPlanarVelocity(Vector3(), movementAcceleration * 2.8f, dt);
			return;
		}

		if(resolvedMovementStyle == 2 && reactionTimer <= 0.0f) {
			float reactionDirection = direction;
			if(TryResolvePlayerReaction(signedDistance, reactionDirection)) {
				BeginWindup(reactionDirection);
				ApplyPlanarVelocity(Vector3(), movementAcceleration * 2.8f, dt);
				return;
			}
		}

		const float edgeScale = SmoothStep01(distanceToEdge /
			(std::max)(0.05f, turnApproachDistance));
		const float styleScale = resolvedMovementStyle == 1 ? 0.72f : 1.0f;
		const float minimumScale = distanceToEdge < turnApproachDistance ? 0.18f : 1.0f;
		const float speedScale = styleScale * (std::max)(minimumScale, edgeScale);
		const Vector3 desired = patrolAxis * (direction * patrolSpeed * speedScale);
		ApplyPlanarVelocity(desired, movementAcceleration, dt);
	}

	void TickTurnPause(float dt) {
		stateTimer -= dt;
		ApplyPlanarVelocity(Vector3(), movementAcceleration * 3.0f, dt);
		if(stateTimer > 0.0f) return;
		motionState = MotionState::Patrol;
		stateTimer = 0.0f;
	}

	void TickWindup(float dt, float signedDistance) {
		stateTimer -= dt;
		ApplyPlanarVelocity(Vector3(), movementAcceleration * 3.5f, dt);
		if(stateTimer > 0.0f) return;

		if(DistanceAvailable(pendingDirection, signedDistance) <= 0.20f) {
			direction = pendingDirection;
			BeginTurnPause();
			return;
		}

		direction = pendingDirection;
		motionState = MotionState::Burst;
		stateTimer = burstDuration;
		EmitBurstStart();
	}

	void TickBurst(float dt, float signedDistance) {
		const float remaining = DistanceAvailable(direction, signedDistance);
		if(remaining <= 0.12f) {
			BeginTurnPause();
			ApplyPlanarVelocity(Vector3(), movementAcceleration * 4.0f, dt);
			return;
		}

		stateTimer -= dt;
		const float edgeScale = std::clamp(remaining / 0.32f, 0.25f, 1.0f);
		const Vector3 desired = patrolAxis *
			(direction * patrolSpeed * burstSpeedMultiplier * edgeScale);
		ApplyPlanarVelocity(desired, movementAcceleration * 4.5f, dt);
		if(stateTimer > 0.0f) return;

		motionState = MotionState::Recover;
		stateTimer = 0.24f;
		burstTimer = burstInterval;
		reactionTimer = reactionCooldown;
	}

	void TickRecover(float dt) {
		stateTimer -= dt;
		const Vector3 desired = patrolAxis * (direction * patrolSpeed * 0.48f);
		ApplyPlanarVelocity(desired, movementAcceleration * 1.6f, dt);
		if(stateTimer > 0.0f) return;
		motionState = MotionState::Patrol;
		stateTimer = 0.0f;
	}

	void BeginTurnPause() {
		const float oldDirection = direction;
		direction *= -1.0f;
		motionState = MotionState::TurnPause;
		stateTimer = turnPauseSeconds;
		burstTimer = (std::max)(burstTimer, turnPauseSeconds + 0.25f);
		reactionTimer = (std::max)(reactionTimer, turnPauseSeconds + 0.25f);
		if(turnDustEnabled && turnDustCooldown <= 0.0f) {
			EmitTurnDust(oldDirection);
			turnDustCooldown = 0.18f;
		}
	}

	void BeginWindup(float requestedDirection) {
		pendingDirection = requestedDirection >= 0.0f ? 1.0f : -1.0f;
		motionState = MotionState::Windup;
		stateTimer = burstWindupSeconds;
		EmitWindupDust();
	}

	bool TryResolvePlayerReaction(float signedDistance, float& outDirection) {
		if(!player.IsValid()) {
			player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		}
		if(!player.IsValid()) return false;
		ComponentRef<TransformComponent> playerTransform(player.GetEntityRef());
		auto* playerPose = playerTransform.TryGet();
		auto* enemyPose = transform.TryGet();
		if(!playerPose || !enemyPose) return false;

		const Vector3 toPlayer = playerPose->position - enemyPose->position;
		const Vector3 planar(toPlayer.x, 0.0f, toPlayer.z);
		const float planarDistance = planar.length();
		if(planarDistance > awarenessDistance ||
		   std::abs(toPlayer.y) > 2.2f) {
			return false;
		}

		const float along = (playerPose->position - origin).dot(patrolAxis);
		const Vector3 projected = origin + patrolAxis * along;
		const Vector3 lateralDelta = playerPose->position - projected;
		const float lateralDistance = std::sqrt(
			lateralDelta.x * lateralDelta.x + lateralDelta.z * lateralDelta.z);
		if(lateralDistance > awarenessCorridorWidth) return false;

		const float clampedTarget = std::clamp(
			along,
			-patrolDistance + 0.30f,
			patrolDistance - 0.30f);
		const float difference = clampedTarget - signedDistance;
		if(std::abs(difference) < 0.85f) return false;

		outDirection = difference >= 0.0f ? 1.0f : -1.0f;
		return DistanceAvailable(outDirection, signedDistance) >= 0.55f;
	}

	float DistanceAvailable(float travelDirection, float signedDistance) const {
		return travelDirection >= 0.0f
			? patrolDistance - signedDistance
			: patrolDistance + signedDistance;
	}

	void ApplyPlanarVelocity(
		const Vector3& desired,
		float acceleration,
		float dt
	) {
		Vector3 current = fallbackVelocity;
		if(!collider.IsValid()) collider = GetComponentRef<ColliderComponent>();
		if(auto* col = collider.TryGet()) {
			if(auto* rigid = col->pRigidbodyDynamic) {
				const physx::PxVec3 velocity = rigid->getLinearVelocity();
				current = Vector3(velocity.x, 0.0f, velocity.z);
				const float maxDelta = acceleration * dt;
				const Vector3 next(
					MoveTowards(current.x, desired.x, maxDelta),
					0.0f,
					MoveTowards(current.z, desired.z, maxDelta));
				rigid->setLinearVelocity(physx::PxVec3(next.x, velocity.y, next.z));
				fallbackVelocity = next;
				currentPlanarSpeed = next.length();
				return;
			}
		}

		const float maxDelta = acceleration * dt;
		fallbackVelocity.x = MoveTowards(fallbackVelocity.x, desired.x, maxDelta);
		fallbackVelocity.z = MoveTowards(fallbackVelocity.z, desired.z, maxDelta);
		if(auto* t = transform.TryGet()) t->position += fallbackVelocity * dt;
		currentPlanarSpeed = fallbackVelocity.length();
	}

	void UpdateMotionPose() {
		auto* t = transform.TryGet();
		if(!t || !motionSquashEnabled) return;

		const float speedRatio = std::clamp(
			currentPlanarSpeed / (std::max)(0.01f, patrolSpeed * burstSpeedMultiplier),
			0.0f,
			1.0f);
		const float cadence = 4.0f + currentPlanarSpeed * 2.4f;
		const float bob = std::sin(motionElapsed * cadence) *
			(0.018f + speedRatio * 0.025f);
		float horizontalStretch = speedRatio * 0.055f;
		float verticalScale = 1.0f - horizontalStretch * 0.60f + bob;
		float widthScale = 1.0f + horizontalStretch;

		switch(motionState) {
		case MotionState::TurnPause:
			verticalScale *= 0.92f;
			widthScale *= 1.06f;
			break;
		case MotionState::Windup: {
			const float progress = burstWindupSeconds > 0.0f
				? 1.0f - std::clamp(stateTimer / burstWindupSeconds, 0.0f, 1.0f)
				: 1.0f;
			verticalScale *= 1.0f - progress * 0.18f;
			widthScale *= 1.0f + progress * 0.13f;
			break;
		}
		case MotionState::Burst:
			verticalScale *= 0.90f;
			widthScale *= 1.10f;
			break;
		case MotionState::Recover:
			verticalScale *= 1.04f;
			widthScale *= 0.98f;
			break;
		case MotionState::Patrol:
			break;
		}

		const bool axisIsX = std::abs(patrolAxis.x) >= std::abs(patrolAxis.z);
		t->scale = axisIsX
			? Vector3(baseScale.x * widthScale, baseScale.y * verticalScale,
				baseScale.z * (1.0f + bob * 0.25f))
			: Vector3(baseScale.x * (1.0f + bob * 0.25f), baseScale.y * verticalScale,
				baseScale.z * widthScale);
	}

	void EnsureParticleEmitter() {
		if(!particle.IsValid()) particle = GetComponentRef<ParticleComponent>();
		auto* effect = particle.TryGet();
		if(!effect || particleConfigured) return;
		effect->isLoop = false;
		effect->SpawnInterval = 0.0f;
		effect->SpawnCount = 0;
		effect->SpawnTimer = 0.0f;
		effect->particleLifeTime = 1.20f;
		effect->particleSize = 0.075f;
		for(auto& state : effect->Particle) state.LifeTime = 0.0f;
		particleConfigured = true;
	}

	void AlignColliderToModelCenter() {
		auto* t = transform.TryGet();
		auto* col = collider.TryGet();
		if(!t || !col || col->colliders.empty()) return;

		ColliderShape& shape = col->colliders.front();
		if(shape.type != ColliderType::Box) return;

		const float authoredOffsetY = shape.offset.y;
		if(std::abs(authoredOffsetY) <= 0.0001f) return;

		const float worldLift = authoredOffsetY * t->scale.y;
		visualCenterLift = worldLift;
		t->position.y += worldLift;
		shape.offset.y = 0.0f;

		if(shape.pxShape) {
			physx::PxTransform localPose = shape.pxShape->getLocalPose();
			localPose.p.y = 0.0f;
			shape.pxShape->setLocalPose(localPose);
		}

		if(auto* rigid = col->pRigidbodyDynamic) {
			physx::PxTransform actorPose = rigid->getGlobalPose();
			actorPose.p.y += worldLift;
			rigid->setGlobalPose(actorPose, true);
		}
		col->needsUpdate = true;
	}

	void EmitStepDust() {
		auto* effect = particle.TryGet();
		if(!effect) return;
		effect->particleSize = 0.060f;
		PlatformerFeedback::DirectionalBurst(
			effect,
			Vector3(0.0f, -0.36f, 0.0f),
			patrolAxis * -direction,
			6,
			1.15f,
			1.0f,
			0.65f,
			0.30f,
			DirectX::XMFLOAT4(0.72f, 0.62f, 0.46f, 0.72f),
			DirectX::XMFLOAT4(0.42f, 0.34f, 0.24f, 0.62f));
	}

	void EmitTurnDust(float oldDirection) {
		auto* effect = particle.TryGet();
		if(!effect) return;
		effect->particleSize = 0.075f;
		PlatformerFeedback::DirectionalBurst(
			effect,
			Vector3(0.0f, -0.35f, 0.0f),
			patrolAxis * -oldDirection,
			20,
			2.0f,
			1.7f,
			1.1f,
			0.40f,
			DirectX::XMFLOAT4(0.88f, 0.60f, 0.28f, 0.88f),
			DirectX::XMFLOAT4(0.42f, 0.26f, 0.12f, 0.74f));
	}

	void EmitWindupDust() {
		auto* effect = particle.TryGet();
		if(!effect) return;
		effect->particleSize = 0.070f;
		PlatformerFeedback::DirectionalBurst(
			effect,
			Vector3(0.0f, -0.34f, 0.0f),
			patrolAxis * -pendingDirection,
			10,
			1.5f,
			1.35f,
			0.85f,
			burstWindupSeconds + 0.12f,
			DirectX::XMFLOAT4(0.78f, 0.66f, 0.46f, 0.78f),
			DirectX::XMFLOAT4(0.48f, 0.34f, 0.20f, 0.70f));
	}

	void EmitBurstStart() {
		auto* effect = particle.TryGet();
		if(!effect) return;
		effect->particleSize = 0.080f;
		PlatformerFeedback::DirectionalBurst(
			effect,
			Vector3(0.0f, -0.32f, 0.0f),
			patrolAxis * -direction,
			18,
			3.2f,
			2.6f,
			1.5f,
			0.48f,
			DirectX::XMFLOAT4(0.92f, 0.70f, 0.34f, 0.94f),
			DirectX::XMFLOAT4(0.48f, 0.24f, 0.08f, 0.82f));
	}

	void HandleContact(const EntityRef& other, bool entered) {
		(void)entered;
		if(defeated || !other.IsValid()) return;
		ComponentRef<PlatformerCharacterController> contactedPlayer(other);
		auto* controller = contactedPlayer.TryGet();
		if(!controller) return;

		auto* enemyTransform = transform.TryGet();
		ComponentRef<TransformComponent> playerTransform(other);
		auto* playerPose = playerTransform.TryGet();
		if(!enemyTransform || !playerPose) return;

		const float stompReferenceY = enemyTransform->position.y - visualCenterLift;
		const float verticalVelocity = controller->GetVerticalVelocity();
		const bool descendingOrNearApex = verticalVelocity <= 0.65f;
		const bool above = playerPose->position.y >= stompReferenceY + stompHeightMargin;
		const float dx = playerPose->position.x - enemyTransform->position.x;
		const float dz = playerPose->position.z - enemyTransform->position.z;
		const float horizontalDistance = std::sqrt(dx * dx + dz * dz);
		const float catchRadius = (std::max)(
			0.72f, (std::max)(baseScale.x, baseScale.z) * 0.72f);
		if(descendingOrNearApex && above && horizontalDistance <= catchRadius) {
			Defeat(*controller);
			return;
		}

		controller->ApplyDamage(enemyTransform->position);
	}

	void Defeat(PlatformerCharacterController& stompPlayer) {
		if(defeated) return;
		EnsureParticleEmitter();
		defeated = true;
		defeatTimer = 0.0f;
		stompPlayer.ApplyStompBounce();
		if(auto* effect = particle.TryGet()) {
			for(auto& state : effect->Particle) state.LifeTime = 0.0f;
			effect->particleSize = 0.19f;
			PlatformerFeedback::LayeredBurst(
				effect,
				Vector3(0.0f, 0.45f, 0.0f),
				144,
				8.2f,
				9.4f,
				defeatDuration + 0.72f,
				DirectX::XMFLOAT4(1.0f, 0.42f, 0.04f, 1.0f),
				DirectX::XMFLOAT4(0.72f, 0.02f, 0.02f, 1.0f));
		}
		PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene());
		if(!camera.IsValid()) {
			camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		}
		if(auto* cameraController = camera.TryGet()) {
			cameraController->AddImpulse(
				0.48f,
				0.26f,
				0.060f,
				Vector3(0.0f, -1.0f, 0.0f));
		}

		if(auto* col = collider.TryGet()) {
			if(auto* rigid = col->pRigidbodyDynamic) {
				rigid->setLinearVelocity(physx::PxVec3(0.0f));
				rigid->setActorFlag(physx::PxActorFlag::eDISABLE_SIMULATION, true);
			}
		}
	}

	static float MoveTowards(float current, float target, float maxDelta) {
		if(current < target) return (std::min)(current + maxDelta, target);
		if(current > target) return (std::max)(current - maxDelta, target);
		return target;
	}

	static float SmoothStep01(float value) {
		value = std::clamp(value, 0.0f, 1.0f);
		return value * value * (3.0f - 2.0f * value);
	}

	static const char* MovementStyleName(int style) {
		switch(style) {
		case 1: return "Burst Patrol";
		case 2: return "Reactive Guard";
		default: return "Smooth Patrol";
		}
	}

	static const char* MotionStateName(MotionState state) {
		switch(state) {
		case MotionState::TurnPause: return "Turn Pause";
		case MotionState::Windup: return "Windup";
		case MotionState::Burst: return "Burst";
		case MotionState::Recover: return "Recover";
		default: return "Patrol";
		}
	}

	ComponentRef<TransformComponent> transform;
	ComponentRef<ColliderComponent> collider;
	ComponentRef<MaterialComponent> material;
	ComponentRef<ParticleComponent> particle;
	ComponentRef<AudioComponent> audio;
	ComponentRef<PlatformerCameraController> camera;
	ComponentRef<PlatformerCharacterController> player;
	Vector3 origin;
	Vector3 baseScale = Vector3(1.0f, 1.0f, 1.0f);
	Vector3 fallbackVelocity;
	MATERIAL baseMaterial{};
	float visualCenterLift = 0.0f;
	float direction = 1.0f;
	float pendingDirection = 1.0f;
	float currentPlanarSpeed = 0.0f;
	float stateTimer = 0.0f;
	float burstTimer = 0.0f;
	float reactionTimer = 0.0f;
	float stepDustTimer = 0.0f;
	float motionElapsed = 0.0f;
	float defeatTimer = 0.0f;
	float turnDustCooldown = 0.0f;
	int resolvedMovementStyle = 0;
	MotionState motionState = MotionState::Patrol;
	bool particleConfigured = false;
	bool defeated = false;
	bool destroyQueued = false;
};