#pragma once
#include "Component.h"

class TransformComponent: public Component {
public:
	float position[3] = {0, 0, 0};
	float rotation[3] = {0, 0, 0};
	float scale[3] = {1, 1, 1};
};
