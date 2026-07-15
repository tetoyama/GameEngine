#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/ColliderComponent.h"
#include "Engine/Scene/Component/materialComponent.h"
#include "Engine/Scene/Component/particleComponent.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerSceneAccess.h"

#include <algorithm>
#include <cmath>

// Lightweight, self-contained ambient presentation for the player. This owns a
// dedicated ParticleComponent so persistent movement trails never clear or
// resize the one-shot interaction bursts emitted by PlatformerPlayerFeedback.
class PlatformerPlayerAmbientFeedback : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerPlayerAmbientFeedback)
		REFLECT_FIELD(bool, movementTrailEnabled, true)
		REFLECT_FIELD(bool, footstepDustEnabled, true)
		REFLECT_FIELD(bool, speedStreakEnabled, true)
		REFLECT_FIELD(bool, airTrailEnabled, true)
		REFLECT_FIELD(bool, skidFeedbackEnabled, true)
		REFLECT_FIELD(bool, apexFeedbackEnabled, true)
		REFLECT_FIELD(bool, ledgeFeedbackEnabled, true)
		REFLECT_FIELD(bool, speedGlowEnabled, true)
		REFLECT_FIELD(bool, damageFlashEnabled, true)
		REFLECT_FIELD(float, minimumTrailSpeed, 1.25f)
		REFLECT_FIELD(float, sprintTrailSpeed, 5.6f)
		REFLECT_FIELD(float, groundSmokeInterval, 0.060f)
		REFLECT_FIELD(float, airTrailInterval, 0.075f)
		REFLECT_FIELD(float, speedStreakInterval, 0.048f)
		REFLECT_FIELD(float, footstepDistance, 1.15f)
		REFLECT_FIELD(float, particleSize, 0.10f)
		REFLECT_FIELD(float, speedGlowThreshold, 4.6f)
		REFLECT_FIELD(float, speedGlowIntensity, 1.8f)

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
		ImGui::Text("Platformer Player Ambient Feedback");
		INSPECTOR_FIELDS();
		ImGui::Text("Speed: %.2f", currentHorizontalSpeed);
		ImGui::Text("Trail Cursor: %d", particleCursor);
	}

	void OnStart() override {
		ValidateSettings();
		emitterTransform = GetComponentRef<TransformComponent>();
		emitterParticle = GetComponentRef<ParticleComponent>();
		ResolvePlayerReferences();
		ConfigureEmitter();

		if(auto* pose = playerTransform.TryGet()) {
			previousPlayerPosition = pose->position;
			if(auto* emitter = emitterTransform.TryGet()) emitter->position = pose->position;
		}
		if(auto* controller = player.TryGet()) {
			previousVerticalVelocity = controller->GetVerticalVelocity();
			wasGrounded = controller->IsGrounded();
		}
		CapturePlayerMaterial();
	}

	void OnUpdate(float dt) override {
		const float safeDt = std::clamp(dt, 0.0f, 0.05f);
		if(safeDt <= 0.0f) return;
		ResolvePlayerReferences();

		auto* controller = player.TryGet();
		auto* pose = playerTransform.TryGet();
		auto* particles = emitterParticle.TryGet();
		if(!controller || !pose || !particles) return;

		Vector3 frameDelta = pose->position - previousPlayerPosition;
		const float teleportDistance = frameDelta.length();
		if(teleportDistance > 4.0f) {
			ClearParticles();
			frameDelta = Vector3();
			travelDistance = 0.0f;
		}
		MoveEmitterPreserveWorld(pose->position);

		Vector3 velocity = safeDt > 0.0001f ? frameDelta / safeDt : Vector3();
		if(auto* col = playerCollider.TryGet()) {
			if(auto* rigid = col->pRigidbodyDynamic) {
				const physx::PxVec3 measured = rigid->getLinearVelocity();
				velocity = Vector3(measured.x, measured.y, measured.z);
			}
		}

		Vector3 horizontalVelocity(velocity.x, 0.0f, velocity.z);
		currentHorizontalSpeed = horizontalVelocity.length();
		Vector3 direction = currentHorizontalSpeed > 0.001f
			? horizontalVelocity / currentHorizontalSpeed
			: previousHorizontalDirection;
		Vector3 side(-direction.z, 0.0f, direction.x);
		if(side.length() <= 0.0001f) side = Vector3(1.0f, 0.0f, 0.0f);
		else side = side.normalize();

		groundSmokeTimer -= safeDt;
		airTrailTimer -= safeDt;
		speedStreakTimer -= safeDt;
		skidCooldown -= safeDt;
		ambientElapsed += safeDt;

		const bool grounded = controller->IsGrounded();
		const bool presentationActive = controller->IsControlEnabled() && !controller->IsCleared();
		const float horizontalTravel = std::sqrt(frameDelta.x * frameDelta.x + frameDelta.z * frameDelta.z);
		if(grounded) travelDistance += horizontalTravel;

		if(presentationActive) {
			if(movementTrailEnabled && grounded && currentHorizontalSpeed >= minimumTrailSpeed && groundSmokeTimer <= 0.0f) {
				EmitGroundSmoke(direction, side, currentHorizontalSpeed);
				const float speedFactor = std::clamp(currentHorizontalSpeed / 8.0f, 0.0f, 1.0f);
				groundSmokeTimer = groundSmokeInterval * (1.15f - speedFactor * 0.62f);
			}

			if(footstepDustEnabled && grounded && currentHorizontalSpeed >= minimumTrailSpeed && travelDistance >= footstepDistance) {
				travelDistance = std::fmod(travelDistance, footstepDistance);
				EmitFootstepDust(direction, side);
				leftFoot = !leftFoot;
			}

			if(speedStreakEnabled && currentHorizontalSpeed >= sprintTrailSpeed && speedStreakTimer <= 0.0f) {
				EmitSpeedStreak(direction, side, currentHorizontalSpeed);
				speedStreakTimer = speedStreakInterval;
			}

			if(airTrailEnabled && !grounded && std::abs(velocity.y) >= 1.4f && airTrailTimer <= 0.0f) {
				EmitAirTrail(direction, side, velocity.y);
				airTrailTimer = airTrailInterval;
			}

			const float directionDot = previousHorizontalDirection.length() > 0.0001f && direction.length() > 0.0001f
				? previousHorizontalDirection.dot(direction)
				: 1.0f;
			const float brakingRate = (previousHorizontalSpeed - currentHorizontalSpeed) / safeDt;
			if(skidFeedbackEnabled && grounded && skidCooldown <= 0.0f && previousHorizontalSpeed >= 3.2f &&
			   ((currentHorizontalSpeed >= 1.4f && directionDot < 0.18f) || brakingRate > 24.0f)) {
				EmitSkid(previousHorizontalDirection.length() > 0.0001f ? previousHorizontalDirection : direction, side);
				skidCooldown = 0.22f;
			}

			if(apexFeedbackEnabled && !grounded && previousVerticalVelocity > 0.55f && velocity.y <= 0.0f) {
				EmitApexRing();
			}

			if(ledgeFeedbackEnabled && wasGrounded && !grounded && velocity.y <= 0.25f && velocity.y > -1.8f) {
				EmitLedgePuff(direction, side);
			}
		}

		UpdatePlayerMaterial(*controller, currentHorizontalSpeed);

		previousPlayerPosition = pose->position;
		previousVerticalVelocity = velocity.y;
		previousHorizontalSpeed = currentHorizontalSpeed;
		if(direction.length() > 0.0001f) previousHorizontalDirection = direction;
		wasGrounded = grounded;
	}

	void OnStop() override {
		RestorePlayerMaterial();
		ClearParticles();
	}

private:
	void ValidateSettings() {
		minimumTrailSpeed = std::clamp(minimumTrailSpeed, 0.2f, 5.0f);
		sprintTrailSpeed = (std::max)(minimumTrailSpeed + 0.25f, sprintTrailSpeed);
		groundSmokeInterval = std::clamp(groundSmokeInterval, 0.025f, 0.20f);
		airTrailInterval = std::clamp(airTrailInterval, 0.03f, 0.25f);
		speedStreakInterval = std::clamp(speedStreakInterval, 0.025f, 0.20f);
		footstepDistance = std::clamp(footstepDistance, 0.35f, 3.0f);
		particleSize = std::clamp(particleSize, 0.035f, 0.24f);
		speedGlowThreshold = std::clamp(speedGlowThreshold, 0.0f, 12.0f);
		speedGlowIntensity = std::clamp(speedGlowIntensity, 0.0f, 6.0f);
	}

	void ResolvePlayerReferences() {
		if(!player.IsValid()) player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		if(!player.IsValid()) return;
		const EntityRef entity = player.GetEntityRef();
		playerTransform = ComponentRef<TransformComponent>(entity);
		playerCollider = ComponentRef<ColliderComponent>(entity);
		playerMaterial = ComponentRef<MaterialComponent>(entity);
	}

	void ConfigureEmitter() {
		if(auto* emitter = emitterTransform.TryGet()) {
			emitter->scale = Vector3(1.0f, 1.0f, 1.0f);
		}
		if(auto* particles = emitterParticle.TryGet()) {
			particles->isLoop = false;
			particles->SpawnInterval = 0.0f;
			particles->SpawnCount = 0;
			particles->SpawnTimer = 0.0f;
			particles->particleLifeTime = 0.90f;
			particles->particleSize = particleSize;
			particles->AddSpeed = Vector3(0.0f, 0.65f / particleSize, 0.0f);
			particles->MulSpeed = Vector3(0.18f, 0.42f, 0.18f);
			for(auto& state : particles->Particle) state.LifeTime = 0.0f;
		}
	}

	void CapturePlayerMaterial() {
		if(playerMaterialCaptured) return;
		if(auto* material = playerMaterial.TryGet()) {
			basePlayerMaterial = material->Material;
			playerMaterialCaptured = true;
		}
	}

	void RestorePlayerMaterial() {
		if(!playerMaterialCaptured) return;
		if(auto* material = playerMaterial.TryGet()) material->Material = basePlayerMaterial;
	}

	void UpdatePlayerMaterial(const PlatformerCharacterController& controller, float speed) {
		if(!playerMaterialCaptured) CapturePlayerMaterial();
		auto* material = playerMaterial.TryGet();
		if(!material || !playerMaterialCaptured) return;
		material->Material = basePlayerMaterial;

		const float speedGlow = speedGlowEnabled
			? SmoothStep01((speed - speedGlowThreshold) / (2.6f + speedGlowThreshold * 0.10f))
			: 0.0f;
		if(speedGlow > 0.0f) {
			material->Material.EmissiveColor = float3(0.08f, 0.48f + speedGlow * 0.28f, 1.0f);
			material->Material.EmissiveIntensity = basePlayerMaterial.EmissiveIntensity + speedGlow * speedGlowIntensity;
		}

		if(damageFlashEnabled && controller.IsInvulnerable()) {
			const float flash = 0.45f + 0.55f * std::abs(std::sin(ambientElapsed * 26.0f));
			material->Material.BaseColor.x = basePlayerMaterial.BaseColor.x + (1.0f - basePlayerMaterial.BaseColor.x) * flash;
			material->Material.BaseColor.y = basePlayerMaterial.BaseColor.y * (1.0f - flash * 0.62f);
			material->Material.BaseColor.z = basePlayerMaterial.BaseColor.z * (1.0f - flash * 0.72f);
			material->Material.EmissiveColor = float3(1.0f, 0.05f, 0.02f);
			material->Material.EmissiveIntensity = basePlayerMaterial.EmissiveIntensity + 2.4f * flash;
		}
	}

	void MoveEmitterPreserveWorld(const Vector3& newPosition) {
		auto* emitter = emitterTransform.TryGet();
		auto* particles = emitterParticle.TryGet();
		if(!emitter || !particles) return;
		const Vector3 delta = newPosition - emitter->position;
		if(delta.length() > 0.00001f) {
			const float inverseSize = 1.0f / (std::max)(0.001f, particles->particleSize);
			for(auto& state : particles->Particle) {
				if(state.LifeTime > 0.0f) state.Position -= delta * inverseSize;
			}
		}
		emitter->position = newPosition;
	}

	void ClearParticles() {
		if(auto* particles = emitterParticle.TryGet()) {
			for(auto& state : particles->Particle) state.LifeTime = 0.0f;
		}
	}

	void SpawnParticle(
		const Vector3& worldOffset,
		const Vector3& worldVelocity,
		float lifetime,
		float sizeScale,
		const DirectX::XMFLOAT4& color
	) {
		auto* particles = emitterParticle.TryGet();
		if(!particles) return;
		const int index = particleCursor++ % MAXPARTICLE;
		PARTICLE& state = particles->Particle[index];
		const float inverseSize = 1.0f / (std::max)(0.001f, particles->particleSize);
		state.Position = worldOffset * inverseSize;
		state.Speed = worldVelocity * inverseSize;
		state.Color = color;
		state.SizeScale = (std::max)(0.10f, sizeScale);
		state.LifeTime = std::clamp(lifetime, 0.05f, particles->particleLifeTime);
	}

	void EmitGroundSmoke(const Vector3& direction, const Vector3& side, float speed) {
		const float strength = std::clamp(speed / 7.5f, 0.25f, 1.0f);
		for(int i = 0; i < 4; ++i) {
			const float wave = Wave(i, 1.71f);
			const float lateral = (static_cast<float>(i) - 1.5f) * 0.08f;
			SpawnParticle(
				Vector3(0.0f, 0.055f, 0.0f) - direction * (0.04f + wave * 0.12f) + side * lateral,
				-direction * (0.45f + strength * 1.25f + wave * 0.35f) + side * lateral * 2.2f + Vector3(0.0f, 0.55f + wave * 0.75f, 0.0f),
				0.48f + wave * 0.25f,
				0.78f + wave * 0.78f,
				DirectX::XMFLOAT4(0.66f + wave * 0.14f, 0.58f + wave * 0.12f, 0.43f + wave * 0.10f, 0.58f));
		}
	}

	void EmitFootstepDust(const Vector3& direction, const Vector3& side) {
		const float footSide = leftFoot ? -0.16f : 0.16f;
		for(int i = 0; i < 7; ++i) {
			const float angle = static_cast<float>(i) * 2.39996323f;
			const float wave = Wave(i, 2.37f);
			const Vector3 radial(std::cos(angle), 0.0f, std::sin(angle));
			SpawnParticle(
				Vector3(0.0f, 0.035f, 0.0f) + side * footSide,
				radial * (0.45f + wave * 0.85f) - direction * 0.35f + Vector3(0.0f, 0.35f + wave * 0.45f, 0.0f),
				0.36f + wave * 0.18f,
				0.48f + wave * 0.60f,
				DirectX::XMFLOAT4(0.82f, 0.72f, 0.52f, 0.62f));
		}
	}

	void EmitSpeedStreak(const Vector3& direction, const Vector3& side, float speed) {
		const float strength = std::clamp((speed - sprintTrailSpeed) / 2.5f, 0.0f, 1.0f);
		for(int i = 0; i < 3; ++i) {
			const float lane = (static_cast<float>(i) - 1.0f) * 0.18f;
			SpawnParticle(
				Vector3(0.0f, 0.48f + 0.20f * static_cast<float>(i % 2), 0.0f) + side * lane - direction * 0.18f,
				-direction * (3.2f + strength * 2.8f) + side * lane * 1.8f + Vector3(0.0f, 0.15f, 0.0f),
				0.20f + strength * 0.12f,
				0.35f + strength * 0.24f,
				DirectX::XMFLOAT4(0.14f, 0.72f + strength * 0.20f, 1.0f, 0.88f));
		}
	}

	void EmitAirTrail(const Vector3& direction, const Vector3& side, float verticalVelocity) {
		const float verticalSign = verticalVelocity >= 0.0f ? -1.0f : 1.0f;
		for(int i = 0; i < 3; ++i) {
			const float lane = (static_cast<float>(i) - 1.0f) * 0.13f;
			SpawnParticle(
				Vector3(0.0f, 0.28f, 0.0f) + side * lane,
				-direction * 1.15f + side * lane * 1.5f + Vector3(0.0f, verticalSign * 0.65f, 0.0f),
				0.30f,
				0.38f + 0.12f * static_cast<float>(i),
				DirectX::XMFLOAT4(0.48f, 0.82f, 1.0f, 0.65f));
		}
	}

	void EmitSkid(const Vector3& oldDirection, const Vector3& side) {
		for(int i = 0; i < 16; ++i) {
			const float angle = static_cast<float>(i) * 2.39996323f;
			const float wave = Wave(i, 1.93f);
			const Vector3 radial(std::cos(angle), 0.0f, std::sin(angle));
			const bool spark = i % 3 == 0;
			SpawnParticle(
				Vector3(0.0f, 0.05f, 0.0f) + side * ((i % 2 == 0) ? -0.16f : 0.16f),
				-oldDirection * (0.8f + wave * 1.5f) + radial * (0.7f + wave * 1.1f) + Vector3(0.0f, spark ? 1.1f : 0.42f, 0.0f),
				spark ? 0.48f : 0.62f,
				spark ? 0.35f : (0.72f + wave * 0.45f),
				spark
					? DirectX::XMFLOAT4(1.0f, 0.42f, 0.04f, 0.95f)
					: DirectX::XMFLOAT4(0.72f, 0.60f, 0.42f, 0.68f));
		}
	}

	void EmitApexRing() {
		for(int i = 0; i < 18; ++i) {
			const float angle = DirectX::XM_2PI * static_cast<float>(i) / 18.0f;
			const Vector3 radial(std::cos(angle), 0.0f, std::sin(angle));
			SpawnParticle(
				Vector3(0.0f, 0.72f, 0.0f),
				radial * 1.65f + Vector3(0.0f, 0.18f + 0.04f * static_cast<float>(i % 3), 0.0f),
				0.38f,
				0.34f + 0.16f * static_cast<float>(i % 2),
				(i % 2 == 0)
					? DirectX::XMFLOAT4(0.16f, 0.86f, 1.0f, 0.82f)
					: DirectX::XMFLOAT4(0.64f, 0.30f, 1.0f, 0.78f));
		}
	}

	void EmitLedgePuff(const Vector3& direction, const Vector3& side) {
		for(int i = 0; i < 9; ++i) {
			const float wave = Wave(i, 2.11f);
			const float lane = (static_cast<float>(i % 3) - 1.0f) * 0.14f;
			SpawnParticle(
				Vector3(0.0f, 0.04f, 0.0f) + side * lane - direction * 0.12f,
				-direction * (0.55f + wave * 0.75f) + side * lane * 2.0f + Vector3(0.0f, 0.38f + wave * 0.48f, 0.0f),
				0.42f + wave * 0.16f,
				0.55f + wave * 0.52f,
				DirectX::XMFLOAT4(0.76f, 0.68f, 0.52f, 0.62f));
		}
	}

	float Wave(int localIndex, float frequency) const {
		const float seed = static_cast<float>(particleCursor + localIndex + 1);
		return 0.5f + 0.5f * std::sin(seed * frequency);
	}

	static float SmoothStep01(float value) {
		value = std::clamp(value, 0.0f, 1.0f);
		return value * value * (3.0f - 2.0f * value);
	}

	ComponentRef<PlatformerCharacterController> player;
	ComponentRef<TransformComponent> playerTransform;
	ComponentRef<ColliderComponent> playerCollider;
	ComponentRef<MaterialComponent> playerMaterial;
	ComponentRef<TransformComponent> emitterTransform;
	ComponentRef<ParticleComponent> emitterParticle;
	MATERIAL basePlayerMaterial{};
	Vector3 previousPlayerPosition;
	Vector3 previousHorizontalDirection;
	float previousHorizontalSpeed = 0.0f;
	float previousVerticalVelocity = 0.0f;
	float currentHorizontalSpeed = 0.0f;
	float groundSmokeTimer = 0.0f;
	float airTrailTimer = 0.0f;
	float speedStreakTimer = 0.0f;
	float skidCooldown = 0.0f;
	float travelDistance = 0.0f;
	float ambientElapsed = 0.0f;
	int particleCursor = 0;
	bool wasGrounded = false;
	bool leftFoot = false;
	bool playerMaterialCaptured = false;
};
