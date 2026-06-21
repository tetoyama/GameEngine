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

// シーンマネージャと各サービスへのポインタをまとめたコンテキスト
// SceneManager::Initialize に渡し、各シーンや ECS システムから参照される
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

// シーンマネージャの再生状態を表す列挙型
enum SceneManagerState
{
	Playing,
	Paused,
	Stopped,
	Step,
};

// 複数シーンの管理と再生状態の制御を行うサービス
class SceneManager: public IService {
public:
	SceneManager() = default;
	~SceneManager() override;

	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

	void Initialize(SceneManagerContext sceneContext);
	void Update(float deltaTime);
	void FixedUpdate(float fixedDeltaTime);
	void Draw();
	void Shutdown() override;

	SceneManagerContext* GetContext(){
		return &m_SceneContext;
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

	// SceneManagerが唯一の所有者。SceneManagerContextへは非所有ポインタを公開する。
	std::unique_ptr<SystemRegistry> systemRegistry;

private:
	void TempSave();
	void TempLoad();

	std::unordered_map<std::string, std::shared_ptr<Scene>> m_activeScenes;
	SceneManagerContext m_SceneContext;

	std::unordered_map<uint32_t, SceneContext*> m_contextRegistry;
	uint32_t m_nextContextID = 1;

	bool m_NeedSceneChange = false;
	std::shared_ptr<Scene> m_NextScene;
};
