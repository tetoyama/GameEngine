#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/audioComponent.h"
#include "Engine/Scene/Component/particleComponent.h"
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

public:
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
		ImGui::Text("Platformer Coin");
		INSPECTOR_FIELDS();
		ImGui::Text("Collected: %s", collected ? "true" : "false");
	}

	void OnStart() override {
		transform = GetComponentRef<TransformComponent>();
		particle = GetComponentRef<ParticleComponent>();
		audio = GetComponentRef<AudioComponent>();
		manager = PlatformerSceneAccess::FindFirst<PlatformerGameManager>(m_ref.GetScene());
		if(auto* t = transform.TryGet()) {
			basePosition = t->position;
			baseScale = t->scale;
		}
		if(auto* game = manager.TryGet()) game->RegisterCoin();
		registered = true;
	}

	void OnUpdate(float dt) override {
		auto* t = transform.TryGet();
		if(!t) return;
		elapsed += (std::max)(0.0f, dt);

		if(!collected) {
			t->position.y = basePosition.y + std::sin(elapsed * bobFrequency * DirectX::XM_2PI) * bobAmplitude;
			t->AddRotationY(rotationSpeed * dt);
			return;
		}

		collectTimer += dt;
		const float normalized = collectDuration > 0.0f
			? std::clamp(collectTimer / collectDuration, 0.0f, 1.0f)
			: 1.0f;

		// Keep the emitter fixed at the collection point. Particle positions are
		// local to this entity, so moving or shrinking the coin Transform would
		// drag and rescale the burst after it has already spawned.
		t->position = basePosition;
		t->scale = baseScale;
		t->AddRotationY(rotationSpeed * dt * 3.5f);

		if(normalized >= 1.0f && !destroyQueued) {
			destroyQueued = QueueDestroySelf();
		}
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
			// Snap out of the bob cycle and emit from local zero. Passing t->position
			// here would apply the world position a second time in RenderableParticle.
			t->position = basePosition;
			t->scale = baseScale;
			PlatformerFeedback::Burst(
				particle.TryGet(),
				Vector3(0.0f, 0.0f, 0.0f),
				18,
				2.6f,
				3.6f,
				collectDuration);
		}
		PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene());
	}

	ComponentRef<TransformComponent> transform;
	ComponentRef<ParticleComponent> particle;
	ComponentRef<AudioComponent> audio;
	ComponentRef<PlatformerGameManager> manager;
	Vector3 basePosition;
	Vector3 baseScale = Vector3(1.0f, 1.0f, 1.0f);
	float elapsed = 0.0f;
	float collectTimer = 0.0f;
	bool registered = false;
	bool collected = false;
	bool destroyQueued = false;
};
