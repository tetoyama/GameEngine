// Engine/Scene/lightSystem.h
#pragma once
#include "Interface/ISystem.h"

struct SceneManagerContext;

class LightSystem: public ISystem{
public:
	LightSystem(SceneManagerContext* context): m_context(context){}
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
	SceneManagerContext* m_context;
};
