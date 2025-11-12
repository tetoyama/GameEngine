// Engine/Scene/SceneManager.h
#pragma once
#include <memory>
#include <Windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "Service/IService.h"

class MainRenderer;
class GraphicsContext;
class AudioContext;
class InputService;
class ResourceService;
class DebugLogSystem;
class ImGuiService;
class SceneManager;

struct ManagerContext {
	SceneManager* sceneManager = nullptr;

	GraphicsContext* graphics = nullptr;
	AudioContext* audio = nullptr;
	MainRenderer* renderer = nullptr;
	InputService* input = nullptr;
	ResourceService* resource = nullptr;
	DebugLogSystem* debug = nullptr;
	ImGuiService* imgui = nullptr;
	HWND hwnd = nullptr;
};

class Scene;

enum SceneManagerState
{
	Playing,	// ゲームプレイ中
	Paused,		// 一時停止中
	Stopped,	// 停止中
	Step,		// 1フレーム進める
};

class SceneManager : public IService {
public:
	SceneManager() = default;
	~SceneManager() = default;

	void Initialize(ManagerContext sceneContext);
	void Update(float deltaTime);
	void FixedUpdate(float fixedDeltaTime);
	void Draw();
	void Shutdown() override;

	void AddScene(std::shared_ptr<Scene> scene);

	void LoadScene(std::shared_ptr<Scene> scene);
	void DeferredLoadScene(std::shared_ptr<Scene> scene);

	void SaveScenes();

	std::shared_ptr<Scene> OpenFromYAMLFile();
	std::shared_ptr<Scene> LoadFromFilePath(const std::string& filePath);

private:

	std::unordered_map<std::string, std::shared_ptr<Scene>> m_activeScenes;
	ManagerContext m_SceneContext;

	bool m_NeedSceneChange = false;
	std::shared_ptr<Scene> m_NextScene;

	bool OpenFlag = false;
};
