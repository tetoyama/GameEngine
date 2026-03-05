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
enum SceneManagerState
{
	Playing,	// ゲームプレイ中（全システムが更新される）
	Paused,		// 一時停止中（FixedUpdate のみ停止）
	Stopped,	// 停止中（エディターモード、ECS システムは動作しない）
	Step,		// 1フレームだけ進める（デバッグ用ステップ実行）
};

// 複数シーンの管理と再生状態の制御を行うサービス
// シーンの追加・ロード・保存・遷移を担当し、各フレームに Update/FixedUpdate/Draw を通知する
class SceneManager : public IService {
public:
	SceneManager() = default;
	~SceneManager() = default;

	// シーンマネージャを初期化する（ECS システムの登録とシステムレジストリの構築）
	void Initialize(SceneManagerContext sceneContext);

	// 毎フレームのゲームロジック更新（再生中のシーンのみ）
	void Update(float deltaTime);

	// 固定タイムステップでの物理シミュレーション更新
	void FixedUpdate(float fixedDeltaTime);

	// 全シーンのレンダリング処理
	void Draw();

	// シーンマネージャを終了する（全アクティブシーンを解放）
	void Shutdown() override;

	// シーンコンテキストへのポインタを返す（他のサービスからシーン情報にアクセスする際に使用）
	SceneManagerContext* GetContext(){
		return &m_SceneContext;
	}

	// 新しいシーンをアクティブシーンリストに追加する（シーン加算ロード）
	void AddScene(std::shared_ptr<Scene> scene);

	// シーンをただちにロードして起動する
	void LoadScene(std::shared_ptr<Scene> scene);

	// シーンをフレーム終了後に遅延ロードする（現在の更新処理が完了してから切り替え）
	void DeferredLoadScene(std::shared_ptr<Scene> scene);

	// 全アクティブシーンを YAML ファイルに保存する
	void SaveScenes();

	// アクティブシーンのマップを返す（キーはシーン名）
	const std::unordered_map<std::string, std::shared_ptr<Scene>>& GetActiveScenes() const { return m_activeScenes; }

	// ファイル選択ダイアログを開き、選択した YAML シーンファイルを読み込む
	std::shared_ptr<Scene> OpenFromYAMLFile();

	// 指定されたファイルパスから YAML シーンを読み込む（起動時の初期シーンロードに使用）
	std::shared_ptr<Scene> LoadFromFilePath(const std::string& filePath);

	SceneManagerState State    = SceneManagerState::Stopped;   // 現在の再生状態
	SceneManagerState OldState = SceneManagerState::Stopped;   // 直前の再生状態（状態遷移検出用）

	std::shared_ptr<SystemRegistry> systemRegistry = nullptr;  // ECS システムレジストリ


private:

	void TempSave(); // エディター再生開始前にシーン状態を一時保存する
	void TempLoad(); // エディター再生停止後に一時保存したシーン状態を復元する

	std::unordered_map<std::string, std::shared_ptr<Scene>> m_activeScenes; // アクティブシーンのマップ
	SceneManagerContext m_SceneContext;                                      // 初期化時に受け取ったコンテキスト

	bool m_NeedSceneChange = false;            // 遅延シーン遷移のフラグ
	std::shared_ptr<Scene> m_NextScene = nullptr; // 遅延遷移先のシーン
};
