#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

class BulletComponent: public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;
		node["Component"] = "BulletComponent";

		return node;
	}

	bool decode(const YAML::Node& node) override{
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text("BulletComponent");
	}

	float maxLifeTime = 2.5f;
	float currentLifeTime = 0.0f;
	float bulletSpeed = 20.0f;
};
