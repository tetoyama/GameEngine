// Engine/Scene/System/renderSystem.h
#pragma once
#include "ISystem.h"
#include "d3d11.h"

class EntityRegistry;
class MainRenderer;

class RenderSystem : public ISystem{
public:
	RenderSystem(EntityRegistry* registry, MainRenderer* renderer)
		: m_registry(registry), m_renderer(renderer){
		Initialize();
	}
	~RenderSystem(){}

	// •`‰æˆ—
	void Initialize() override;
	void Draw() override;
	void Finalize()override;

private:
	EntityRegistry* m_registry = nullptr;
	MainRenderer* m_renderer = nullptr;
};
