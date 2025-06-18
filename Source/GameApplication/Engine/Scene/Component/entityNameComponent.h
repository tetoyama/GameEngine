#pragma once
#include "Interface/IComponent.h"
#include <string>

class NameComponent: public IComponent {
public:

	YAML::Node encode() override{
		YAML::Node node;
		return node;
	}

	bool decode(const YAML::Node& node) override{
		return true;
	}
	std::string name;
};
