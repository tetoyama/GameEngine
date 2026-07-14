#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/audioComponent.h"
#include "Engine/Scene/Component/particleComponent.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerFeedback.h"

#include <algorithm>
#include <cmath>

class PlatformerCheckpoint : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerCheckpoint)
		REFLECT_FIELD(Vector3, respawnOffset, Vector3(0.0f, 1.1f, 0.0f))
		REFLECT_FIELD(float, activationScale, 1.25f)
		REFLECT_FIELD(float, pulseSpeed, 3.0f)

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
		ImGui::Text("Platformer Checkpoint");
		INSPECTOR_FIELDS();
		ImGui::Text("Activated: %s", activated ? "true" : "false");
	}

	void OnStart() override {
		transform = GetComponentRef<TransformComponent>();
		particle = GetComponentRef<ParticleComponent>();
		audio = GetComponentRef<AudioComponent>();
		if(auto* t = transform.TryGet()) baseScale = t->scale;
	}

	void OnUpdate(float dt) override {
		auto* t = transform.TryGet();
		if(!t) return;
		if(activated) {
			pulseTime += dt;
			const float pulse = 1.0f + 0.035f * std::sin(pulseTime * pulseSpeed * DirectX::XM_2PI);
			t->scale = baseScale * activationScale * pulse;
			t->AddRotationY(dt * 0.7f);
		}
	}

	void OnTriggerEnter(const HitInfo& hit) override {
		Activate(hit.other);
	}

	void OnCollisionEnter(const HitInfo& hit) override {
		Activate(hit.other);
	}

private:
	void Activate(const EntityRef& other) {
		if(!other.IsValid()) return;
		ComponentRef<PlatformerCharacterController> player(other);
		auto* controller = player.TryGet();
		auto* t = transform.TryGet();
		if(!controller || !t) return;

		const Vector3 target = t->position + respawnOffset;
		const bool changed = !activated || (controller->GetCheckpoint() - target).length() > 0.01f;
		controller->SetCheckpoint(target);
		if(!changed) return;

		activated = true;
		pulseTime = 0.0f;
		PlatformerFeedback::Burst(particle.TryGet(), t->position + Vector3(0.0f, 0.5f, 0.0f), 24, 3.1f, 4.2f, 0.8f);
		PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene());
	}

	ComponentRef<TransformComponent> transform;
	ComponentRef<ParticleComponent> particle;
	ComponentRef<AudioComponent> audio;
	Vector3 baseScale = Vector3(1.0f, 1.0f, 1.0f);
	float pulseTime = 0.0f;
	bool activated = false;
};
