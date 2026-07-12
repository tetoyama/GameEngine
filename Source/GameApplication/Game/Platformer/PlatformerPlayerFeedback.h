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
		REFLECT_FIELD(float, stepAssistHeight, 0.34f)
		REFLECT_FIELD(float, stepAssistProbeDistance, 0.40f)
		REFLECT_FIELD(float, stepAssistForwardOffset, 0.02f)
		REFLECT_FIELD(float, stepAssistCooldown, 0.055f)

public:
	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		DECODE_FIELDS(node);
		ValidateAssistSettings();
		return true;
	}

	void inspector(SceneContext* context) override {
		ImGui::Text("Platformer Player Feedback");
		INSPECTOR_FIELDS();
		ImGui::Text("Run Assist: %.2f", runAssistNormalized);
		ImGui::Text("Step Cooldown: %.3f", stepAssistTimer);
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
			EmitAt(origin, 56, 5.4f, 5.2f, 0.88f, 0.21f);
			Impulse(0.48f, 0.28f, 0.060f);
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ImpactPath);
		}

		if(controller->GetRespawnEventRevision() != respawnRevision) {
			respawnRevision = controller->GetRespawnEventRevision();
			runAssistTimer = 0.0f;
			stepAssistTimer = stepAssistCooldown;
			EmitAt(origin, 46, 4.2f, 6.6f, 1.05f, 0.17f);
			Impulse(0.18f, 0.30f, 0.022f, Vector3(0.0f, 1.0f, 0.0f));
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::CheckpointPath);
		}

		previousVerticalVelocity = controller->GetVerticalVelocity();
	}

private:
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
		stepAssistHeight = std::clamp(stepAssistHeight, 0.05f, 0.42f);
		stepAssistProbeDistance = std::clamp(stepAssistProbeDistance, 0.15f, 0.65f);
		stepAssistForwardOffset = std::clamp(stepAssistForwardOffset, 0.0f, 0.20f);
		stepAssistCooldown = std::clamp(stepAssistCooldown, 0.01f, 0.20f);
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

	void ApplyStepAssist(
		const PlatformerCharacterController& controller,
		TransformComponent& playerPose
	) {
		if(!stepAssistEnabled || stepAssistTimer > 0.0f || !controller.IsControlEnabled() ||
		   !controller.IsGrounded() || std::abs(controller.GetVerticalVelocity()) > 0.75f) {
			return;
		}

		Vector3 direction = BuildCameraRelativeInput();
		direction.y = 0.0f;
		if(direction.length() <= 0.0001f) return;
		direction = direction.normalize();

		auto* colliderComponent = playerCollider.TryGet();
		auto* rigid = colliderComponent ? colliderComponent->pRigidbodyDynamic : nullptr;
		auto* probe = PlatformerSceneAccess::Physics(m_ref.GetScene());
		auto* physics = probe ? probe->Raw() : nullptr;
		if(!rigid || !physics) return;

		const physx::PxVec3 velocity = rigid->getLinearVelocity();
		const Vector3 horizontal(velocity.x, 0.0f, velocity.z);
		if(horizontal.length() < 0.45f || horizontal.normalize().dot(direction) < 0.25f) return;

		const physx::PxU32 selfMask = ResolveActorLayerMask(rigid);
		const float lowerProbeHeight = (std::min)(0.10f, stepAssistHeight * 0.40f);
		const Vector3 faceOrigin = playerPose.position + direction * stepAssistForwardOffset;
		const physx::PxVec3 forward(direction.x, 0.0f, direction.z);

		const RayHit lowerHit = physics->RaycastWithMask(
			physx::PxVec3(faceOrigin.x, playerPose.position.y + lowerProbeHeight, faceOrigin.z),
			forward,
			stepAssistProbeDistance,
			selfMask);
		if(!IsStaticSolidHit(lowerHit) || std::abs(lowerHit.normal.y) > 0.45f) return;

		// A clear ray above the accepted step height distinguishes a curb or stair
		// from a full wall, arena boundary or large platform side.
		const RayHit upperHit = physics->RaycastWithMask(
			physx::PxVec3(faceOrigin.x, playerPose.position.y + stepAssistHeight + 0.08f, faceOrigin.z),
			forward,
			stepAssistProbeDistance,
			selfMask);
		if(IsStaticSolidHit(upperHit)) return;

		const float beyondFace = std::clamp(
			lowerHit.distance + 0.14f,
			0.14f,
			stepAssistProbeDistance + 0.06f);
		const Vector3 topSample = faceOrigin + direction * beyondFace;
		const RayHit topHit = physics->RaycastWithMask(
			physx::PxVec3(topSample.x, playerPose.position.y + stepAssistHeight + 0.16f, topSample.z),
			physx::PxVec3(0.0f, -1.0f, 0.0f),
			stepAssistHeight + 0.24f,
			selfMask);
		const float minNormalY = std::cos(50.0f * DirectX::XM_PI / 180.0f);
		if(!IsStaticSolidHit(topHit) || topHit.normal.y < minNormalY) return;

		const float rise = topHit.position.y - playerPose.position.y;
		if(rise <= 0.025f || rise > stepAssistHeight + 0.025f) return;

		// The player capsule spans approximately 1.5 m from the entity origin. Check
		// upward clearance before moving it so low ceilings cannot trap the actor.
		const RayHit ceilingHit = physics->RaycastWithMask(
			physx::PxVec3(playerPose.position.x, playerPose.position.y + 1.52f, playerPose.position.z),
			physx::PxVec3(0.0f, 1.0f, 0.0f),
			rise + 0.06f,
			selfMask);
		if(IsStaticSolidHit(ceilingHit)) return;

		Vector3 target = playerPose.position;
		target.y += rise + 0.012f;
		target += direction * 0.025f;
		playerPose.position = target;

		physx::PxTransform actorPose = rigid->getGlobalPose();
		actorPose.p = physx::PxVec3(target.x, target.y, target.z);
		rigid->setGlobalPose(actorPose, true);
		rigid->setLinearVelocity(physx::PxVec3(velocity.x, 0.0f, velocity.z));
		rigid->wakeUp();
		stepAssistTimer = stepAssistCooldown;
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
	float previousVerticalVelocity = 0.0f;
	uint32_t jumpRevision = 0;
	uint32_t landRevision = 0;
	uint32_t wallRevision = 0;
	uint32_t stompRevision = 0;
	uint32_t damageRevision = 0;
	uint32_t respawnRevision = 0;
};
