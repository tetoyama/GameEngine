#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/materialComponent.h"
#include "Engine/Scene/Component/audioComponent.h"
#include "Engine/Scene/Component/particleComponent.h"
#include "Game/Platformer/PlatformerCameraController.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerFeedback.h"
#include "Game/Platformer/PlatformerGameManager.h"
#include "Game/Platformer/PlatformerSceneAccess.h"

#include <algorithm>
#include <cmath>

class PlatformerCoin : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerCoin)
		REFLECT_FIELD(float, rotationSpeed, 2.8f)
		REFLECT_FIELD(float, bobAmplitude, 0.18f)
		REFLECT_FIELD(float, bobFrequency, 2.2f)
		REFLECT_FIELD(float, collectDuration, 0.55f)
		REFLECT_FIELD(float, idleSparkInterval, 0.62f)

public:
	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		DECODE_FIELDS(node);
		collectDuration = (std::max)(0.20f, collectDuration);
		idleSparkInterval = std::clamp(idleSparkInterval, 0.25f, 1.5f);
		return true;
	}

	void inspector(SceneContext* context) override {
		ImGui::Text("Platformer Coin");
		INSPECTOR_FIELDS();
		ImGui::Text("Collected: %s", collected ? "true" : "false");
	}

	void OnStart() override {
		transform = GetComponentRef<TransformComponent>();
		material = GetComponentRef<MaterialComponent>();
		particle = GetComponentRef<ParticleComponent>();
		audio = GetComponentRef<AudioComponent>();
		manager = PlatformerSceneAccess::FindFirst<PlatformerGameManager>(m_ref.GetScene());
		camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		if(auto* t = transform.TryGet()) {
			basePosition = t->position;
			baseScale = t->scale;
			idleSparkTimer = idleSparkInterval *
				(0.35f + 0.45f * std::abs(std::sin(basePosition.x * 0.37f + basePosition.z * 0.19f)));
		}
		if(auto* mat = material.TryGet()) baseMaterial = mat->Material;
		if(auto* game = manager.TryGet()) game->RegisterCoin();
		registered = true;
	}

	void OnUpdate(float dt) override {
		auto* t = transform.TryGet();
		if(!t) return;
		const float safeDt = (std::max)(0.0f, dt);
		elapsed += safeDt;

		if(!collected) {
			t->position.y = basePosition.y + std::sin(elapsed * bobFrequency * DirectX::XM_2PI) * bobAmplitude;
			t->AddRotationY(rotationSpeed * safeDt);
			idleSparkTimer -= safeDt;
			if(idleSparkTimer <= 0.0f) {
				idleSparkTimer = idleSparkInterval;
				if(auto* effect = particle.TryGet()) {
					effect->particleSize = 0.055f;
					PlatformerFeedback::LayeredBurst(
						effect,
						Vector3(0.0f, 0.12f, 0.0f),
						18,
						0.85f,
						1.8f,
						0.52f,
						DirectX::XMFLOAT4(1.0f, 0.88f, 0.20f, 1.0f),
						DirectX::XMFLOAT4(1.0f, 0.42f, 0.04f, 1.0f));
				}
			}
			return;
		}

		collectTimer += safeDt;
		const float normalized = collectDuration > 0.0f
			? std::clamp(collectTimer / collectDuration, 0.0f, 1.0f)
			: 1.0f;

		// Keep the emitter fixed at the collection point. Particle positions are
		// local to this entity, so moving or shrinking the coin Transform would
		// drag and rescale the burst after it has already spawned.
		t->position = basePosition;
		t->scale = baseScale;
		t->AddRotationY(rotationSpeed * safeDt * (5.5f + normalized * 8.0f));

		if(auto* mat = material.TryGet()) {
			const float flash = (1.0f - normalized) * (1.0f - normalized);
			mat->Material.BaseColor.x = baseMaterial.BaseColor.x + (1.0f - baseMaterial.BaseColor.x) * flash;
			mat->Material.BaseColor.y = baseMaterial.BaseColor.y + (1.0f - baseMaterial.BaseColor.y) * flash;
			mat->Material.BaseColor.z = baseMaterial.BaseColor.z + (0.72f - baseMaterial.BaseColor.z) * flash;
			mat->Material.EmissiveColor = float3(1.0f, 0.58f, 0.04f);
			mat->Material.EmissiveIntensity = baseMaterial.EmissiveIntensity + flash * 7.5f;
		}

		if(normalized >= 1.0f && !destroyQueued) {
			destroyQueued = QueueDestroySelf();
		}
	}

	void OnStop() override {
		if(auto* t = transform.TryGet()) t->scale = baseScale;
		if(auto* mat = material.TryGet()) mat->Material = baseMaterial;
	}

	void OnTriggerEnter(const HitInfo& hit) override {
		TryCollect(hit.other);
	}

	void OnCollisionEnter(const HitInfo& hit) override {
		TryCollect(hit.other);
	}

private:
	void TryCollect(const EntityRef& other) {
		if(collected || !other.IsValid()) return;
		ComponentRef<PlatformerCharacterController> player(other);
		if(!player.IsValid()) return;

		if(!manager.IsValid()) manager = PlatformerSceneAccess::FindFirst<PlatformerGameManager>(m_ref.GetScene());
		if(auto* game = manager.TryGet()) {
			if(!game->CollectCoin()) return;
		}

		collected = true;
		collectTimer = 0.0f;
		if(auto* t = transform.TryGet()) {
			t->position = basePosition;
			t->scale = baseScale;
			if(auto* effect = particle.TryGet()) {
				for(auto& state : effect->Particle) state.LifeTime = 0.0f;
				effect->particleSize = 0.135f;
				PlatformerFeedback::LayeredBurst(
					effect,
					Vector3(),
					112,
					5.8f,
					7.4f,
					0.88f,
					DirectX::XMFLOAT4(1.0f, 0.92f, 0.26f, 1.0f),
					DirectX::XMFLOAT4(1.0f, 0.28f, 0.02f, 1.0f));
			}
		}
		if(!camera.IsValid()) camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		if(auto* cameraController = camera.TryGet()) {
			cameraController->AddImpulse(0.16f, 0.16f, 0.026f, Vector3(0.0f, 1.0f, 0.0f));
		}
		PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene());
	}

	ComponentRef<TransformComponent> transform;
	ComponentRef<MaterialComponent> material;
	ComponentRef<ParticleComponent> particle;
	ComponentRef<AudioComponent> audio;
	ComponentRef<PlatformerGameManager> manager;
	ComponentRef<PlatformerCameraController> camera;
	Vector3 basePosition;
	Vector3 baseScale = Vector3(1.0f, 1.0f, 1.0f);
	MATERIAL baseMaterial{};
	float elapsed = 0.0f;
	float collectTimer = 0.0f;
	float idleSparkTimer = 0.0f;
	bool registered = false;
	bool collected = false;
	bool destroyQueued = false;
};