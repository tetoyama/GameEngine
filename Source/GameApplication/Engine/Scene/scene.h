// Engine/Scene/Scene.h
#pragma once
#include <memory>
#include <vector>
#include <d3d11.h>

struct SceneContext;

class EntityRegistry;
class TransformSystem;
class RenderSystem;

class Scene {
public:
	Scene();
	~Scene();

	void Initialize(SceneContext* set);
	void Update(float deltaTime);
	void FixedUpdate(float fixedDeltaTime);
	void Render();
	void Shutdown();

	std::shared_ptr<EntityRegistry> GetEntityRegistry();

private:

	SceneContext* m_SceneContext = nullptr;
	std::shared_ptr<EntityRegistry> m_entityRegistry;
	std::unique_ptr<TransformSystem> m_transformSystem;
	std::unique_ptr<RenderSystem> m_renderSystem;
};
