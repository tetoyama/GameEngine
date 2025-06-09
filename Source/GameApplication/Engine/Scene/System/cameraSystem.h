#pragma once
#include "Interface/ISystem.h"

struct SceneContext;

class CameraSystem : public ISystem {

public:

	CameraSystem(SceneContext* context): m_context(context){
		Initialize();
	}

	void Initialize() override {};
	void Finalize()override {};

	void Start() override {};
	void Update(float deltaTime) override;
	void FixedUpdate(float fidedDeltaTime) override {}
	void Draw() override {}

private:
	SceneContext* m_context;
};
