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
		auto* feedbackPose = transform.TryGet();
		if(!controller || !playerPose) return;
		if(feedbackPose) feedbackPose->position = playerPose->position;

		const Vector3 origin = playerPose->position + Vector3(0.0f, particleVerticalOffset, 0.0f);

		if(controller->GetJumpEventRevision() != jumpRevision) {
			jumpRevision = controller->GetJumpEventRevision();
			const bool triple = controller->GetJumpStage() == 3;
			PlatformerFeedback::Burst(particle.TryGet(), origin, triple ? 30 : 12, triple ? 4.2f : 2.0f, triple ? 5.0f : 2.8f, triple ? 0.8f : 0.4f);
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ActionPath);
		}

		if(controller->GetLandEventRevision() != landRevision) {
			landRevision = controller->GetLandEventRevision();
			PlatformerFeedback::Burst(particle.TryGet(), playerPose->position, 10, 2.2f, 1.2f, 0.35f);
		}

		if(controller->GetWallKickEventRevision() != wallRevision) {
			wallRevision = controller->GetWallKickEventRevision();
			const Vector3 wallOrigin = origin - controller->GetLastWallNormal() * 0.3f;
			PlatformerFeedback::Burst(particle.TryGet(), wallOrigin, 24, 3.8f, 4.4f, 0.65f);
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ActionPath);
		}

		if(controller->GetStompEventRevision() != stompRevision) {
			stompRevision = controller->GetStompEventRevision();
			PlatformerFeedback::Burst(particle.TryGet(), playerPose->position, 28, 4.0f, 5.2f, 0.75f);
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ImpactPath);
		}

		if(controller->GetDamageEventRevision() != damageRevision) {
			damageRevision = controller->GetDamageEventRevision();
			PlatformerFeedback::Burst(particle.TryGet(), origin, 32, 4.6f, 4.8f, 0.8f);
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::ImpactPath);
		}

		if(controller->GetRespawnEventRevision() != respawnRevision) {
			respawnRevision = controller->GetRespawnEventRevision();
			PlatformerFeedback::Burst(particle.TryGet(), origin, 36, 4.0f, 6.0f, 1.0f);
			PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene(), PlatformerSoundLibrary::CheckpointPath);
		}
	}

private:
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
