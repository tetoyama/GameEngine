#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"
#include "Backends/myVector2.h"

class SpriteRendererComponent: public IComponent {
public:
	Vector2 anchor = {0.5f, 0.5f}; // 中央
	Vector2 pivot = {0.0f, 0.0f};  // 中心

	YAML::Node encode() override{
		YAML::Node node;
		node["anchor"] = anchor;
		node["pivot"] = pivot;

		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{
		if(node["anchor"])
			anchor = node["anchor"].as<Vector2>();

		if(node["pivot"])
			pivot = node["pivot"].as<Vector2>();
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text("RectTransform");

		ImGui::DragFloat2("Anchor", &anchor.x, 0.01f, 0.0f, 1.0f, "%.2f");
		ImGui::DragFloat2("Pivot", &pivot.x, 0.01f, -0.5f, 0.5f, "%.2f");

	}
};
