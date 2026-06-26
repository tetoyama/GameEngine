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
#include "UI/SceneStorageSettingsPanel.h"
#include "UI/BRAIN/BRAIN.h"
#include "UI/CB41.h"

#include "Analysis/AnalyzerManager.h"

#include <chrono>

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

	// 表示名はPerformance MonitorとProfilerで使用する固定名。
	UIs.clear();
	UIs.push_back({"MenuBar", new MenuBar()});
	UIs.push_back({"PerformanceMonitor", new PerformanceMonitor()});
	UIs.push_back({"Hierarchy", new Hierarchy()});
	UIs.push_back({"Inspector", new Inspector()});
	UIs.push_back({"AssetsBrowser", new AssetsBrowser()});
	UIs.push_back({"DebugLogWindow", new DebugLogWindow()});
	UIs.push_back({"ViewWindow", new ViewWindow()});
	UIs.push_back({"SystemSetting", new SystemSetting()});
	UIs.push_back({"SceneStorageSettings", new SceneStorageSettingsPanel()});
	//UIs.push_back({"BRAIN", new BRAIN()});
	//UIs.push_back({"CB41", new CB41()});

	m_CurrentPanelTimings.clear();
	m_CompletedPanelTimings.clear();
	m_CurrentPanelTimings.reserve(UIs.size());
	m_CompletedPanelTimings.reserve(UIs.size());

	// 全パネルを初期化（editorService への参照を渡す）
	for (auto& panel : UIs) {
		if(panel.ui){
			panel.ui->Initialize(this);
		}
	}
}

// -----------------------------------------------------------------------
// EditorService::Draw
// 全 UI パネルを描画する。毎フレーム ImGui フレーム内で呼ばれる。
// -----------------------------------------------------------------------
void EditorService::Draw(EditorDrawContext ctx) {
	// PerformanceMonitorには現在描画中ではなく、前回完了したPanel計測を渡す。
	ctx.EditorPanelTimings = &m_CompletedPanelTimings;
	m_CurrentPanelTimings.clear();

	using Clock = std::chrono::steady_clock;
	for (auto& panel : UIs) {
		if(!panel.ui) continue;

		const auto begin = Clock::now();
		panel.ui->Draw(ctx);
		const auto end = Clock::now();

		const double seconds =
			std::chrono::duration<double>(end - begin).count();
		m_CurrentPanelTimings.push_back({panel.name, seconds});
	}

	m_CompletedPanelTimings = m_CurrentPanelTimings;
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

	for (auto& panel : UIs) {
		if(panel.ui){
			panel.ui->Finalize();
			delete panel.ui;
			panel.ui = nullptr;
		}
	}
	UIs.clear();
	m_CurrentPanelTimings.clear();
	m_CompletedPanelTimings.clear();
}
