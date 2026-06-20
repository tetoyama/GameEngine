// =======================================================================
// 
// editorService.cpp
// 
// =======================================================================
#include "editorService.h"
#include "Editor/InterFace/IEditorUI.h"
#include "UI/MenuBar.h"
#include "UI/PerformanceMonitor.h"
#include "UI/Hierarchy.h"
#include "UI/Inspector.h"
#include "UI/AssetsBrowser.h"
#include "UI/DebugLogWindow.h"
#include "UI/ViewWindow.h"
#include "UI/SystemSetting.h"
#include "UI/BRAIN/BRAIN.h"
#include "UI/CB41.h"

#include "Analysis/AnalyzerManager.h"

// -----------------------------------------------------------------------
// EditorService::Initialize
// エディターの各 UI パネルと AnalyzerManager を生成・初期化する
// 全パネルは IEditorUI::Initialize に this を渡してサービス参照を取得する
// -----------------------------------------------------------------------
void EditorService::Initialize(EditorServiceContext context) {

	// 各依存サービスへの参照を保持（UI パネルから参照できるようにする）
	debugLogSystem = context.debugLogSystem;
	resourceService = context.resourceService;
	sceneManager = context.sceneManager;
	llamaService = context.llamaService;

	// AnalyzerManager の生成・初期化（ソースコード解析機能の起動）
	analyzer = new AnalyzerManager();
	if (analyzer) {

		AnalyzerManagerContext ctx;
		ctx.debug = debugLogSystem;

		analyzer->Initialize(ctx);
	}

	// 全 UI パネルを生成してリストに追加する
	// 注意: BRAIN は現在無効化中（コメントアウト）
	UIs.clear();
	UIs.push_back(new MenuBar());           // ファイル・編集メニューバー
	UIs.push_back(new PerformanceMonitor()); // FPS・更新時間モニター
	UIs.push_back(new Hierarchy());         // シーン階層ウィンドウ
	UIs.push_back(new Inspector());         // コンポーネントインスペクター
	UIs.push_back(new AssetsBrowser());     // アセットブラウザ
	UIs.push_back(new DebugLogWindow());    // デバッグログウィンドウ
	UIs.push_back(new ViewWindow());        // シーンビューウィンドウ
	UIs.push_back(new SystemSetting());     // システム設定ウィンドウ
	UIs.push_back(new BRAIN());           // LLM エージェントウィンドウ（開発中）
	// UIs.push_back(new CB41());				// CB41用

	// 全パネルを初期化（editorService への参照を渡す）
	for (auto ui : UIs) {
		ui->Initialize(this);
	}
}

// -----------------------------------------------------------------------
// EditorService::Draw
// 全 UI パネルを描画する。毎フレーム ImGui フレーム内で呼ばれる。
// -----------------------------------------------------------------------
void EditorService::Draw(EditorDrawContext ctx) {
	for (auto ui : UIs) {
		ui->Draw(ctx);
	}
}

// -----------------------------------------------------------------------
// EditorService::Shutdown
// AnalyzerManager と全 UI パネルを終了・解放する
// -----------------------------------------------------------------------
void EditorService::Shutdown() {

	// AnalyzerManager の終了と解放
	if (analyzer) {
		analyzer->Finalize();
		delete analyzer;
		analyzer = nullptr;
	}

	// 全 UI パネルを終了・解放（逆順でも構わないが登録順で解放）
	for (auto ui : UIs) {
		ui->Finalize();
		delete ui;
		ui = nullptr;
	}
	UIs.clear();
}

