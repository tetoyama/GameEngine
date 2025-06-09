// Engine/Scene/Scene.h
#pragma once
#include <memory>
#include <vector>
#include <d3d11.h>

struct SceneManagerContext;

class EntityRegistry;
class ComponentRegistry;
class SystemRegistry;

struct SceneContext{
	EntityRegistry* entity = nullptr;
	ComponentRegistry* component = nullptr;
	SystemRegistry* system = nullptr;
	SceneManagerContext* manager = nullptr;
};

class Scene {
public:
	Scene();
	~Scene();

	void Initialize(SceneManagerContext* set);
	void Update(float deltaTime);
	void FixedUpdate(float fixedDeltaTime);
	void Render();
	void Shutdown();

	SceneContext* GetSceneContext(){return &m_SceneContext;}

private:

	SceneManagerContext* m_SceneManagerContext = nullptr;

	std::shared_ptr<EntityRegistry> m_entityRegistry;
	std::shared_ptr<ComponentRegistry> m_componentRegistry;
	std::shared_ptr<SystemRegistry> m_systemRegistry;

	SceneContext m_SceneContext{};
};
