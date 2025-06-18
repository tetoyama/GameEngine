#pragma once
#include "Interface/IComponent.h"
#include "Backends/myVector3.h"

class PlayerComponent : public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;
		return node;
	}

	bool decode(const YAML::Node& node) override{
		return true;
	}
	float moveSpeed = 5.0f;
	float cameraRotate = 2.0f;
	float cameraDistance = 20.0f;
	float cameraLerp = 8.0f;
};
