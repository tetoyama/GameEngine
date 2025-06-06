#pragma once
#include "Interface/ISystem.h"

class EntityRegistry;
class MainRenderer;

class CameraSystem : public ISystem {

public:

	CameraSystem(EntityRegistry* registry, MainRenderer* renderer)
		: m_registry(registry), m_renderer(renderer) {
		Initialize();
	}

	void Initialize() override {};
	void Finalize()override {};

	void Start() override {};
	void Update(float deltaTime) override {};
	void Draw() override;

private:
	EntityRegistry* m_registry = nullptr;
	MainRenderer* m_renderer = nullptr;
};
