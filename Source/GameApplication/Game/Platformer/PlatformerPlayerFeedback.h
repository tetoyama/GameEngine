#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/ColliderComponent.h"
#include "Engine/Scene/Component/CameraComponent.h"
#include "Engine/Scene/Component/audioComponent.h"
#include "Engine/Scene/Component/particleComponent.h"
#include "Game/Platformer/PlatformerCameraController.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerFeedback.h"
#include "Game/Platformer/PlatformerSceneAccess.h"
#include "Game/Platformer/PlatformerSoundLibrary.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

class PlatformerPlayerFeedback : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerPlayerFeedback)
		REFLECT_FIELD(float, particleVerticalOffset, 0.45f)
		REFLECT_FIELD(bool, movementAssistEnabled, true)
		REFLECT_FIELD(float, assistBaseRunSpeed, 7.0f)
		REFLECT_FIELD(float, assistRunSpeedBonus, 1.10f)
		REFLECT_FIELD(float, assistBuildSeconds, 0.65f)
		REFLECT_FIELD(float, assistLowSpeedAcceleration, 14.0f)
		REFLECT_FIELD(float, assistHighSpeedAcceleration, 43.0f)
		REFLECT_FIELD(float, assistReverseMultiplier, 1.75f)
		REFLECT_FIELD(float, assistAirAcceleration, 18.0f)
		REFLECT_FIELD(float, assistFirstJumpBoost, 0.28f)
		REFLECT_FIELD(float, assistSecondJumpBoost, 0.58f)
		REFLECT_FIELD(float, assistThirdJumpBoost, 0.95f)
		REFLECT_FIELD(float, assistLandingCarryBoost, 0.24f)
		REFLECT_FIELD(bool, stepAssistEnabled, true)
		REFLECT_FIELD(float, stepAssistHeight, 0.42f)
		REFLECT_FIELD(float, stepAssistProbeDistance, 0.52f)
		REFLECT_FIELD(float, stepAssistForwardOffset, 0.04f)
		REFLECT_FIELD(float, stepAssistCooldown, 0.035f)
		REFLECT_FIELD(float, stepAssistGroundGrace, 0.10f)
		REFLECT_FIELD(float, stepAssistSideProbeScale, 0.62f)
		REFLECT_FIELD(float, stepAssistTopInset, 0.12f)
		REFLECT_FIELD(float, stepAssistForwardNudge, 0.035f)
		REFLECT_FIELD(float, stepAssistHeightTolerance, 0.08f)
		REFLECT_FIELD(bool, stepAssistFeedbackEnabled, true)

public:
	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		const bool legacyStepAssist = !node["stepAssistGroundGrace"];
		DECODE_FIELDS(node);
		if(legacyStepAssist) {
			// Existing scenes used the original centre-ray implementation. Promote its
			// conservative values to the safer wide-probe defaults on first load.
			stepAssistHeight = (std::max)(stepAssistHeight, 0.38f);
			stepAssistProbeDistance = (std::max)(stepAssistProbeDistance, 0.48f);
			stepAssistForwardOffset = (std::max)(stepAssistForwardOffset, 0.035f);
			stepAssistCooldown = (std::min)(stepAssistCooldown, 0.04f);
		}
		ValidateAssistSettings();
		return true;
	}

	void inspector(SceneContext* context) override {
		ImGui::Text("Platformer Player Feedback");
		INSPECTOR_FIELDS();
		ImGui::Text("Run Assist: %.2f", runAssistNormalized);
		ImGui::Text("Step Cooldown: %.3f", stepAssistTimer);
		ImGui::Text("Step Ground Grace: %.3f", stepAssistGroundTimer);
		ImGui::Text("Last Step Rise: %.3f", lastStepRise);
	}

	void OnStart() override {
		ValidateAssistSettings();
		player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		ResolvePlayerRuntimeRefs();
		ResolveCameraTransform();
		transform = GetComponentRef<TransformComponent>();
		particle = GetComponentRef<ParticleComponent>();
		audio = GetComponentRef<AudioComponent>();
		stepAssistTimer = 0.0f;
		stepAssistGroundTimer = 0.0f;
		lastStepRise = 0.0f;
		if(auto* controller = player.TryGet()) {
			CaptureRevisions(*controller);
			previousVerticalVelocity = controller->GetVerticalVelocity();
		}
	}

	void OnFixedUpdate(float dt) override {
		if(dt <= 0.0f) return;
		ValidateAssistSettings();
		stepAssistTimer = (std::max)(0.0f, stepAssistTimer - dt);
		if(!player.IsValid()) player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		auto* controller = player.TryGet();
		ComponentRef<TransformComponent> playerTransform(player.GetEntityRef());
		auto* playerPose = playerTransform.TryGet();
		if(!controller || !playerPose) return;

		if(controller->IsGrounded()) stepAssistGroundTimer = stepAssistGroundGrace;
		else stepAssistGroundTimer = (std::max)(0.0f, stepAssistGroundTimer - dt);

		ResolvePlayerRuntimeRefs();
		ApplyMovementAssist(*controller, dt);
		ApplyStepAssist(*controller, *playerPose);

		const Vector3 origin = playerPose->position + Vector3(0.0f, particleVerticalOffset, 0.0f);

		if(controller->GetJumpEventRevision() != jumpRevision) {
			jumpRevision = controller->GetJumpEventRevision();
			const int stage = controller->GetJumpStage();
			ApplyJumpMomentum(stage);
			if(stage >= 3) {
				EmitAt(origin, 44, 4.8f, 6.2f, 0.82f, 0.18f);
				Impulse(0.24f, 0.20f, 0.040f, Vector3(0.0f, 1.0f, 0.0f));
			} else if(stage == 2) {
				EmitAt(origin, 26, 3.2f, 4.3f, 0.58f, 0.145f);
				Impulse(0.12f, 0.13f, 0.018f, Vector3(0.0f, 1.0f, 0.0f));
			} else {
				EmitAt(origin, 16, 2.3f, 3.1f, 0.42f, 0.12f);
				Impulse(0.065f, 0.10f, 0.008f, Vector3(0.0f, 1.0f, 0.0f));
			}
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ActionPath);
		}

		if(controller->GetLandEventRevision() != landRevision) {
			landRevision = controller->GetLandEventRevision();
			ApplyLandingMomentum();
			const float impactSpeed = std::abs(previousVerticalVelocity);
			const float impact = std::clamp((impactSpeed - 1.5f) / 10.0f, 0.25f, 1.0f);
			const int count = 16 + static_cast<int>(impact * 24.0f);
			EmitAt(playerPose->position, count, 2.4f + impact * 2.4f, 1.0f + impact * 1.1f, 0.38f + impact * 0.18f, 0.12f + impact * 0.05f);
			Impulse(0.08f + impact * 0.20f, 0.11f + impact * 0.09f, -0.008f - impact * 0.015f, Vector3(0.0f, -1.0f, 0.0f));
		}

		if(controller->GetWallKickEventRevision() != wallRevision) {
			wallRevision = controller->GetWallKickEventRevision();
			runAssistTimer = (std::max)(runAssistTimer, assistBuildSeconds * 0.35f);
			const Vector3 wallNormal = controller->GetLastWallNormal();
			const Vector3 wallOrigin = origin - wallNormal * 0.3f;
			EmitAt(wallOrigin, 34, 4.2f, 5.0f, 0.68f, 0.165f);
			Impulse(0.21f, 0.17f, 0.025f, wallNormal);
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ActionPath);
		}

		if(controller->GetStompEventRevision() != stompRevision) {
			stompRevision = controller->GetStompEventRevision();
			ApplyHorizontalBoost(0.30f);
			EmitAt(playerPose->position, 52, 5.2f, 6.4f, 0.82f, 0.20f);
			Impulse(0.38f, 0.23f, 0.050f, Vector3(0.0f, -1.0f, 0.0f));
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ImpactPath);
		}

		if(controller->GetDamageEventRevision() != damageRevision) {
			damageRevision = controller->GetDamageEventRevision();
			runAssistTimer = 0.0f;
			stepAssistTimer = stepAssistCooldown;
			stepAssistGroundTimer = 0.0f;
			EmitAt(origin, 56, 5.4f, 5.2f, 0.88f, 0.21f);
			Impulse(0.48f, 0.28f, 0.060f);
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ImpactPath);
		}

		if(controller->GetRespawnEventRevision() != respawnRevision) {
			respawnRevision = controller->GetRespawnEventRevision();
			runAssistTimer = 0.0f;
			stepAssistTimer = stepAssistCooldown;
			stepAssistGroundTimer = 0.0f;
			EmitAt(origin, 46, 4.2f, 6.6f, 1.05f, 0.17f);
			Impulse(0.18f, 0.30f, 0.022f, Vector3(0.0f, 1.0f, 0.0f));
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::CheckpointPath);
		}

		previousVerticalVelocity = controller->GetVerticalVelocity();
	}

private:
	struct StepCapsuleMetrics {
		float radius = 0.25f;
		float footY = 0.0f;
		float headY = 1.5f;
	};

	struct StepTopCandidate {
		RayHit hit{};
		float rise = 0.0f;
		int lane = 0;
		bool valid = false;
	};

	void ValidateAssistSettings() {
		assistBaseRunSpeed = (std::max)(0.1f, assistBaseRunSpeed);
		assistRunSpeedBonus = (std::max)(0.0f, assistRunSpeedBonus);
		assistBuildSeconds = (std::max)(0.05f, assistBuildSeconds);
		assistLowSpeedAcceleration = (std::max)(0.0f, assistLowSpeedAcceleration);
		assistHighSpeedAcceleration = (std::max)(0.0f, assistHighSpeedAcceleration);
		assistReverseMultiplier = (std::max)(1.0f, assistReverseMultiplier);
		assistAirAcceleration = (std::max)(0.0f, assistAirAcceleration);
		assistFirstJumpBoost = (std::max)(0.0f, assistFirstJumpBoost);
		assistSecondJumpBoost = (std::max)(0.0f, assistSecondJumpBoost);
		assistThirdJumpBoost = (std::max)(0.0f, assistThirdJumpBoost);
		assistLandingCarryBoost = (std::max)(0.0f, assistLandingCarryBoost);
		stepAssistHeight = std::clamp(stepAssistHeight, 0.08f, 0.48f);
		stepAssistProbeDistance = std::clamp(stepAssistProbeDistance, 0.25f, 0.75f);
		stepAssistForwardOffset = std::clamp(stepAssistForwardOffset, 0.0f, 0.18f);
		stepAssistCooldown = std::clamp(stepAssistCooldown, 0.015f, 0.12f);
		stepAssistGroundGrace = std::clamp(stepAssistGroundGrace, 0.02f, 0.18f);
		stepAssistSideProbeScale = std::clamp(stepAssistSideProbeScale, 0.35f, 0.90f);
		stepAssistTopInset = std::clamp(stepAssistTopInset, 0.06f, 0.24f);
		stepAssistForwardNudge = std::clamp(stepAssistForwardNudge, 0.0f, 0.08f);
		stepAssistHeightTolerance = std::clamp(stepAssistHeightTolerance, 0.025f, 0.12f);
	}

	void ResolvePlayerRuntimeRefs() {
		if(!player.IsValid()) return;
		playerCollider = ComponentRef<ColliderComponent>(player.GetEntityRef());
	}

	void ResolveCameraTransform() {
		auto cameraComponent = PlatformerSceneAccess::FindFirst<CameraComponent>(m_ref.GetScene());
		cameraTransform = cameraComponent.IsValid()
			? ComponentRef<TransformComponent>(cameraComponent.GetEntityRef())
			: ComponentRef<TransformComponent>{};
	}

	static bool IsStaticSolidHit(const RayHit& hit) {
		if(!hit.hit) return false;
		if(hit.hitShape && hit.hitShape->getFlags().isSet(physx::PxShapeFlag::eTRIGGER_SHAPE)) return false;
		return !hit.hitActor || hit.hitActor->getType() != physx::PxActorType::eRIGID_DYNAMIC;
	}

	static physx::PxU32 ResolveActorLayerMask(physx::PxRigidDynamic* rigid) {
		if(!rigid || rigid->getNbShapes() == 0) return 1u;
		physx::PxShape* shape = nullptr;
		if(rigid->getShapes(&shape, 1) == 0 || !shape) return 1u;
		const physx::PxU32 mask = shape->getQueryFilterData().word0;
		return mask != 0u ? mask : 1u;
	}

	static StepCapsuleMetrics ResolveCapsuleMetrics(
		const ColliderComponent& colliderComponent,
		const TransformComponent& playerPose
	) {
		StepCapsuleMetrics metrics;
		metrics.footY = playerPose.position.y;
		metrics.headY = playerPose.position.y + 1.5f;

		for(const ColliderShape& shape : colliderComponent.colliders) {
			if(shape.type != ColliderType::Capsule) continue;
			const float scaleX = std::abs(playerPose.scale.x);
			const float scaleY = std::abs(playerPose.scale.y);
			const float scaleZ = std::abs(playerPose.scale.z);
			const float conservativeScale = (std::max)(0.0001f, (std::max)(scaleX, (std::max)(scaleY, scaleZ)));
			const float verticalScale = (std::max)(0.0001f, scaleY);
			metrics.radius = (std::max)(0.05f, shape.radius * conservativeScale);
			const float capsuleHalfHeight = shape.height * 0.5f * conservativeScale + metrics.radius;
			const float centerY = playerPose.position.y + shape.offset.y * verticalScale;
			metrics.footY = centerY - capsuleHalfHeight;
			metrics.headY = centerY + capsuleHalfHeight;
			break;
		}
		return metrics;
	}

	static bool IsStepFace(const RayHit& hit, const Vector3& direction) {
		if(!IsStaticSolidHit(hit) || std::abs(hit.normal.y) > 0.48f) return false;
		Vector3 horizontalNormal(hit.normal.x, 0.0f, hit.normal.z);
		if(horizontalNormal.length() <= 0.0001f) return false;
		horizontalNormal = horizontalNormal.normalize();
		return horizontalNormal.dot(direction) <= -0.12f;
	}

	bool HasStepHeadClearance(
		PlatformerPhysicsProbe& physics,
		physx::PxU32 selfMask,
		const StepCapsuleMetrics& metrics,
		const Vector3& currentPosition,
		const Vector3& targetPosition,
		const Vector3& side,
		float sideOffset,
		float rise
	) const {
		const float distance = rise + 0.065f;
		const Vector3 bases[2] = { currentPosition, targetPosition };
		const float lanes[3] = { 0.0f, -sideOffset, sideOffset };
		for(const Vector3& base : bases) {
			for(float lane : lanes) {
				const Vector3 sample = base + side * lane;
				const RayHit hit = physics.RaycastWithMask(
					physx::PxVec3(sample.x, metrics.headY + 0.015f, sample.z),
					physx::PxVec3(0.0f, 1.0f, 0.0f),
					distance,
					selfMask);
				if(IsStaticSolidHit(hit)) return false;
			}
		}
		return true;
	}

	bool ResolveStepTop(
		PlatformerPhysicsProbe& physics,
		physx::PxU32 selfMask,
		const StepCapsuleMetrics& metrics,
		const Vector3& sampleCenter,
		const Vector3& side,
		float sideOffset,
		float& outRise
	) const {
		const float topLift = (std::max)(0.12f, metrics.radius * 0.40f);
		const float rayStartY = metrics.footY + stepAssistHeight + topLift;
		const float rayDistance = stepAssistHeight + topLift + 0.10f;
		const float minNormalY = std::cos(50.0f * DirectX::XM_PI / 180.0f);
		const float lanes[3] = { 0.0f, -sideOffset, sideOffset };
		StepTopCandidate candidates[3];

		for(int i = 0; i < 3; ++i) {
			const Vector3 sample = sampleCenter + side * lanes[i];
			const RayHit hit = physics.RaycastWithMask(
				physx::PxVec3(sample.x, rayStartY, sample.z),
				physx::PxVec3(0.0f, -1.0f, 0.0f),
				rayDistance,
				selfMask);
			const float rise = hit.position.y - metrics.footY;
			const bool valid = IsStaticSolidHit(hit) &&
				hit.normal.y >= minNormalY &&
				rise > 0.018f &&
				rise <= stepAssistHeight + 0.025f;
			candidates[i].hit = hit;
			candidates[i].rise = rise;
			candidates[i].lane = i;
			candidates[i].valid = valid;
		}

		int bestCount = 0;
		int bestCenterSupport = 0;
		float bestSum = 0.0f;
		for(int reference = 0; reference < 3; ++reference) {
			if(!candidates[reference].valid) continue;
			int count = 0;
			int centerSupport = 0;
			float sum = 0.0f;
			for(int candidate = 0; candidate < 3; ++candidate) {
				if(!candidates[candidate].valid) continue;
				if(std::abs(candidates[candidate].rise - candidates[reference].rise) > stepAssistHeightTolerance) continue;
				++count;
				if(candidate == 0) centerSupport = 1;
				sum += candidates[candidate].rise;
			}
			if(count > bestCount || (count == bestCount && centerSupport > bestCenterSupport)) {
				bestCount = count;
				bestCenterSupport = centerSupport;
				bestSum = sum;
			}
		}

		// Requiring two coherent support samples prevents climbing isolated corners,
		// rails and decorative spikes while still accepting diagonal stair entry.
		if(bestCount < 2) return false;
		outRise = bestSum / static_cast<float>(bestCount);
		return true;
	}

	void ApplyStepAssist(
		const PlatformerCharacterController& controller,
		TransformComponent& playerPose
	) {
		if(!stepAssistEnabled || stepAssistTimer > 0.0f || stepAssistGroundTimer <= 0.0f ||
		   !controller.IsControlEnabled()) {
			return;
		}
		const float verticalVelocity = controller.GetVerticalVelocity();
		if(verticalVelocity > 0.65f || verticalVelocity < -1.25f) return;

		Vector3 inputDirection = BuildCameraRelativeInput();
		inputDirection.y = 0.0f;
		if(inputDirection.length() <= 0.0001f) return;
		inputDirection = inputDirection.normalize();

		auto* colliderComponent = playerCollider.TryGet();
		auto* rigid = colliderComponent ? colliderComponent->pRigidbodyDynamic : nullptr;
		auto* physics = PlatformerSceneAccess::Physics(m_ref.GetScene());
		if(!colliderComponent || !rigid || !physics) return;

		const physx::PxVec3 rigidVelocity = rigid->getLinearVelocity();
		Vector3 horizontalVelocity(rigidVelocity.x, 0.0f, rigidVelocity.z);
		const float horizontalSpeed = horizontalVelocity.length();
		if(horizontalSpeed < 0.35f) return;
		horizontalVelocity = horizontalVelocity.normalize();
		if(horizontalVelocity.dot(inputDirection) < 0.15f) return;

		Vector3 direction = inputDirection * 0.72f + horizontalVelocity * 0.28f;
		if(direction.length() <= 0.0001f) return;
		direction = direction.normalize();
		const Vector3 side(-direction.z, 0.0f, direction.x);

		const StepCapsuleMetrics metrics = ResolveCapsuleMetrics(*colliderComponent, playerPose);
		const float sideOffset = std::clamp(
			metrics.radius * stepAssistSideProbeScale,
			0.07f,
			0.22f);
		const float lowerProbeHeight = std::clamp(stepAssistHeight * 0.28f, 0.055f, 0.115f);
		const float startOffset = stepAssistForwardOffset + metrics.radius * 0.12f;
		const float faceDistance = stepAssistProbeDistance + metrics.radius * 0.25f;
		const Vector3 faceBase(
			playerPose.position.x + direction.x * startOffset,
			metrics.footY + lowerProbeHeight,
			playerPose.position.z + direction.z * startOffset);
		const float lanes[3] = { 0.0f, -sideOffset, sideOffset };
		const physx::PxU32 selfMask = ResolveActorLayerMask(rigid);

		RayHit nearestFace{};
		float nearestDistance = faceDistance + 1.0f;
		for(float lane : lanes) {
			const Vector3 origin = faceBase + side * lane;
			const RayHit hit = physics->RaycastWithMask(
				physx::PxVec3(origin.x, origin.y, origin.z),
				physx::PxVec3(direction.x, 0.0f, direction.z),
				faceDistance,
				selfMask);
			if(!IsStepFace(hit, direction) || hit.distance >= nearestDistance) continue;
			nearestFace = hit;
			nearestDistance = hit.distance;
		}
		if(nearestDistance > faceDistance) return;

		// The entire capsule width must pass above the candidate face. A single clear
		// centre ray is not enough when approaching a wall or stair corner diagonally.
		const float upperY = metrics.footY + stepAssistHeight + (std::max)(0.075f, metrics.radius * 0.32f);
		const float upperDistance = (std::min)(faceDistance, nearestDistance + metrics.radius * 0.95f);
		for(float lane : lanes) {
			const Vector3 origin = Vector3(faceBase.x, upperY, faceBase.z) + side * lane;
			const RayHit hit = physics->RaycastWithMask(
				physx::PxVec3(origin.x, origin.y, origin.z),
				physx::PxVec3(direction.x, 0.0f, direction.z),
				upperDistance,
				selfMask);
			if(IsStaticSolidHit(hit)) return;
		}

		const float inset = (std::max)(stepAssistTopInset, metrics.radius * 0.58f);
		const Vector3 topSampleCenter(
			faceBase.x + direction.x * (nearestDistance + inset),
			metrics.footY,
			faceBase.z + direction.z * (nearestDistance + inset));
		float rise = 0.0f;
		if(!ResolveStepTop(*physics, selfMask, metrics, topSampleCenter, side, sideOffset * 0.88f, rise)) return;

		const Vector3 currentPosition = playerPose.position;
		Vector3 targetPosition = currentPosition;
		targetPosition.y += rise + 0.010f;
		targetPosition += direction * stepAssistForwardNudge;
		if(!HasStepHeadClearance(
			*physics,
			selfMask,
			metrics,
			currentPosition,
			targetPosition,
			side,
			sideOffset,
			rise)) {
			return;
		}

		playerPose.position = targetPosition;
		physx::PxTransform actorPose = rigid->getGlobalPose();
		actorPose.p = physx::PxVec3(targetPosition.x, targetPosition.y, targetPosition.z);
		rigid->setGlobalPose(actorPose, true);
		const float safeVertical = std::clamp(rigidVelocity.y, 0.0f, 0.25f);
		rigid->setLinearVelocity(physx::PxVec3(rigidVelocity.x, safeVertical, rigidVelocity.z));
		rigid->wakeUp();

		lastStepRise = rise;
		stepAssistTimer = stepAssistCooldown;
		stepAssistGroundTimer = stepAssistGroundGrace;
		if(stepAssistFeedbackEnabled) {
			EmitStepFeedback(
				Vector3(targetPosition.x, metrics.footY + rise + 0.025f, targetPosition.z),
				direction);
		}
	}

	void EmitStepFeedback(const Vector3& worldOrigin, const Vector3& direction) {
		auto* feedbackPose = transform.TryGet();
		auto* feedbackParticle = particle.TryGet();
		if(!feedbackPose || !feedbackParticle) return;
		for(auto& state : feedbackParticle->Particle) state.LifeTime = 0.0f;
		feedbackPose->position = worldOrigin;
		feedbackPose->scale = Vector3(1.0f, 1.0f, 1.0f);
		feedbackParticle->particleSize = 0.050f;
		PlatformerFeedback::DirectionalBurst(
			feedbackParticle,
			Vector3(),
			direction * -1.0f,
			12,
			1.2f,
			1.4f,
			0.75f,
			0.34f,
			DirectX::XMFLOAT4(0.72f, 0.62f, 0.42f, 1.0f),
			DirectX::XMFLOAT4(0.28f, 0.72f, 1.0f, 1.0f));
	}

	void ApplyMovementAssist(const PlatformerCharacterController& controller, float dt) {
		if(!movementAssistEnabled || !controller.IsControlEnabled()) {
			runAssistTimer = (std::max)(0.0f, runAssistTimer - dt * 2.4f);
			runAssistNormalized = runAssistTimer / assistBuildSeconds;
			return;
		}

		const Vector3 inputDirection = BuildCameraRelativeInput();
		if(inputDirection.length() <= 0.0001f) {
			runAssistTimer = (std::max)(0.0f, runAssistTimer - dt * 1.8f);
			runAssistNormalized = runAssistTimer / assistBuildSeconds;
			return;
		}

		auto* colliderComponent = playerCollider.TryGet();
		auto* rigid = colliderComponent ? colliderComponent->pRigidbodyDynamic : nullptr;
		if(!rigid) return;

		const physx::PxVec3 rigidVelocity = rigid->getLinearVelocity();
		Vector3 horizontal(rigidVelocity.x, 0.0f, rigidVelocity.z);
		const float speed = horizontal.length();
		const Vector3 currentDirection = speed > 0.0001f ? horizontal / speed : inputDirection;
		const float alignment = currentDirection.dot(inputDirection);

		// CharacterController owns the complete tangent velocity while grounded on a
		// slope. Replacing only X/Z here while preserving its slope-derived Y breaks
		// the tangent, creates an outward normal velocity on descents, and makes the
		// capsule lose contact with the ramp. Keep the run-charge state but do not
		// overwrite PhysX velocity until the surface is effectively flat again.
		constexpr float kSlopeVerticalThreshold = 0.35f;
		if(controller.IsGrounded() && std::abs(rigidVelocity.y) > kSlopeVerticalThreshold) {
			runAssistNormalized = SmoothStep01(runAssistTimer / assistBuildSeconds);
			return;
		}

		float targetSpeed = assistBaseRunSpeed;
		float acceleration = assistLowSpeedAcceleration;
		if(controller.IsGrounded()) {
			if(alignment < -0.05f) {
				runAssistTimer = 0.0f;
				acceleration *= assistReverseMultiplier;
			} else if(alignment > 0.45f || speed < 0.5f) {
				runAssistTimer = (std::min)(assistBuildSeconds, runAssistTimer + dt);
			} else {
				runAssistTimer = (std::max)(0.0f, runAssistTimer - dt * 1.6f);
			}
			runAssistNormalized = SmoothStep01(runAssistTimer / assistBuildSeconds);
			targetSpeed += assistRunSpeedBonus * runAssistNormalized;
			if(speed > assistBaseRunSpeed * 0.88f && targetSpeed > assistBaseRunSpeed) {
				acceleration = (std::max)(acceleration, assistHighSpeedAcceleration);
			}
		} else {
			runAssistTimer = (std::max)(0.0f, runAssistTimer - dt * 0.35f);
			runAssistNormalized = SmoothStep01(runAssistTimer / assistBuildSeconds);
			targetSpeed += assistRunSpeedBonus * runAssistNormalized * 0.65f;
			acceleration = assistAirAcceleration * (alignment < 0.0f ? 1.35f : 1.0f);
		}

		const Vector3 target = inputDirection * targetSpeed;
		horizontal = MoveTowards(horizontal, target, acceleration * dt);
		rigid->setLinearVelocity(physx::PxVec3(horizontal.x, rigidVelocity.y, horizontal.z));
		rigid->wakeUp();
	}

	void ApplyJumpMomentum(int stage) {
		const float boost = stage >= 3 ? assistThirdJumpBoost
			: stage == 2 ? assistSecondJumpBoost
			: assistFirstJumpBoost;
		ApplyHorizontalBoost(boost);
	}

	void ApplyLandingMomentum() {
		const Vector3 inputDirection = BuildCameraRelativeInput();
		if(inputDirection.length() <= 0.0001f) return;
		auto* colliderComponent = playerCollider.TryGet();
		auto* rigid = colliderComponent ? colliderComponent->pRigidbodyDynamic : nullptr;
		if(!rigid) return;
		const physx::PxVec3 velocity = rigid->getLinearVelocity();

		// A landing event also occurs when reacquiring a ramp. Do not add a flat
		// horizontal impulse to a slope-tangent velocity on that transition.
		if(std::abs(velocity.y) > 0.35f) return;

		Vector3 horizontal(velocity.x, 0.0f, velocity.z);
		if(horizontal.length() <= 0.35f || horizontal.normalize().dot(inputDirection) <= 0.15f) return;
		ApplyHorizontalBoost(assistLandingCarryBoost);
		runAssistTimer = (std::max)(runAssistTimer, assistBuildSeconds * 0.45f);
	}

	void ApplyHorizontalBoost(float amount) {
		if(!movementAssistEnabled || amount <= 0.0f) return;
		const Vector3 inputDirection = BuildCameraRelativeInput();
		if(inputDirection.length() <= 0.0001f) return;
		auto* colliderComponent = playerCollider.TryGet();
		auto* rigid = colliderComponent ? colliderComponent->pRigidbodyDynamic : nullptr;
		if(!rigid) return;

		const physx::PxVec3 velocity = rigid->getLinearVelocity();
		Vector3 horizontal(velocity.x, 0.0f, velocity.z);
		horizontal += inputDirection * amount;
		const float speedCap = assistBaseRunSpeed + assistRunSpeedBonus + 0.65f;
		if(horizontal.length() > speedCap) horizontal = horizontal.normalize() * speedCap;
		rigid->setLinearVelocity(physx::PxVec3(horizontal.x, velocity.y, horizontal.z));
		rigid->wakeUp();
	}

	Vector3 BuildCameraRelativeInput() {
		Vector3 raw;
		if(IsKeyHeld('W') || IsKeyHeld(VK_UP)) raw.z += 1.0f;
		if(IsKeyHeld('S') || IsKeyHeld(VK_DOWN)) raw.z -= 1.0f;
		if(IsKeyHeld('D') || IsKeyHeld(VK_RIGHT)) raw.x += 1.0f;
		if(IsKeyHeld('A') || IsKeyHeld(VK_LEFT)) raw.x -= 1.0f;
		if(raw.length() <= 0.0001f) return {};
		raw = raw.normalize();

		if(!cameraTransform.IsValid()) ResolveCameraTransform();
		if(auto* cameraPose = cameraTransform.TryGet()) {
			Vector3 forward = cameraPose->front();
			Vector3 right = cameraPose->right();
			forward.y = 0.0f;
			right.y = 0.0f;
			if(forward.length() > 0.0001f) forward = forward.normalize();
			if(right.length() > 0.0001f) right = right.normalize();
			const Vector3 result = forward * raw.z + right * raw.x;
			if(result.length() > 0.0001f) return result.normalize();
		}
		return raw;
	}

	bool IsKeyHeld(int keyCode) const {
		if(GetKey(keyCode)) return true;
		SceneContext* context = m_ref.GetScene();
		if(!context || !context->manager || !context->manager->hwnd) return false;
		HWND foreground = GetForegroundWindow();
		if(foreground != context->manager->hwnd && !IsChild(context->manager->hwnd, foreground)) return false;
		return (GetAsyncKeyState(keyCode) & 0x8000) != 0;
	}

	void EmitAt(
		const Vector3& worldOrigin,
		int count,
		float horizontalSpeed,
		float upwardSpeed,
		float lifetime,
		float particleSize
	) {
		auto* feedbackPose = transform.TryGet();
		auto* feedbackParticle = particle.TryGet();
		if(!feedbackPose || !feedbackParticle) return;

		// Particle positions are local to the feedback entity. Place the emitter at
		// the event once and emit from local zero so world coordinates are not added
		// again by RenderableParticle. Clearing the previous burst also prevents old
		// particles from being dragged when the emitter is moved to a later event.
		for(auto& state : feedbackParticle->Particle) state.LifeTime = 0.0f;
		feedbackPose->position = worldOrigin;
		feedbackParticle->particleSize = particleSize;
		PlatformerFeedback::Burst(
			feedbackParticle,
			Vector3(0.0f, 0.0f, 0.0f),
			count,
			horizontalSpeed,
			upwardSpeed,
			lifetime);
	}

	void Impulse(
		float strength,
		float duration,
		float fovKick,
		const Vector3& direction = Vector3()
	) {
		if(!camera.IsValid()) camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		if(auto* controller = camera.TryGet()) controller->AddImpulse(strength, duration, fovKick, direction);
	}

	void CaptureRevisions(const PlatformerCharacterController& controller) {
		jumpRevision = controller.GetJumpEventRevision();
		landRevision = controller.GetLandEventRevision();
		wallRevision = controller.GetWallKickEventRevision();
		stompRevision = controller.GetStompEventRevision();
		damageRevision = controller.GetDamageEventRevision();
		respawnRevision = controller.GetRespawnEventRevision();
	}

	static float SmoothStep01(float value) {
		value = std::clamp(value, 0.0f, 1.0f);
		return value * value * (3.0f - 2.0f * value);
	}

	static Vector3 MoveTowards(const Vector3& current, const Vector3& target, float maxDelta) {
		const Vector3 delta = target - current;
		const float distance = delta.length();
		if(distance <= maxDelta || distance <= 0.0001f) return target;
		return current + delta * (maxDelta / distance);
	}

	ComponentRef<PlatformerCharacterController> player;
	ComponentRef<PlatformerCameraController> camera;
	ComponentRef<ColliderComponent> playerCollider;
	ComponentRef<TransformComponent> cameraTransform;
	ComponentRef<TransformComponent> transform;
	ComponentRef<ParticleComponent> particle;
	ComponentRef<AudioComponent> audio;
	float runAssistTimer = 0.0f;
	float runAssistNormalized = 0.0f;
	float stepAssistTimer = 0.0f;
	float stepAssistGroundTimer = 0.0f;
	float lastStepRise = 0.0f;
	float previousVerticalVelocity = 0.0f;
	uint32_t jumpRevision = 0;
	uint32_t landRevision = 0;
	uint32_t wallRevision = 0;
	uint32_t stompRevision = 0;
	uint32_t damageRevision = 0;
	uint32_t respawnRevision = 0;
};
