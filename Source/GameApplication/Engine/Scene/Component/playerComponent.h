#pragma once
#include "Interface/IComponent.h"
#include "Backends/myVector3.h"

class PlayerComponent : public IComponent {
public:
	float moveSpeed = 5.0f;
	float cameraRotate = 2.0f;
	float cameraDistance = 20.0f;
	float cameraLerp = 2.0f;
};
