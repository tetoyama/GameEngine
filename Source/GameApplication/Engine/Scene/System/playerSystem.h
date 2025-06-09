#pragma once
#include "Interface/ISystem.h"

struct SceneContext;

class InputSystem;
class CameraComponent;
class TransformComponent;

class PlayerSystem : public ISystem {
public:
	PlayerSystem(SceneContext* context): m_context(context){
		Initialize();
	}
	~PlayerSystem() {}

	void Initialize() override {};
	void Finalize()override {};

	void Start() override;
	void Update(float deltaTime) override;
	void FixedUpdate(float fidedDeltaTime) override {}
	void Draw() override {};

private:
	SceneContext* m_context;

	InputSystem* m_inputSystem = nullptr;

	CameraComponent* m_cameraComponent = nullptr;
	TransformComponent* m_cameraTransform = nullptr;
};
