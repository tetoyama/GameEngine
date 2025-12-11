#pragma once
#include "Interface/IComponent.h"
#include "Backends/myVector3.h"
#include "BackEnds/YAMLConverters.h"
#include "Registry/componentRegistry.h"
#include "DebugTools/ImGuiSystem.h"


class OutlineComponent: public IComponent{

public:
	YAML::Node encode() override{
		YAML::Node node;

		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{

		return true;
	}
	void inspector(SceneContext* context) override{

	}
};