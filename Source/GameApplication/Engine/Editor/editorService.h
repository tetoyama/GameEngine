// =======================================================================
// 
// editorService.h
// 
// =======================================================================
#pragma once
#include "Service/IService.h"
#include <vector>
#include "InterFace/IEditorUI.h"
#include "Command/CommandManager.h"
#include "Runtime/TimeService/timeService.h"

struct EditorDrawContext;
class EditorService;

class ResourceService;
class DebugLogService;
class SceneManager;
class LLAMAService;

class AnalyzerManager;

struct SceneManagerContext;

// 前回完了したEditor Panel描画のCPU時間。
struct EditorPanelTiming {
	const char* name = nullptr;
	double seconds = 0.0;
};

// エディターサービスの初期化に必要な依存サービスをまとめた構造体
struct EditorServiceContext {
	DebugLogService* debugLogSystem = nullptr;   // デバッグログ出力サービス
	ResourceService* resourceService = nullptr;  // リソース読み込みサービス
	SceneManager*    sceneManager    = nullptr;  // シーン管理サービス
	LLAMAService*    llamaService    = nullptr;  // LLM エージェントサービス
};

// エディターのUIパネル・コマンド履歴・コード解析機能をまとめて管理するサービス
// IEditorUI を継承した各パネル（Hierarchy, Inspector 等）のライフサイクルを制御する
class EditorService : public IService {

public:

	// エディターサービスを初期化し、全 UI パネルと AnalyzerManager を生成・起動する
	void Initialize(EditorServiceContext context);

	// 毎フレーム全 UI パネルを描画する
	// ctx にはパフォーマンス情報（FPS・更新時間等）が含まれる
	void Draw(EditorDrawContext ctx);

	// 全 UI パネルと AnalyzerManager を解放する
	void Shutdown() override;

	// 型 T の UI パネルを取得する（T は IEditorUI の派生クラスである必要がある）
	// 見つからない場合は nullptr を返す
	template<typename T>
	T* GetUI() {
		static_assert(std::is_base_of<IEditorUI, T>::value, "T must inherit from IEditorUI");
		for (auto& panel : UIs) {
			if (auto p = dynamic_cast<T*>(panel.ui)) {
				return p;
			}
		}
		return nullptr;
	}

	DebugLogService* debugLogSystem = nullptr;  // デバッグログサービスへの参照
	ResourceService* resourceService = nullptr; // リソースサービスへの参照
	SceneManager*    sceneManager    = nullptr; // シーンマネージャへの参照
	LLAMAService*    llamaService    = nullptr; // LLAMA サービスへの参照

	AnalyzerManager* analyzer = nullptr;        // ソースコード解析マネージャ

	CommandManager commandManager;              // アンドゥ・リドゥ対応コマンド履歴

private:
	struct EditorUIPanelEntry {
		const char* name = nullptr;
		IEditorUI* ui = nullptr;
	};

	std::vector<EditorUIPanelEntry> UIs;
	std::vector<EditorPanelTiming> m_CurrentPanelTimings;
	std::vector<EditorPanelTiming> m_CompletedPanelTimings;
};

// エディター描画フェーズに渡すパフォーマンス統計情報
struct EditorDrawContext {
	double UpdateTime = 0.0;              // 更新フェーズの所要時間（秒）
	double DrawTime = 0.0;                // 前回完了した描画フェーズ全体（秒）
	double FPS = 0.0;                     // 更新フレームレート（fps）
	double FixedUpdateFPS = 0.0;          // 固定更新フレームレート（fps）
	DrawTimingBreakdown DrawTiming{};     // 前回完了したDrawフレームのCPU内訳
	const std::vector<EditorPanelTiming>* EditorPanelTimings = nullptr;
	double GPUFrameTime = 0.0;            // 非同期Timestamp Queryで取得したGPU時間（秒）
	bool GPUFrameTimeValid = false;
	bool VSyncEnabled = false;            // Present待機の判断に使用するVSync設定
	bool TearingSupported = false;
};
