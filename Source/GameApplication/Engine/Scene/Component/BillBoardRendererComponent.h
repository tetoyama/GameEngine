#pragma once
#include "Interface/IComponent.h"

class BillBoardRendererComponent : public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;
		return node;
	}

	bool decode(const YAML::Node& node) override{
		return true;
	}

	bool RotateX = false;
};
