// Engine/Scene/transformSystem.h
#pragma once

class EntityRegistry;

class TransformSystem {
public:
	TransformSystem(EntityRegistry* registry);
	~TransformSystem();

	// 毎フレームの更新
	void Update(float deltaTime);

private:
	EntityRegistry* m_registry;
};
