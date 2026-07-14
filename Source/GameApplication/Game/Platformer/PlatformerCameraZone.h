#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Game/Platformer/PlatformerCameraController.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerFeedback.h"
#include "Game/Platformer/PlatformerSceneAccess.h"

#include <algorithm>

class PlatformerCameraZone : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerCameraZone)
		REFLECT_FIELD(int, profile, 0)
		REFLECT_FIELD(int, exitProfile, 0)
		REFLECT_FIELD(bool, restoreOnExit, false)
		REFLECT_FIELD(bool, transitionFeedbackEnabled, true)

public:
	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		DECODE_FIELDS(node);
		profile = std::clamp(profile, 0, 4);
		exitProfile = std::clamp(exitProfile, 0, 4);
		return true;
	}

	void inspector(SceneContext* context) override {
		ImGui::Text("Platformer Camera Zone");
		INSPECTOR_FIELDS();
		ImGui::Text("Feedback Cooldown: %.2f", feedbackCooldown);
	}

	void OnStart() override {
		camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
	}

	void OnUpdate(float dt) override {
		feedbackCooldown = (std::max)(0.0f, feedbackCooldown - (std::max)(0.0f, dt));
	}

	void OnTriggerEnter(const HitInfo& hit) override {
		if(!IsPlayer(hit.other)) return;
		if(!camera.IsValid()) camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		if(auto* controller = camera.TryGet()) {
			controller->SetProfile(static_cast<PlatformerCameraController::Profile>(std::clamp(profile, 0, 4)));
		}
		EmitTransitionFeedback(hit.other, profile, true);
	}

	void OnTriggerExit(const HitInfo& hit) override {
		if(!restoreOnExit || !IsPlayer(hit.other)) return;
		if(auto* controller = camera.TryGet()) {
			controller->SetProfile(static_cast<PlatformerCameraController::Profile>(std::clamp(exitProfile, 0, 4)));
		}
		EmitTransitionFeedback(hit.other, exitProfile, false);
	}

private:
	static bool IsPlayer(const EntityRef& entity) {
		return ComponentRef<PlatformerCharacterController>(entity).IsValid();
	}

	void EmitTransitionFeedback(const EntityRef& playerEntity, int targetProfile, bool entering) {
		if(!transitionFeedbackEnabled || feedbackCooldown > 0.0f) return;
		ComponentRef<TransformComponent> playerTransform(playerEntity);
		auto* pose = playerTransform.TryGet();
		if(!pose) return;

		const DirectX::XMFLOAT4 primary = targetProfile >= 3
			? DirectX::XMFLOAT4(1.0f, 0.22f, 0.62f, 1.0f)
			: (targetProfile == 2
				? DirectX::XMFLOAT4(0.72f, 0.28f, 1.0f, 1.0f)
				: DirectX::XMFLOAT4(0.18f, 0.82f, 1.0f, 1.0f));
		const DirectX::XMFLOAT4 secondary = entering
			? DirectX::XMFLOAT4(0.20f, 1.0f, 0.72f, 1.0f)
			: DirectX::XMFLOAT4(1.0f, 0.72f, 0.18f, 1.0f);
		const Vector3 direction = entering
			? Vector3(0.0f, 1.0f, 0.0f)
			: Vector3(0.0f, -0.35f, 0.0f);

		PlatformerFeedback::SharedDirectionalBurst(
			m_ref.GetScene(),
			pose->position + Vector3(0.0f, 0.75f, 0.0f),
			direction,
			48,
			3.2f,
			3.0f,
			3.4f,
			0.62f,
			0.085f,
			primary,
			secondary);
		if(auto* controller = camera.TryGet()) {
			controller->AddImpulse(
				0.065f,
				0.15f,
				entering ? 0.010f : -0.006f,
				direction);
		}
		feedbackCooldown = 0.24f;
	}

	ComponentRef<PlatformerCameraController> camera;
	float feedbackCooldown = 0.0f;
};