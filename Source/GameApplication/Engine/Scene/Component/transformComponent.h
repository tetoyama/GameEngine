#pragma once
#include "IComponent.h"
#include "Backends/myVector3.h"

class TransformComponent : public IComponent{
public:
	Vector3 position = Vector3(0, 0, 0);
	Vector3 rotation = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);
};
