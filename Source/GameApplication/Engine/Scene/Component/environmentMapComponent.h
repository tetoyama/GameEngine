// =======================================================================
// 
// environmentMapComponent.h
// 
// =======================================================================
#pragma once
#include "Interface/IComponent.h"
#include "BackEnds/YAMLConverters.h"

// EnvironmentMapComponent
// このコンポーネントを持つエンティティの TextureComponent テクスチャを
// 環境マップ（PBRメタリック反射）として使用する
class EnvironmentMapComponent : public IComponent {
public:
	YAML::Node encode() override {
		YAML::Node node;
		node["enabled"] = enabled;
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		if (node["enabled"])
			enabled = node["enabled"].as<bool>();
		return true;
	}

	void inspector(SceneContext* context) override {
		ImGui::Checkbox("Enabled", &enabled);
		ImGui::TextDisabled("TextureComponent のテクスチャが環境マップとして使われます");
	}

	bool enabled = true;
};
