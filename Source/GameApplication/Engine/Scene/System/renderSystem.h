// Engine/Scene/System/renderSystem.h
#pragma once

class EntityRegistry;
class MainRenderer;

class RenderSystem {
public:
	RenderSystem(EntityRegistry* registry, MainRenderer* renderer)
		: m_registry(registry), m_renderer(renderer){}
	~RenderSystem(){}

	// •`‰æˆ—
	void Render();

private:
	EntityRegistry* m_registry;
	MainRenderer* m_renderer;
};
