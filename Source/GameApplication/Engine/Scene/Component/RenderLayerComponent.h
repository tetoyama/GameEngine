#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

enum class RenderLayer: int {
	Background2D = 0,
	Opaque3D,
	Transparent3D,
	SortTransparent3D,
	OverlayUI,
	Debug,
	MaxRenderLayer
};

// レイヤー名を文字列で取得するユーティリティ
inline const char* GetRenderLayerName(const RenderLayer& layer) {
	switch (layer) {
		case RenderLayer::Background2D:        return "Background 2D";
		case RenderLayer::Opaque3D:            return "Opaque 3D";
		case RenderLayer::SortTransparent3D:   return "Sorted Transparent 3D";
		case RenderLayer::Transparent3D:       return "Transparent 3D";
		case RenderLayer::OverlayUI:           return "Overlay UI";
		case RenderLayer::Debug:               return "Debug";
		default:                               return "Unknown";
	}
}

class RenderLayerComponent: public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;
		node["layer"] = static_cast<int>(layer);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{
		if(!node["layer"]) return false;
		layer = static_cast<RenderLayer>(node["layer"].as<int>());
		return true;
	}

	void inspector(SceneContext* context) override{
		static const char* items[] = {"Background2D","Opaque3D", "Transparent3D","SortTransparent3D", "OverlayUI", "Debug"};
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

	bool decode(SceneContext* context, const YAML::Node& node) override{
		if(!node["layer"]) return false;
		order = node["layer"].as<int>();
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::DragInt("Order In Layer", &order, 1.0f, -1000, 1000, "%d");

	}
};
