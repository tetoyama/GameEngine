// Engine/Scene/System/renderSystem.h
#pragma once

class EntityRegistry;

class RenderSystem {
public:
	RenderSystem(EntityRegistry* registry);
	~RenderSystem();

	// •`‰æˆ—
	void Render();

private:
	EntityRegistry* m_registry;
};
