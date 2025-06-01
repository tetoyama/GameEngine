#pragma once
#include "Component.h"
#include "Backends/myVector3.h"

class TransformComponent: public Component {
public:
	Vector3 position = {0, 0, 0};
	Vector3 rotation = {0, 0, 0};
	Vector3 scale = {1, 1, 1};
};
