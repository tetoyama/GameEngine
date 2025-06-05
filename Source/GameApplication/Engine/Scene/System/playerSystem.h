#pragma once
#include "ISystem.h"

class EntityRegistry;
struct SceneContext;

class PlayerSystem : ISystem {
public:
	PlayerSystem(EntityRegistry* registry, SceneContext* context) : m_registry(registry),m_context(context) {
		Initialize();
	}
	~PlayerSystem() {}

	void Initialize() override {};
	void Update(float deltaTime) override;
	void Draw() override {};
	void Finalize()override {};
private:
	EntityRegistry* m_registry;
	SceneContext* m_context;
};
