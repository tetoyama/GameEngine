#pragma once
#include "Interface/IComponent.h"

class ExplosionEffectComponent : public IComponent {
public:
	float LifeTime = 0.5f;
	float Timer = 0.0f;
};
