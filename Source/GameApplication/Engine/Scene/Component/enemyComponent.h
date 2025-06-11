#pragma once
#include "Interface/IComponent.h"
#include "Backends/myVector3.h"

class EnemyComponent: public IComponent {
public:
	bool setOriginPos = false;
	Vector3 OriginPos = Vector3(0.0f,0.0f,0.0f);
	int maxDistance = 10;
	Vector3 TargetPos = Vector3(0.0f,0.0f,0.0f);

	float MoveSpeed = 0.1f;
};