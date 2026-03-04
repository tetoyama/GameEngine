// =======================================================================
// 
// entityNameComponent.h
// 
// =======================================================================
#pragma once
#include "Interface/IComponent.h"
#include "Backends/YAMLConverters.h"

#include <string>

// エンティティの名前を管理するコンポーネント
class NameComponent: public IComponent {
public:

	YAML::Node encode() override{
		YAML::Node node;
		node["Name"] = name;
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{

		if(node["Name"]){
			name = node["Name"].as<std::string>();
		}
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text(name.c_str());
	}

	std::string name;
};
