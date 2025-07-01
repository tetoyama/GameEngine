#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

enum class RenderLayer: int {
	Opaque3D = 0,
	Transparent3D = 1,
	UI = 2,
	Debug = 3,
};

class RenderLayerComponent: public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;
		node["layer"] = static_cast<int>(layer);
		return node;
	}

	bool decode(const YAML::Node& node) override{
		if(!node["layer"]) return false;
		layer = static_cast<RenderLayer>(node["layer"].as<int>());
		return true;
	}

	void inspector(SceneContext* context) override{
		static const char* items[] = {"Opaque3D", "Transparent3D", "UI", "Debug"};
		int currentLayer = static_cast<int>(layer);

		if(ImGui::Combo("Render Layer", &currentLayer, items, IM_ARRAYSIZE(items))){
			layer = static_cast<RenderLayer>(currentLayer);
		}
	}

	RenderLayer layer = RenderLayer::Opaque3D;

};



class OrderInLayerComponent: public IComponent {
public:
	int order = 0;

	YAML::Node encode() override{
		YAML::Node node;
		node["layer"] = order;
		return node;
	}

	bool decode(const YAML::Node& node) override{
		if(!node["layer"]) return false;
		order = node["layer"].as<int>();
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::DragInt("Order In Layer", &order, 1.0f, -1000, 1000, "%d");

	}
};
