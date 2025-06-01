// Engine/Scene/Scene.h
#pragma once
#include <memory>
#include <vector>

class MainRenderer;
class GraphicsContext;

class EntityRegistry;
class TransformSystem;
class RenderSystem;

class Scene {
public:
	Scene();
	~Scene();

	void Initialize(GraphicsContext* graphiccontext, MainRenderer* mainRenderer);
	void Update(float deltaTime);
	void FixedUpdate(float fixedDeltaTime);
	void Render();
	void Shutdown();

	std::shared_ptr<EntityRegistry> GetEntityRegistry();

private:
	std::shared_ptr<EntityRegistry> m_entityRegistry;
	std::unique_ptr<TransformSystem> m_transformSystem;
	std::unique_ptr<RenderSystem> m_renderSystem;
};
