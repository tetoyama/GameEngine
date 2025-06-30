#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

class SpriteRendererComponent : public IComponent {
public:
	YAML::Node encode() override {
		YAML::Node node;

		return node;
	}

	bool decode(const YAML::Node& node) override {
		return true;
	}

	void inspector(SceneContext* context) override {

	}

};
