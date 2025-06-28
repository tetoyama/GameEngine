#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

#include "Backends/myVector3.h"

class EnemyComponent: public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;
		node["setOriginPos"] = setOriginPos;
		node["OriginPos"] = OriginPos;
		node["maxDistance"] = maxDistance;
		node["TargetPos"] = TargetPos;
		node["MoveSpeed"] = MoveSpeed;

		return node;
	}

	bool decode(const YAML::Node& node) override{

		if(node["setOriginPos"]){
			setOriginPos = node["setOriginPos"].as<bool>();
		}

		if(node["OriginPos"]){
			OriginPos = node["OriginPos"].as<Vector3>();
		}

		if(node["maxDistance"]){
			maxDistance = node["maxDistance"].as<int>();
		}

		if(node["TargetPos"]){
			TargetPos = node["TargetPos"].as<Vector3>();
		}

		if(node["MoveSpeed"]){
			MoveSpeed = node["MoveSpeed"].as<float>();
		}
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text("EnemyComponent");
	}

	bool setOriginPos = false;
	Vector3 OriginPos = Vector3(0.0f,0.0f,0.0f);
	int maxDistance = 15;
	Vector3 TargetPos = Vector3(0.0f,0.0f,0.0f);

	float MoveSpeed = 1.0f;
};
