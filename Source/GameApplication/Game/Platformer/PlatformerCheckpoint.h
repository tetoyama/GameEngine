#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/audioComponent.h"
#include "Engine/Scene/Component/particleComponent.h"
#include "Game/Platformer/PlatformerCameraController.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerFeedback.h"
#include "Game/Platformer/PlatformerSceneAccess.h"

#include <algorithm>
#include <cmath>

class PlatformerCheckpoint : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerCheckpoint)
		REFLECT_FIELD(Vector3, respawnOffset, Vector3(0.0f, 1.1f, 0.0f))
		REFLECT_FIELD(float, activationScale, 1.25f)
		REFLECT_FIELD(float, pulseSpeed, 3.0f)
		REFLECT_FIELD(float, beaconInterval, 0.72f)

public:
	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		DECODE_FIELDS(node);
		activationScale = (std::max)(1.0f, activationScale);
		pulseSpeed = (std::max)(0.1f, pulseSpeed);
		beaconInterval = std::clamp(beaconInterval, 0.30f, 2.0f);
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
		camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		if(auto* t = transform.TryGet()) baseScale = t->scale;
		beaconTimer = beaconInterval;
	}

	void OnUpdate(float dt) override {
		auto* t = transform.TryGet();
		if(!t) return;
		const float safeDt = (std::max)(0.0f, dt);
		if(!activated) return;

		pulseTime += safeDt;
		const float wave = 0.5f + 0.5f * std::sin(pulseTime * pulseSpeed * DirectX::XM_2PI);
		const float pulse = 1.0f + 0.045f * wave;
		t->scale = baseScale * activationScale * pulse;
		t->AddRotationY(safeDt * (0.75f + wave * 0.55f));

		beaconTimer -= safeDt;
		if(beaconTimer > 0.0f) return;
		beaconTimer = beaconInterval;
		if(auto* effect = particle.TryGet()) {
			effect->particleSize = 0.075f;
			PlatformerFeedback::DirectionalBurst(
				effect,
				Vector3(0.0f, 0.55f, 0.0f),
				Vector3(0.0f, 1.0f, 0.0f),
				26,
				2.4f,
				1.5f,
				3.8f,
				0.72f,
				DirectX::XMFLOAT4(0.18f, 1.0f, 0.72f, 1.0f),
				DirectX::XMFLOAT4(0.16f, 0.55f, 1.0f, 1.0f));
		}
	}

	void OnStop() override {
		if(auto* t = transform.TryGet()) t->scale = baseScale;
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
		beaconTimer = beaconInterval * 0.55f;
		if(auto* effect = particle.TryGet()) {
			for(auto& state : effect->Particle) state.LifeTime = 0.0f;
			effect->particleSize = 0.15f;
			// The checkpoint owns this emitter, so the burst origin must remain local.
			PlatformerFeedback::LayeredBurst(
				effect,
				Vector3(0.0f, 0.50f, 0.0f),
				128,
				5.2f,
				8.2f,
				1.12f,
				DirectX::XMFLOAT4(0.22f, 1.0f, 0.68f, 1.0f),
				DirectX::XMFLOAT4(0.12f, 0.48f, 1.0f, 1.0f));
		}
		if(!camera.IsValid()) camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		if(auto* cameraController = camera.TryGet()) {
			cameraController->AddImpulse(0.28f, 0.28f, 0.038f, Vector3(0.0f, 1.0f, 0.0f));
		}
		PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene());
	}

	ComponentRef<TransformComponent> transform;
	ComponentRef<ParticleComponent> particle;
	ComponentRef<AudioComponent> audio;
	ComponentRef<PlatformerCameraController> camera;
	Vector3 baseScale = Vector3(1.0f, 1.0f, 1.0f);
	float pulseTime = 0.0f;
	float beaconTimer = 0.0f;
	bool activated = false;
};