#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Game/Platformer/PlatformerCameraController.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerSceneAccess.h"

#include <algorithm>

class PlatformerCameraZone : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerCameraZone)
		REFLECT_FIELD(int, profile, 0)
		REFLECT_FIELD(int, exitProfile, 0)
		REFLECT_FIELD(bool, restoreOnExit, false)

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
	}

	void OnStart() override {
		camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
	}

	void OnTriggerEnter(const HitInfo& hit) override {
		if(!IsPlayer(hit.other)) return;
		if(!camera.IsValid()) camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		if(auto* controller = camera.TryGet()) {
			controller->SetProfile(static_cast<PlatformerCameraController::Profile>(std::clamp(profile, 0, 4)));
		}
	}

	void OnTriggerExit(const HitInfo& hit) override {
		if(!restoreOnExit || !IsPlayer(hit.other)) return;
		if(auto* controller = camera.TryGet()) {
			controller->SetProfile(static_cast<PlatformerCameraController::Profile>(std::clamp(exitProfile, 0, 4)));
		}
	}

private:
	static bool IsPlayer(const EntityRef& entity) {
		return ComponentRef<PlatformerCharacterController>(entity).IsValid();
	}

	ComponentRef<PlatformerCameraController> camera;
};
