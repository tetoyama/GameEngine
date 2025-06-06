// Engine/Scene/transformSystem.h
#pragma once
#include "Interface/ISystem.h"

class EntityRegistry;

class TransformSystem : public ISystem{
public:
	TransformSystem(EntityRegistry* registry): m_registry(registry){
		Initialize();
	}
	~TransformSystem(){}

	void Initialize() override{};
	void Finalize()override {};

	void Start() override {};
	void Update(float deltaTime) override{};
	void FixedUpdate(float fidedDeltaTime) override {}
	void Draw() override {};
private:
	EntityRegistry* m_registry;
};
