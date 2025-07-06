// Engine/Scene/lightSystem.h
#pragma once
#include "Interface/ISystem.h"

struct SceneContext;

class LightSystem: public ISystem{
public:
	LightSystem(SceneContext* context): m_context(context){}
	~LightSystem(){}

	void Initialize() override;
	void Finalize()override{};

	void Start() override{};
	void Update(float deltaTime) override;
	void FixedUpdate(float fidedDeltaTime) override{}
	void Draw() override;
	void EditorUpdate(float deltaTime) override{
		Update(deltaTime);
	}
private:
	SceneContext* m_context;
};
