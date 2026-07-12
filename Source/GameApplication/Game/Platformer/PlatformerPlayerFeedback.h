#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/audioComponent.h"
#include "Engine/Scene/Component/particleComponent.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerFeedback.h"
#include "Game/Platformer/PlatformerSceneAccess.h"
#include "Game/Platformer/PlatformerSoundLibrary.h"

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
			const bool triple = controller->GetJumpStage() == 3;
			EmitAt(origin, triple ? 30 : 12, triple ? 4.2f : 2.0f, triple ? 5.0f : 2.8f, triple ? 0.8f : 0.4f);
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ActionPath);
		}

		if(controller->GetLandEventRevision() != landRevision) {
			landRevision = controller->GetLandEventRevision();
			EmitAt(playerPose->position, 10, 2.2f, 1.2f, 0.35f);
		}

		if(controller->GetWallKickEventRevision() != wallRevision) {
			wallRevision = controller->GetWallKickEventRevision();
			const Vector3 wallOrigin = origin - controller->GetLastWallNormal() * 0.3f;
			EmitAt(wallOrigin, 24, 3.8f, 4.4f, 0.65f);
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ActionPath);
		}

		if(controller->GetStompEventRevision() != stompRevision) {
			stompRevision = controller->GetStompEventRevision();
			EmitAt(playerPose->position, 28, 4.0f, 5.2f, 0.75f);
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ImpactPath);
		}

		if(controller->GetDamageEventRevision() != damageRevision) {
			damageRevision = controller->GetDamageEventRevision();
			EmitAt(origin, 32, 4.6f, 4.8f, 0.8f);
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ImpactPath);
		}

		if(controller->GetRespawnEventRevision() != respawnRevision) {
			respawnRevision = controller->GetRespawnEventRevision();
			EmitAt(origin, 36, 4.0f, 6.0f, 1.0f);
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::CheckpointPath);
		}
	}

private:
	void EmitAt(
		const Vector3& worldOrigin,
		int count,
		float horizontalSpeed,
		float upwardSpeed,
		float lifetime
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
		PlatformerFeedback::Burst(
			feedbackParticle,
			Vector3(0.0f, 0.0f, 0.0f),
			count,
			horizontalSpeed,
			upwardSpeed,
			lifetime);
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
