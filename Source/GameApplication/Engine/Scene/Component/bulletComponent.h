#pragma once
#include "Interface/IComponent.h"
#include "Backends/myVector3.h"

class BulletComponent: public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;
		return node;
	}

	bool decode(const YAML::Node& node) override{
		return true;
	}
	float maxLifeTime = 2.5f;
	float currentLifeTime = 0.0f;
	float bulletSpeed = 20.0f;
};
