#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

class PlayerComponent : public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;
		node["Component"] = "PlayerComponent";

		node["moveSpeed"] = moveSpeed;
		node["cameraRotate"] = cameraRotate;
		node["cameraDistance"] = cameraDistance;
		node["cameraLerp"] = cameraLerp;
		return node;
	}

	bool decode(const YAML::Node& node) override{
		if (!node.IsMap()) { return false; }

		if (node["moveSpeed"]) {
			moveSpeed = node["moveSpeed"].as<float>();
		}
		if (node["cameraRotate"]) {
			cameraRotate = node["cameraRotate"].as<float>();
		}
		if (node["cameraDistance"]) {
			cameraDistance = node["cameraDistance"].as<float>();
		}
		if (node["cameraLerp"]) {
			cameraLerp = node["cameraLerp"].as<float>();
		}
		return true;
	}
	float moveSpeed = 5.0f;
	float cameraRotate = 2.0f;
	float cameraDistance = 20.0f;
	float cameraLerp = 8.0f;
};
