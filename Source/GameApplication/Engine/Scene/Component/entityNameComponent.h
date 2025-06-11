#pragma once
#include "Interface/IComponent.h"
#include <string>

class NameComponent: public IComponent {
public:
	std::string name;
};
