// Engine/Scene/transformSystem.h
#pragma once
#include "ISystem.h"

class EntityRegistry;

class TransformSystem : ISystem{
public:
	TransformSystem(EntityRegistry* registry);
	~TransformSystem();

	// 毎フレームの更新
	void Update(float deltaTime);

private:
	EntityRegistry* m_registry;
};
