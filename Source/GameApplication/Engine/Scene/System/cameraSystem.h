#pragma once
#include "Interface/ISystem.h"

struct SceneManagerContext;

class CameraSystem : public ISystem {

public:

	CameraSystem(SceneManagerContext* context): m_context(context){}

	void Initialize() override;
	void Finalize()override {}

	void Start() override {};
	void Update(float deltaTime) override;
	void FixedUpdate(float fidedDeltaTime) override {}
	void Draw() override {}
	void EditorUpdate(float deltaTime) override{}

private:
	SceneManagerContext* m_context;
};
