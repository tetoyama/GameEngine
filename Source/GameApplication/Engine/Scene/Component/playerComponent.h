#pragma once
#include "Interface/IComponent.h"
#include "Backends/myVector3.h"

class PlayerComponent : public IComponent {
public:
	float moveSpeed = 50.0f;
	float cameraRotate = 2.0f;
	float cameraLerp = 4.5f;
};
