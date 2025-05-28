// Engine/Scene/System/renderSystem.h
#pragma once
#include "ISystem.h"

class EntityRegistry;
class MainRenderer;

class RenderSystem : public ISystem{
public:
	RenderSystem(EntityRegistry* registry, MainRenderer* renderer)
		: m_registry(registry), m_renderer(renderer){}
	~RenderSystem(){}

	// •`‰æˆ—
	void Initialize() override;
	void Draw() override;
	void Finalize()override;

private:
	EntityRegistry* m_registry;
	MainRenderer* m_renderer;
};
