// =======================================================================
// 
// environmentMapComponent.h
// 
// =======================================================================
#pragma once
#include "Interface/IComponent.h"
#include "Backends/YAMLConverters.h"

// EnvironmentMapComponent
// このコンポーネントを持つエンティティの TextureComponent テクスチャを
// 環境マップ（PBRメタリック反射）として使用する
class EnvironmentMapComponent {
public:
	YAML::Node encode(){
		YAML::Node node;
		node["enabled"] = enabled;
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node){
		(void)context;
		if(node["enabled"]){
			enabled = node["enabled"].as<bool>();
		}
		return true;
	}

	void inspector(SceneContext* context){
		(void)context;
		ImGui::Checkbox("Enabled", &enabled);
		ImGui::TextDisabled("TextureComponent のテクスチャが環境マップとして使われます");
	}

	bool enabled = true;
};