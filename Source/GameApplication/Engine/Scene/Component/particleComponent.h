#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

#define MAXPARTICLE 256

struct PARTICLE
{
	Vector3 Position = Vector3(0,0,0);
	float LifeTime = 0.0f;
};

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

	PARTICLE Particle[MAXPARTICLE];

	float particleLifeTime = 1.0f; // パーティクルのライフタイム
	float particleSize = 1.0f; // パーティクルのサイズ
	float particleSpeed = 1.0f; // パーティクルの速度
};
