#pragma once
#include "IComponent.h"
#include "Backends/myVector3.h"

class PlayerComponent : public IComponent {
public:
	float moveSpeed = 50.0f;
};
