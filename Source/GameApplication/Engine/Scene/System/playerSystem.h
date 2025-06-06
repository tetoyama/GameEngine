#pragma once
#include "Interface/ISystem.h"

class EntityRegistry;
struct SceneContext;

class InputSystem;
class CameraComponent;
class TransformComponent;

class PlayerSystem : public ISystem {
public:
	PlayerSystem(EntityRegistry* registry, SceneContext* context) : m_registry(registry),m_context(context) {
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
	EntityRegistry* m_registry;
	SceneContext* m_context;

	InputSystem* m_inputSystem = nullptr;

	CameraComponent* m_cameraComponent = nullptr;
	TransformComponent* m_cameraTransform = nullptr;
};
