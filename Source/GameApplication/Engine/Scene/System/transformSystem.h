// Engine/Scene/transformSystem.h
#pragma once
#include "ISystem.h"

class EntityRegistry;

class TransformSystem : ISystem{
public:
	TransformSystem(EntityRegistry* registry): m_registry(registry){
		Initialize();
	}
	~TransformSystem(){}

	void Initialize() override{};
	void Update(float deltaTime) override{};
	void Draw() override{};
	void Finalize()override{};
private:
	EntityRegistry* m_registry;
};
