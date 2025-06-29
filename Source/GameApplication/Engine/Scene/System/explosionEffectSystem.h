#pragma once
#include "Interface/ISystem.h"

struct SceneContext;

class ExplosionEffectSystem : public ISystem {
public:
	ExplosionEffectSystem(SceneContext* context) : m_context(context) {}
	~ExplosionEffectSystem() {}

	void Initialize() override {}
	void Finalize()override {}

	void Start() override {}
	void Update(float deltaTime) override;
	void FixedUpdate(float fidedDeltaTime) override {}
	void Draw() override {};
	void EditorUpdate(float deltaTime) override{}

private:
	SceneContext* m_context;
};
