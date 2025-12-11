// Scene/transformSystem.h
#pragma once
#include "Interface/ISystem.h"

struct SceneManagerContext;

class TransformSystem : public ISystem{
public:
	TransformSystem(SceneManagerContext* context): m_context(context){}
	~TransformSystem(){}

	void Initialize() override;
	void Finalize()override {};

	void Start() override {};
	void Update(float deltaTime) override{}
	void FixedUpdate(float fidedDeltaTime) override {}
	void Draw() override;
	void EditorUpdate(float deltaTime) override {}
private:
	SceneManagerContext* m_context;
};
