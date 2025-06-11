// Engine/Scene/transformSystem.h
#pragma once
#include "Interface/ISystem.h"

struct SceneContext;

class TransformSystem : public ISystem{
public:
	TransformSystem(SceneContext* context): m_context(context){}
	~TransformSystem(){}

	void Initialize() override;
	void Finalize()override {};

	void Start() override {};
	void Update(float deltaTime) override{};
	void FixedUpdate(float fidedDeltaTime) override {}
	void Draw() override{}
private:
	SceneContext* m_context;
};
