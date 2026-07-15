#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/ColliderComponent.h"
#include "Engine/Scene/Component/entityNameComponent.h"
#include "Engine/Scene/Component/materialComponent.h"
#include "Engine/Scene/Component/particleComponent.h"
#include "Game/Platformer/PlatformerCameraController.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerFeedback.h"
#include "Game/Platformer/PlatformerSceneAccess.h"

#include <algorithm>
#include <cmath>

class PlatformerMovingPlatform : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerMovingPlatform)
		REFLECT_FIELD(Vector3, localOffset, Vector3(0.0f, 3.0f, 0.0f))
		REFLECT_FIELD(float, cycleSeconds, 3.5f)
		REFLECT_FIELD(float, phaseOffset, 0.0f)
		REFLECT_FIELD(bool, carryPlayer, true)
		REFLECT_FIELD(bool, interactionFeedbackEnabled, true)
		REFLECT_FIELD(bool, pathTelegraphEnabled, true)
		REFLECT_FIELD(int, telegraphDotCount, 18)
		REFLECT_FIELD(float, telegraphParticleSize, 0.085f)
		REFLECT_FIELD(float, telegraphPulseSpeed, 3.2f)
		REFLECT_FIELD(float, preTurnWarningSeconds, 0.38f)
		REFLECT_FIELD(bool, motionTrailEnabled, true)
		REFLECT_FIELD(float, motionTrailInterval, 0.075f)
		REFLECT_FIELD(bool, platformGlowEnabled, true)
		REFLECT_FIELD(float, platformGlowIntensity, 2.2f)

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
		ImGui::Text("Platformer Moving Platform");
		INSPECTOR_FIELDS();
		ImGui::Text("Rider: %s", rider.IsValid() ? "attached" : "none");
		ImGui::Text("Path Progress: %.3f", currentEased);
		ImGui::Text("Next Turn: %.3f sec", timeToNextTurn);
	}

	void OnStart() override {
		ValidateSettings();
		transform = GetComponentRef<TransformComponent>();
		material = GetComponentRef<MaterialComponent>();
		camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		if(auto* t = transform.TryGet()) {
			startPosition = t->position;
			previousPosition = t->position;
		}
		if(auto* mat = material.TryGet()) {
			baseMaterial = mat->Material;
			materialCaptured = true;
		}
		elapsed = (std::max)(0.0f, cycleSeconds) * phaseOffset;
		motionTrailTimer = 0.0f;
		warningTriggeredThisLeg = false;
		QueueCreateTelegraphEmitter();
	}

	void OnFixedUpdate(float dt) override {
		auto* t = transform.TryGet();
		if(!t || cycleSeconds <= 0.001f) return;
		const float safeDt = (std::max)(0.0f, dt);
		elapsed += safeDt;
		endpointFeedbackCooldown = (std::max)(0.0f, endpointFeedbackCooldown - safeDt);
		motionTrailTimer -= safeDt;

		const float normalized = std::fmod(elapsed, cycleSeconds) / cycleSeconds;
		const float eased = 0.5f - 0.5f * std::cos(normalized * DirectX::XM_2PI);
		const Vector3 next = startPosition + localOffset * eased;
		const Vector3 delta = next - previousPosition;
		t->position = next;
		previousPosition = next;
		currentEased = eased;

		const float alongMotion = delta.dot(localOffset);
		const int travelSign = alongMotion > 0.00001f ? 1 : (alongMotion < -0.00001f ? -1 : 0);
		if(interactionFeedbackEnabled && travelSign != 0 && previousTravelSign != 0 &&
		   travelSign != previousTravelSign && endpointFeedbackCooldown <= 0.0f) {
			EmitEndpointFeedback(*t, travelSign);
			endpointFeedbackCooldown = 0.22f;
		}
		if(travelSign != 0) previousTravelSign = travelSign;

		const float halfPhase = std::fmod(normalized, 0.5f);
		timeToNextTurn = (0.5f - halfPhase) * cycleSeconds;
		const float speedWave = std::abs(std::sin(normalized * DirectX::XM_2PI));
		const float endpointProximity = 1.0f - std::clamp(speedWave * 2.8f, 0.0f, 1.0f);

		if(pathTelegraphEnabled) {
			UpdatePathTelegraph(normalized, eased, travelSign, endpointProximity);
			if(timeToNextTurn <= preTurnWarningSeconds && !warningTriggeredThisLeg) {
				EmitPreTurnWarning(*t, travelSign);
				warningTriggeredThisLeg = true;
			}
			if(timeToNextTurn > preTurnWarningSeconds * 1.65f) warningTriggeredThisLeg = false;
		}

		const float platformSpeed = safeDt > 0.0001f ? delta.length() / safeDt : 0.0f;
		if(motionTrailEnabled && platformSpeed > 0.18f && motionTrailTimer <= 0.0f) {
			EmitMotionTrail(*t, delta, platformSpeed);
			motionTrailTimer = motionTrailInterval;
		}
		UpdatePlatformMaterial(endpointProximity, travelSign);

		if(carryPlayer && riderGrace > 0.0f && rider.IsValid()) {
			riderGrace = (std::max)(0.0f, riderGrace - safeDt);
			MoveRider(delta);
		} else {
			riderGrace = (std::max)(0.0f, riderGrace - safeDt);
			if(riderGrace <= 0.0f) rider = {};
		}
	}

	void OnStop() override {
		if(materialCaptured) {
			if(auto* mat = material.TryGet()) mat->Material = baseMaterial;
		}
		if(telegraphEntity.IsValid()) QueueDestroyEntity(telegraphEntity.GetEntityID());
		telegraphEntity = {};
		telegraphTransform.Reset();
		telegraphParticle.Reset();
	}

	void OnCollisionStay(const HitInfo& hit) override {
		if(!carryPlayer || !hit.other.IsValid()) return;
		ComponentRef<PlatformerCharacterController> controller(hit.other);
		ComponentRef<TransformComponent> playerTransform(hit.other);
		auto* platformPose = transform.TryGet();
		auto* playerPose = playerTransform.TryGet();
		if(!controller.IsValid() || !platformPose || !playerPose) return;
		if(playerPose->position.y + 0.05f < platformPose->position.y) return;

		const bool newRider = !rider.IsValid() || !(rider == hit.other) || riderGrace <= 0.0f;
		rider = hit.other;
		riderGrace = 0.12f;
		if(newRider && interactionFeedbackEnabled) EmitMountFeedback(*playerPose);
	}

	void OnCollisionExit(const HitInfo& hit) override {
		if(!(rider == hit.other)) return;
		riderGrace = 0.08f;
	}

private:
	static constexpr int kTransientStart = 64;

	void ValidateSettings() {
		cycleSeconds = (std::max)(0.05f, cycleSeconds);
		phaseOffset = std::clamp(phaseOffset, 0.0f, 1.0f);
		telegraphDotCount = std::clamp(telegraphDotCount, 6, 40);
		telegraphParticleSize = std::clamp(telegraphParticleSize, 0.035f, 0.20f);
		telegraphPulseSpeed = std::clamp(telegraphPulseSpeed, 0.2f, 12.0f);
		preTurnWarningSeconds = std::clamp(preTurnWarningSeconds, 0.10f, 1.25f);
		motionTrailInterval = std::clamp(motionTrailInterval, 0.035f, 0.30f);
		platformGlowIntensity = std::clamp(platformGlowIntensity, 0.0f, 8.0f);
	}

	void QueueCreateTelegraphEmitter() {
		SceneContext* context = m_ref.GetScene();
		if(!context || !context->commands) return;
		const CommandEntity runtime = QueueCreateEntity();
		QueueAddComponent<NameComponent>(runtime);
		QueueAddComponent<TransformComponent>(runtime);
		QueueAddComponent<ParticleComponent>(runtime);
		QueueEntitySetup(runtime, [this](Entity entity, SceneContext& scene) {
			telegraphEntity = EntityRef(entity, &scene);
			telegraphTransform = ComponentRef<TransformComponent>(entity, &scene);
			telegraphParticle = ComponentRef<ParticleComponent>(entity, &scene);
			if(auto* name = scene.component->GetComponent<NameComponent>(entity)) {
				name->name = "PlatformerMovingPlatformTelegraph";
			}
			ConfigureTelegraphEmitter();
		});
	}

	void ConfigureTelegraphEmitter() {
		if(auto* pose = telegraphTransform.TryGet()) {
			pose->position = startPosition;
			pose->scale = Vector3(1.0f, 1.0f, 1.0f);
		}
		if(auto* particles = telegraphParticle.TryGet()) {
			particles->isLoop = false;
			particles->SpawnInterval = 0.0f;
			particles->SpawnCount = 0;
			particles->SpawnTimer = 0.0f;
			particles->particleLifeTime = 1.0f;
			particles->particleSize = telegraphParticleSize;
			particles->AddSpeed = Vector3(0.0f, -1.2f / telegraphParticleSize, 0.0f);
			particles->MulSpeed = Vector3(0.22f, 0.34f, 0.22f);
			for(auto& state : particles->Particle) state.LifeTime = 0.0f;
		}
	}

	void MoveRider(const Vector3& delta) {
		ComponentRef<TransformComponent> riderTransform(rider);
		ComponentRef<ColliderComponent> riderCollider(rider);
		auto* pose = riderTransform.TryGet();
		if(!pose) return;
		pose->position += delta;

		if(auto* col = riderCollider.TryGet()) {
			if(auto* rigid = col->pRigidbodyDynamic) {
				const physx::PxTransform current = rigid->getGlobalPose();
				rigid->setGlobalPose(physx::PxTransform(
					current.p + physx::PxVec3(delta.x, delta.y, delta.z), current.q));
			}
		}
	}

	void UpdatePathTelegraph(float normalized, float eased, int travelSign, float endpointProximity) {
		auto* particles = telegraphParticle.TryGet();
		if(!particles) return;
		particles->particleSize = telegraphParticleSize;

		Vector3 pathDirection = localOffset;
		if(pathDirection.length() <= 0.0001f) pathDirection = Vector3(0.0f, 1.0f, 0.0f);
		else pathDirection = pathDirection.normalize();
		Vector3 side(-pathDirection.z, 0.0f, pathDirection.x);
		if(side.length() <= 0.0001f) side = Vector3(1.0f, 0.0f, 0.0f);
		else side = side.normalize();
		Vector3 secondary = Cross(pathDirection, side);
		if(secondary.length() <= 0.0001f) secondary = Vector3(0.0f, 0.0f, 1.0f);
		else secondary = secondary.normalize();

		for(int i = 0; i < telegraphDotCount; ++i) {
			const float t = telegraphDotCount > 1
				? static_cast<float>(i) / static_cast<float>(telegraphDotCount - 1)
				: 0.0f;
			const float traveling = 0.5f + 0.5f * std::sin(
				(t * 5.0f - normalized * telegraphPulseSpeed * static_cast<float>(travelSign == 0 ? 1 : travelSign)) * DirectX::XM_2PI);
			const float markerDistance = std::abs(t - eased);
			const float markerBoost = 1.0f - std::clamp(markerDistance / 0.10f, 0.0f, 1.0f);
			const DirectX::XMFLOAT4 color(
				0.12f + t * 0.42f,
				0.62f + traveling * 0.26f,
				1.0f,
				0.30f + traveling * 0.32f + markerBoost * 0.28f);
			RefreshPersistentParticle(
				i,
				startPosition + localOffset * t,
				color,
				0.42f + traveling * 0.26f + markerBoost * 0.60f);
		}

		const int startHalo = telegraphDotCount;
		const int endHalo = telegraphDotCount + 4;
		const float haloPulse = 0.5f + 0.5f * std::sin(elapsed * telegraphPulseSpeed * DirectX::XM_2PI);
		const float haloRadius = 0.20f + haloPulse * 0.08f;
		for(int i = 0; i < 4; ++i) {
			const float angle = DirectX::XM_PIDIV2 * static_cast<float>(i);
			const Vector3 ringOffset = side * (std::cos(angle) * haloRadius) + secondary * (std::sin(angle) * haloRadius);
			const bool destinationIsEnd = travelSign >= 0;
			RefreshPersistentParticle(
				startHalo + i,
				startPosition + ringOffset,
				destinationIsEnd
					? DirectX::XMFLOAT4(0.18f, 0.64f, 1.0f, 0.52f)
					: DirectX::XMFLOAT4(1.0f, 0.62f, 0.10f, 0.82f),
				0.55f + (!destinationIsEnd ? endpointProximity : 0.0f) * 0.65f);
			RefreshPersistentParticle(
				endHalo + i,
				startPosition + localOffset + ringOffset,
				destinationIsEnd
					? DirectX::XMFLOAT4(1.0f, 0.62f, 0.10f, 0.82f)
					: DirectX::XMFLOAT4(0.18f, 0.64f, 1.0f, 0.52f),
				0.55f + (destinationIsEnd ? endpointProximity : 0.0f) * 0.65f);
		}

		const int markerStart = telegraphDotCount + 8;
		const Vector3 current = startPosition + localOffset * eased;
		for(int i = 0; i < 4; ++i) {
			const float trail = static_cast<float>(i) * 0.16f;
			const Vector3 markerPosition = current - pathDirection * static_cast<float>(travelSign == 0 ? 1 : travelSign) * trail;
			RefreshPersistentParticle(
				markerStart + i,
				markerPosition,
				rider.IsValid()
					? DirectX::XMFLOAT4(0.18f, 1.0f, 0.55f, 0.95f)
					: DirectX::XMFLOAT4(0.86f, 0.96f, 1.0f, 0.95f),
				1.15f - static_cast<float>(i) * 0.16f);
		}
	}

	void RefreshPersistentParticle(
		int index,
		const Vector3& worldPosition,
		const DirectX::XMFLOAT4& color,
		float sizeScale
	) {
		auto* particles = telegraphParticle.TryGet();
		if(!particles || index < 0 || index >= kTransientStart) return;
		PARTICLE& state = particles->Particle[index];
		const float inverseSize = 1.0f / (std::max)(0.001f, particles->particleSize);
		state.Position = (worldPosition - startPosition) * inverseSize;
		state.Speed = Vector3();
		state.Color = color;
		state.SizeScale = (std::max)(0.10f, sizeScale);
		state.LifeTime = particles->particleLifeTime;
	}

	void SpawnTransient(
		const Vector3& worldPosition,
		const Vector3& worldVelocity,
		float lifetime,
		float sizeScale,
		const DirectX::XMFLOAT4& color
	) {
		auto* particles = telegraphParticle.TryGet();
		if(!particles) return;
		const int transientCount = MAXPARTICLE - kTransientStart;
		const int index = kTransientStart + (transientCursor++ % transientCount);
		PARTICLE& state = particles->Particle[index];
		const float inverseSize = 1.0f / (std::max)(0.001f, particles->particleSize);
		state.Position = (worldPosition - startPosition) * inverseSize;
		state.Speed = worldVelocity * inverseSize;
		state.Color = color;
		state.SizeScale = (std::max)(0.10f, sizeScale);
		state.LifeTime = std::clamp(lifetime, 0.05f, particles->particleLifeTime);
	}

	void EmitPreTurnWarning(const TransformComponent& platformPose, int travelSign) {
		Vector3 direction = localOffset;
		if(direction.length() <= 0.0001f) direction = Vector3(0.0f, 1.0f, 0.0f);
		else direction = direction.normalize() * static_cast<float>(travelSign == 0 ? 1 : travelSign);
		Vector3 side(-direction.z, 0.0f, direction.x);
		if(side.length() <= 0.0001f) side = Vector3(1.0f, 0.0f, 0.0f);
		else side = side.normalize();

		for(int i = 0; i < 28; ++i) {
			const float angle = DirectX::XM_2PI * static_cast<float>(i) / 28.0f;
			const Vector3 radial(std::cos(angle), 0.18f * std::sin(angle * 2.0f), std::sin(angle));
			const bool spark = i % 4 == 0;
			SpawnTransient(
				platformPose.position + side * (std::cos(angle) * 0.15f),
				radial * (spark ? 3.8f : 2.1f) - direction * 0.45f + Vector3(0.0f, spark ? 1.7f : 0.65f, 0.0f),
				spark ? 0.55f : 0.72f,
				spark ? 0.48f : 0.82f,
				spark
					? DirectX::XMFLOAT4(1.0f, 0.28f, 0.02f, 0.96f)
					: DirectX::XMFLOAT4(1.0f, 0.78f, 0.12f, 0.76f));
		}
		Impulse(0.10f, 0.13f, -0.004f, direction * -1.0f);
	}

	void EmitMotionTrail(const TransformComponent& platformPose, const Vector3& delta, float speed) {
		Vector3 direction = delta;
		if(direction.length() <= 0.0001f) return;
		direction = direction.normalize();
		Vector3 side(-direction.z, 0.0f, direction.x);
		if(side.length() <= 0.0001f) side = Vector3(1.0f, 0.0f, 0.0f);
		else side = side.normalize();
		const float strength = std::clamp(speed / 5.0f, 0.25f, 1.0f);
		for(int i = 0; i < 3; ++i) {
			const float lane = (static_cast<float>(i) - 1.0f) * 0.18f;
			SpawnTransient(
				platformPose.position - direction * 0.12f + side * lane,
				-direction * (1.3f + strength * 2.8f) + side * lane * 1.4f + Vector3(0.0f, 0.24f, 0.0f),
				0.34f,
				0.36f + strength * 0.26f,
				DirectX::XMFLOAT4(0.18f, 0.74f + strength * 0.20f, 1.0f, 0.68f));
		}
	}

	void UpdatePlatformMaterial(float endpointProximity, int travelSign) {
		if(!platformGlowEnabled || !materialCaptured) return;
		auto* mat = material.TryGet();
		if(!mat) return;
		mat->Material = baseMaterial;
		const float movementPulse = 0.5f + 0.5f * std::sin(elapsed * telegraphPulseSpeed * 2.0f);
		const float riderBoost = rider.IsValid() ? 0.85f : 0.0f;
		mat->Material.EmissiveColor = rider.IsValid()
			? float3(0.10f, 1.0f, 0.45f)
			: (travelSign >= 0 ? float3(0.08f, 0.58f, 1.0f) : float3(0.52f, 0.18f, 1.0f));
		mat->Material.EmissiveIntensity = baseMaterial.EmissiveIntensity +
			0.28f + movementPulse * 0.22f + endpointProximity * platformGlowIntensity + riderBoost;
	}

	void EmitMountFeedback(const TransformComponent& playerPose) {
		PlatformerFeedback::SharedDirectionalBurst(
			m_ref.GetScene(),
			playerPose.position + Vector3(0.0f, 0.05f, 0.0f),
			Vector3(0.0f, 1.0f, 0.0f),
			42,
			2.8f,
			2.6f,
			2.2f,
			0.52f,
			0.095f,
			DirectX::XMFLOAT4(0.30f, 0.92f, 1.0f, 1.0f),
			DirectX::XMFLOAT4(0.48f, 0.32f, 1.0f, 1.0f));
		Impulse(0.075f, 0.12f, 0.008f, Vector3(0.0f, 1.0f, 0.0f));
	}

	void EmitEndpointFeedback(const TransformComponent& platformPose, int travelSign) {
		Vector3 direction = localOffset;
		if(direction.length() <= 0.0001f) direction = Vector3(0.0f, 1.0f, 0.0f);
		else direction = direction.normalize() * static_cast<float>(-travelSign);
		PlatformerFeedback::SharedDirectionalBurst(
			m_ref.GetScene(),
			platformPose.position,
			direction,
			34,
			2.4f,
			2.2f,
			1.8f,
			0.48f,
			0.08f,
			DirectX::XMFLOAT4(0.18f, 0.72f, 1.0f, 1.0f),
			DirectX::XMFLOAT4(0.62f, 0.22f, 1.0f, 1.0f));
	}

	void Impulse(float strength, float duration, float fovKick, const Vector3& direction) {
		if(!camera.IsValid()) camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		if(auto* controller = camera.TryGet()) {
			controller->AddImpulse(strength, duration, fovKick, direction);
		}
	}

	static Vector3 Cross(const Vector3& a, const Vector3& b) {
		return Vector3(
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x);
	}

	ComponentRef<TransformComponent> transform;
	ComponentRef<MaterialComponent> material;
	ComponentRef<PlatformerCameraController> camera;
	EntityRef rider;
	EntityRef telegraphEntity;
	ComponentRef<TransformComponent> telegraphTransform;
	ComponentRef<ParticleComponent> telegraphParticle;
	Vector3 startPosition;
	Vector3 previousPosition;
	MATERIAL baseMaterial{};
	float elapsed = 0.0f;
	float riderGrace = 0.0f;
	float endpointFeedbackCooldown = 0.0f;
	float motionTrailTimer = 0.0f;
	float currentEased = 0.0f;
	float timeToNextTurn = 0.0f;
	int previousTravelSign = 0;
	int transientCursor = 0;
	bool warningTriggeredThisLeg = false;
	bool materialCaptured = false;
};
