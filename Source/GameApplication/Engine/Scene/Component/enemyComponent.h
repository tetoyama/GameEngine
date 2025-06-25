#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

#include "Backends/myVector3.h"

class EnemyComponent: public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;
		node["Component"] = "EnemyComponent";

		return node;
	}

	bool decode(const YAML::Node& node) override{
		return true;
	}

	void inspector() override{
		ImGui::Text("EnemyComponent");
	}

	bool setOriginPos = false;
	Vector3 OriginPos = Vector3(0.0f,0.0f,0.0f);
	int maxDistance = 15;
	Vector3 TargetPos = Vector3(0.0f,0.0f,0.0f);

	float MoveSpeed = 1.0f;
};
