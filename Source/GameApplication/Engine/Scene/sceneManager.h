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

struct SceneContext;
class SceneManager;
class Scene;

// シーンマネージャと各サービスへのポインタをまとめたコンテキスト
// SceneManager::Initialize に渡し、各シーンや ECS システムから参照される
struct SceneManagerContext {
	SceneManager* sceneManager = nullptr;   // シーンマネージャ本体（シーン遷移操作等に使用）

	SystemRegistry* systemRegistry = nullptr; // ECS システムのレジストリ（自動設定）

	// ビューポートサイズ（ゲームプレイ画面とエディター画面それぞれ）
	Vector2 PlayerScreenSize = { 1280.0f, 720.0f };
	Vector2 EditorScreenSize = { 1280.0f, 720.0f };

	GraphicsContext* graphics  = nullptr;  // DirectX 11 グラフィクスコンテキスト
	AudioContext*    audio     = nullptr;  // オーディオコンテキスト
	MainRenderer*    renderer  = nullptr;  // メインレンダラー
	InputService*    input     = nullptr;  // 入力サービス
	ResourceService* resource  = nullptr;  // リソースサービス
	DebugLogService* debug     = nullptr;  // デバッグログサービス
	ImGuiService*    imgui     = nullptr;  // ImGui サービス
	ConfigService*   config    = nullptr;  // 設定サービス
	EditorService*   editor    = nullptr;  // エディターサービス（エディタービルド時のみ有効）
	HWND             hwnd      = nullptr;  // メインウィンドウハンドル
};


// シーンマネージャの再生状態を表す列挙型
enum class SceneManagerState
{
	PLAYING,	// ゲームプレイ中（全システムが更新される）
	PAUSED,		// 一時停止中（FixedUpdate のみ停止）
	STOPPED,	// 停止中（エディターモード、ECS システムは動作しない）
	STEP,		// 1フレームだけ進める（デバッグ用ステップ実行）
};

// 複数シーンの管理と再生状態の制御を行うサービス
// シーンの追加・ロード・保存・遷移を担当し、各フレームに Update/FixedUpdate/Draw を通知する
class SceneManager: public IService {
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

	// --- コンテキストとIDの相互変換 ---
	// ポインタからIDを取得（未登録なら登録する）
	uint32_t GetIDFromContext(SceneContext* ctx);
	// IDからポインタを安全に取得
	SceneContext* GetContextFromID(uint32_t id);

	void AddScene(std::shared_ptr<Scene> scene);
	void LoadScene(std::shared_ptr<Scene> scene);
	void DeferredLoadScene(std::shared_ptr<Scene> scene);
	void SaveScenes();

	const std::unordered_map<std::string, std::shared_ptr<Scene>>& GetActiveScenes() const{
		return m_activeScenes;
	}

	std::shared_ptr<Scene> OpenFromYAMLFile();
	std::shared_ptr<Scene> LoadFromFilePath(const std::string& filePath);

	SceneManagerState State = SceneManagerState::STOPPED;
	SceneManagerState OldState = SceneManagerState::STOPPED;

	std::shared_ptr<SystemRegistry> systemRegistry = nullptr;

private:
	void TempSave();
	void TempLoad();

	std::unordered_map<std::string, std::shared_ptr<Scene>> m_activeScenes;
	SceneManagerContext m_SceneContext;

	std::unordered_map<uint32_t, SceneContext*> m_contextRegistry;
	uint32_t m_nextContextID = 1;

	bool m_NeedSceneChange = false;
	std::shared_ptr<Scene> m_NextScene = nullptr;
};
