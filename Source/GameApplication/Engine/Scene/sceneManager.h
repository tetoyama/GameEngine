// Engine/Scene/SceneManager.h
#pragma once
#include <memory>
#include "Service/IService.h"

class Scene;

class MainRenderer;
class GraphicsContext;
class InputSystem;
class ResourceSystem;

struct SceneContext {
	GraphicsContext* graphics = nullptr;
	MainRenderer* renderer = nullptr;
	InputSystem* input = nullptr;
	ResourceSystem* resource = nullptr;
};

class SceneManager : public IService {
public:
	SceneManager() = default;
	~SceneManager() = default;

	void Initialize(SceneContext sceneContext);
	void Update(float deltaTime);
	void FixedUpdate(float fixedDeltaTime);
	void Render();
	void Shutdown() override;

	void LoadScene(std::shared_ptr<Scene> scene);
	std::shared_ptr<Scene> GetActiveScene() const;

private:
	std::shared_ptr<Scene> m_activeScene;
	SceneContext m_SceneContext;
};
