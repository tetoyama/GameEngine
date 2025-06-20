#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

class ExplosionEffectComponent : public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;
		node["Component"] = "ExplosionEffectComponent";

		return node;
	}

	bool decode(const YAML::Node& node) override{
		return true;
	}
	// è¡Ç¶ÇÈÇ‹Ç≈ÇÃéûä‘
	float LifeTime = 0.5f;

	// ê∂ê¨Ç©ÇÁÇÃåoâﬂéûä‘
	float Timer = 0.0f;
};
