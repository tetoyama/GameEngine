#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"

#include <algorithm>

// Compatibility component retained for old serialized data. The complete
// platformer course is authored directly in PlatformerTechDemo.scene.
class PlatformerStageBuilder : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerStageBuilder)
		REFLECT_FIELD(bool, buildOnStart, false)
		REFLECT_FIELD(int, stageRevision, 3)

public:
	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		(void)context;
		DECODE_FIELDS(node);
		stageRevision = (std::max)(3, stageRevision);
		buildOnStart = false;
		return true;
	}

	void inspector(SceneContext* context) override {
		(void)context;
		ImGui::Text("Platformer Stage");
		ImGui::TextWrapped(
			"The complete course is baked directly into PlatformerTechDemo.scene.");
		ImGui::Text("Stage Revision: %d", stageRevision);
	}

	void OnStart() override {
		buildOnStart = false;
	}
};
