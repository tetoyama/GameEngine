#pragma once
#include "Interface/IComponent.h"

class ExplosionEffectComponent : public IComponent {
public:
	// Á‚¦‚é‚Ü‚Å‚ÌŠÔ
	float LifeTime = 0.5f;

	// ¶¬‚©‚ç‚ÌŒo‰ßŠÔ
	float Timer = 0.0f;
};
