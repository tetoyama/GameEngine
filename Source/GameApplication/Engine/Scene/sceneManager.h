// Engine/Scene/SceneManager.h
#pragma once
#include <memory>
#include "Service/IService.h"

class Scene;
class MainRenderer;
class GraphicsContext;
class InputSystem;

class SceneManager : public IService {
public:
	SceneManager();
	~SceneManager();

	void Initialize(GraphicsContext* graphiccontext, MainRenderer* mainRenderer, InputSystem* inputsystem);
	void Update(float deltaTime);
	void FixedUpdate(float fixedDeltaTime);
	void Render();
	void Shutdown();

	void LoadScene(std::shared_ptr<Scene> scene);
	std::shared_ptr<Scene> GetActiveScene() const;

private:
	std::shared_ptr<Scene> m_activeScene;
	MainRenderer* m_mainRenderer = nullptr;
	GraphicsContext* m_graphicsContext = nullptr;
	InputSystem* m_InputSystem = nullptr;
};
