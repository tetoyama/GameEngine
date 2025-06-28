#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

class ExplosionEffectComponent : public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;
		node["LifeTime"] = LifeTime;
		node["Timer"] = Timer;

		return node;
	}

	bool decode(const YAML::Node& node) override{

		if(node["LifeTime"]){
			LifeTime = node["LifeTime"].as<float>();
		}
		if(node["Timer"]){
			Timer = node["Timer"].as<float>();
		}
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text("ExplosionEffectComponent");
	}

	// 消えるまでの時間
	float LifeTime = 0.5f;

	// 生成からの経過時間
	float Timer = 0.0f;
};
