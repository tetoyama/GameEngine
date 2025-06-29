#pragma once
#include "Interface/ISystem.h"

struct SceneContext;

class BulletSystem: public ISystem {
public:
	BulletSystem(SceneContext* context): m_context(context){}
	~BulletSystem(){}

	void Initialize() override{}
	void Finalize()override{}

	void Start() override{}
	void Update(float deltaTime) override;
	void FixedUpdate(float fidedDeltaTime) override{}
	void Draw() override{};
	void EditorUpdate(float deltaTime) override{}

private:
	SceneContext* m_context;
};
