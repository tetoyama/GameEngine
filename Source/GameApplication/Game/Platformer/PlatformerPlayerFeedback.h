#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
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
		ImGui::Text("Platformer Player Feedback");
		INSPECTOR_FIELDS();
	}

	void OnStart() override {
		player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		transform = GetComponentRef<TransformComponent>();
		particle = GetComponentRef<ParticleComponent>();
		audio = GetComponentRef<AudioComponent>();
		if(auto* controller = player.TryGet()) CaptureRevisions(*controller);
	}

	void OnFixedUpdate(float dt) override {
		if(!player.IsValid()) player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		auto* controller = player.TryGet();
		ComponentRef<TransformComponent> playerTransform(player.GetEntityRef());
		auto* playerPose = playerTransform.TryGet();
		if(!controller || !playerPose) return;

		const Vector3 origin = playerPose->position + Vector3(0.0f, particleVerticalOffset, 0.0f);

		if(controller->GetJumpEventRevision() != jumpRevision) {
			jumpRevision = controller->GetJumpEventRevision();
			const int stage = controller->GetJumpStage();
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
			const float impact = std::clamp((std::abs(controller->GetVerticalVelocity()) - 1.5f) / 10.0f, 0.25f, 1.0f);
			const int count = 16 + static_cast<int>(impact * 24.0f);
			EmitAt(playerPose->position, count, 2.4f + impact * 2.4f, 1.0f + impact * 1.1f, 0.38f + impact * 0.18f, 0.12f + impact * 0.05f);
			Impulse(0.08f + impact * 0.20f, 0.11f + impact * 0.09f, -0.008f - impact * 0.015f, Vector3(0.0f, -1.0f, 0.0f));
		}

		if(controller->GetWallKickEventRevision() != wallRevision) {
			wallRevision = controller->GetWallKickEventRevision();
			const Vector3 wallNormal = controller->GetLastWallNormal();
			const Vector3 wallOrigin = origin - wallNormal * 0.3f;
			EmitAt(wallOrigin, 34, 4.2f, 5.0f, 0.68f, 0.165f);
			Impulse(0.21f, 0.17f, 0.025f, wallNormal);
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ActionPath);
		}

		if(controller->GetStompEventRevision() != stompRevision) {
			stompRevision = controller->GetStompEventRevision();
			EmitAt(playerPose->position, 52, 5.2f, 6.4f, 0.82f, 0.20f);
			Impulse(0.38f, 0.23f, 0.050f, Vector3(0.0f, -1.0f, 0.0f));
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ImpactPath);
		}

		if(controller->GetDamageEventRevision() != damageRevision) {
			damageRevision = controller->GetDamageEventRevision();
			EmitAt(origin, 56, 5.4f, 5.2f, 0.88f, 0.21f);
			Impulse(0.48f, 0.28f, 0.060f);
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ImpactPath);
		}

		if(controller->GetRespawnEventRevision() != respawnRevision) {
			respawnRevision = controller->GetRespawnEventRevision();
			EmitAt(origin, 46, 4.2f, 6.6f, 1.05f, 0.17f);
			Impulse(0.18f, 0.30f, 0.022f, Vector3(0.0f, 1.0f, 0.0f));
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::CheckpointPath);
		}
	}

private:
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

	ComponentRef<PlatformerCharacterController> player;
	ComponentRef<PlatformerCameraController> camera;
	ComponentRef<TransformComponent> transform;
	ComponentRef<ParticleComponent> particle;
	ComponentRef<AudioComponent> audio;
	uint32_t jumpRevision = 0;
	uint32_t landRevision = 0;
	uint32_t wallRevision = 0;
	uint32_t stompRevision = 0;
	uint32_t damageRevision = 0;
	uint32_t respawnRevision = 0;
};