// =======================================================================
// 
// RenderLayerComponent.h
// 
// =======================================================================
#pragma once
#include "Interface/IComponent.h"
#include "Backends/YAMLConverters.h"
#include "System/Render/RenderSystem/renderLayer.h"

// 描画レイヤーを管理するコンポーネント
class RenderLayerComponent {
public:
	YAML::Node encode(){
		YAML::Node node;
		node["layer"] = static_cast<int>(layer);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node){
		(void)context;
		if(!node["layer"]) return false;
		layer = static_cast<RenderLayer>(node["layer"].as<int>());
		return true;
	}

	void inspector(SceneContext* context){
		(void)context;
		static const char* items[] = {
			"Background2D", "Opaque3D", "Transparent3D",
			"SortTransparent3D", "OverlayUI", "Debug"
		};
		int currentLayer = static_cast<int>(layer);

		if(ImGui::Combo("Render Layer", &currentLayer, items, IM_ARRAYSIZE(items))){
			layer = static_cast<RenderLayer>(currentLayer);
		}
	}

	RenderLayer layer = RenderLayer::Opaque3D;
};

// レイヤー内の描画順序を管理するコンポーネント
class OrderInLayerComponent {
public:
	int order = 0;

	YAML::Node encode(){
		YAML::Node node;
		node["order"] = order;
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node){
		(void)context;
		// 後方互換: 旧キー "layer" もフォールバックで読み込む
		if(node["order"]){
			order = node["order"].as<int>();
		} else if(node["layer"]){
			order = node["layer"].as<int>();
		} else {
			return false;
		}
		return true;
	}

	void inspector(SceneContext* context){
		(void)context;
		ImGui::UndoDragInt("Order In Layer", &order, 1.0f, -1000, 1000);
	}
};