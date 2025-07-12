#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"
#include "Shader/CommonBuffer.h"

class AudioComponent : public IComponent {
public:
	YAML::Node encode() override {
		YAML::Node node;


		//node["Enable"] = light.Enable;

		return node;
	}

	bool decode(const YAML::Node& node) override {
		if (!node.IsMap()) {
			return false;
		}

		//if (node["Enable"])
		//	light.Enable = node["Enable"].as<BOOL>();

		return true;
	}

	void inspector(SceneContext* context) {
		
	}


};
