#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

class ParticleComponent : public IComponent {
public:
	YAML::Node encode() override {
		YAML::Node node;

		return node;
	}

	bool decode(const YAML::Node& node) override {
		if (!node.IsMap()) { return false; }

		return true;
	}

	void inspector(SceneContext* context) override {

	}

	float particleLifeTime = 1.0f; // パーティクルのライフタイム
	float particleSize = 1.0f; // パーティクルのサイズ
	float particleSpeed = 1.0f; // パーティクルの速度
};
