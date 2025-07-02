// Engine/Scene/SceneManager.h
#pragma once
#include <memory>
#include <Windows.h>
#include "Service/IService.h"

class Scene;

class MainRenderer;
class GraphicsContext;
class InputService;
class ResourceService;
class DebugLogSystem;
class ImGuiService;
class SceneManager;

struct SceneManagerContext {
	SceneManager* sceneManager = nullptr;

	GraphicsContext* graphics = nullptr;
	MainRenderer* renderer = nullptr;
	InputService* input = nullptr;
	ResourceService* resource = nullptr;
	DebugLogSystem* debug = nullptr;
	ImGuiService* imgui = nullptr;
	HWND hwnd = nullptr;
};

class SceneManager : public IService {
public:
	SceneManager() = default;
	~SceneManager() = default;

	void Initialize(SceneManagerContext sceneContext);
	void Update(float deltaTime);
	void FixedUpdate(float fixedDeltaTime);
	void Render();
	void Shutdown() override;

	void LoadScene(std::shared_ptr<Scene> scene);
	std::shared_ptr<Scene> GetActiveScene() const;
	void DeferredLoadScene(std::shared_ptr<Scene> scene) {
		m_NextScene.reset();
		m_NeedSceneChange = true;
		m_NextScene = scene;
	}
	void SaveScene();
	void OpenScene();

private:
	std::shared_ptr<Scene> m_activeScene;
	SceneManagerContext m_SceneContext;

	bool m_NeedSceneChange = false;
	std::shared_ptr<Scene> m_NextScene;

	bool OpenFlag = false;
};
