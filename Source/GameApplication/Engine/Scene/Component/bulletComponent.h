#pragma once
#include "Interface/IComponent.h"
#include "Backends/myVector3.h"

class BulletComponent: public IComponent {
public:
	float maxLifeTime = 2.5f;
	float currentLifeTime = 0.0f;
	float bulletSpeed = 20.0f;
};
