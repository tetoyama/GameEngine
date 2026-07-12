#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"

// The platformer stage is authored directly in PlatformerTechDemo.scene.
// This component remains registered only so older scenes containing the
// previous runtime builder can still be loaded without an unknown-component
// error. It deliberately performs no runtime entity generation.
class PlatformerStageBuilder : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerStageBuilder)
		REFLECT_FIELD(bool, buildOnStart, false)
		REFLECT_FIELD(int, stageRevision, 2)

public:
	YAML::Node encode() override {
		YAML::Node node;
		buildOnStart = false;
		ENCODE_FIELDS(node);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		DECODE_FIELDS(node);
		buildOnStart = false;
		if(stageRevision < 2) stageRevision = 2;
		return true;
	}

	void inspector(SceneContext* context) override {
		ImGui::Text("Platformer Stage");
		ImGui::TextWrapped(
			"The stage is baked into PlatformerTechDemo.scene. "
			"Runtime stage generation is disabled.");
		ImGui::Text("Stage Revision: %d", stageRevision);
	}

	void OnStart() override {
		// Intentionally empty. Stage entities are loaded from the scene file.
		buildOnStart = false;
	}
};
