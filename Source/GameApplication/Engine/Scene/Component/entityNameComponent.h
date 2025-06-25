#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

#include <string>

class NameComponent: public IComponent {
public:

	YAML::Node encode() override{
		YAML::Node node;
		node["Component"] = "NameComponent";
		node["Name"] = name;
		return node;
	}

	bool decode(const YAML::Node& node) override{

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
