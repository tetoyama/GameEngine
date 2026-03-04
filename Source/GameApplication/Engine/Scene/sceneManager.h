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

class MainRenderer;
class GraphicsContext;
class AudioContext;
class InputService;
class ResourceService;
class DebugLogService;
class ImGuiService;
class ConfigService;
class EditorService;

class SystemRegistry;

class SceneManager;
class Scene;

// シーンマネージャと各サービスへのポインタをまとめたコンテキスト
struct SceneManagerContext {
	SceneManager* sceneManager = nullptr;

	SystemRegistry* systemRegistry = nullptr;

	// 画面情報
	Vector2 PlayerScreenSize = { 1280.0f, 720.0f };
	Vector2 EditorScreenSize = { 1280.0f, 720.0f };

	GraphicsContext* graphics = nullptr;
	AudioContext* audio = nullptr;
	MainRenderer* renderer = nullptr;
	InputService* input = nullptr;
	ResourceService* resource = nullptr;
	DebugLogService* debug = nullptr;
	ImGuiService* imgui = nullptr;
	ConfigService* config = nullptr;
	EditorService* editor = nullptr;
	HWND hwnd = nullptr;
};


enum SceneManagerState
{
	Playing,	// ゲームプレイ中
	Paused,		// 一時停止中
	Stopped,	// 停止中
	Step,		// 1フレーム進める
};

// 複数シーンの管理と再生状態の制御を行うサービス
class SceneManager : public IService {
public:
	SceneManager() = default;
	~SceneManager() = default;

	void Initialize(SceneManagerContext sceneContext);
	void Update(float deltaTime);
	void FixedUpdate(float fixedDeltaTime);
	void Draw();
	void Shutdown() override;

	SceneManagerContext* GetContext(){
		return &m_SceneContext;
	}

	void AddScene(std::shared_ptr<Scene> scene);

	void LoadScene(std::shared_ptr<Scene> scene);
	void DeferredLoadScene(std::shared_ptr<Scene> scene);

	void SaveScenes();

	const std::unordered_map<std::string, std::shared_ptr<Scene>>& GetActiveScenes() const { return m_activeScenes; }

	std::shared_ptr<Scene> OpenFromYAMLFile();
	std::shared_ptr<Scene> LoadFromFilePath(const std::string& filePath);

	SceneManagerState State = SceneManagerState::Stopped;
	SceneManagerState OldState = SceneManagerState::Stopped;

	std::shared_ptr<SystemRegistry> systemRegistry = nullptr;


private:

	void TempSave(); // 一時保存
	void TempLoad(); // 一時読み込み

	std::unordered_map<std::string, std::shared_ptr<Scene>> m_activeScenes;
	SceneManagerContext m_SceneContext;

	bool m_NeedSceneChange = false;
	std::shared_ptr<Scene> m_NextScene = nullptr;
};
