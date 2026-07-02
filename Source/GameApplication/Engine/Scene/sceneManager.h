// =======================================================================
// 
// sceneManager.h
// 
// =======================================================================
#pragma once
#include "buildSetting.h"
#include <memory>
#include <Windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "Service/IService.h"
#include "Backends/myVector2.h"
#include "Graphics/mainRenderer.h"

class GraphicsContext;
class AudioContext;
class InputService;
class ResourceService;
class DebugLogService;
class ImGuiService;
class ConfigService;
class EditorService;
class SystemRegistry;

struct SceneContext;
class SceneManager;
class Scene;

struct SceneManagerContext {
	SceneManager* sceneManager = nullptr;
	SystemRegistry* systemRegistry = nullptr;

	Vector2 PlayerScreenSize = { 1280.0f, 720.0f };
	Vector2 EditorScreenSize = { 1280.0f, 720.0f };

	GraphicsContext* graphics  = nullptr;
	AudioContext*    audio     = nullptr;
	MainRenderer*    renderer  = nullptr;
	InputService*    input     = nullptr;
	ResourceService* resource  = nullptr;
	DebugLogService* debug     = nullptr;
	ImGuiService*    imgui     = nullptr;
	ConfigService*   config    = nullptr;
	EditorService*   editor    = nullptr;
	HWND             hwnd      = nullptr;
};

enum SceneManagerState
{
	Playing,
	Paused,
	Stopped,
	Step,
};

class SceneManager: public IService {
public:
	SceneManager();
	~SceneManager() override;

	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

	void Initialize(SceneManagerContext sceneContext);
	void Update(float deltaTime);
	void FixedUpdate(float fixedDeltaTime);
	void Draw();
	void Shutdown() override;

	SceneManagerContext* GetContext() noexcept {
		return &m_SceneContext;
	}

	const SceneManagerContext* GetContext() const noexcept {
		return &m_SceneContext;
	}

	SystemRegistry* GetSystemRegistry() noexcept {
		return m_systemRegistry.get();
	}

	const SystemRegistry* GetSystemRegistry() const noexcept {
		return m_systemRegistry.get();
	}

	uint32_t GetIDFromContext(SceneContext* ctx);
	SceneContext* GetContextFromID(uint32_t id);
	void UnregisterContext(SceneContext* ctx);

	void AddScene(std::shared_ptr<Scene> scene);
	void LoadScene(std::shared_ptr<Scene> scene);
	void DeferredLoadScene(std::shared_ptr<Scene> scene);
	void SaveScenes();

	const std::unordered_map<std::string, std::shared_ptr<Scene>>& GetActiveScenes() const{
		return m_activeScenes;
	}

	std::shared_ptr<Scene> OpenFromYAMLFile();
	std::shared_ptr<Scene> LoadFromFilePath(const std::string& filePath);

	SceneManagerState State = SceneManagerState::Stopped;
	SceneManagerState OldState = SceneManagerState::Stopped;

private:
	void TempSave();
	void TempLoad();

	std::unique_ptr<SystemRegistry> m_systemRegistry;

	std::unordered_map<std::string, std::shared_ptr<Scene>> m_activeScenes;
	SceneManagerContext m_SceneContext;

	std::unordered_map<uint32_t, SceneContext*> m_contextRegistry;
	uint32_t m_nextContextID = 1;

	bool m_NeedSceneChange = false;
	std::shared_ptr<Scene> m_NextScene;
};
