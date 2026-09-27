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
#include "UI/ModernImGui/EditorIconWidgets.h"
#include "UI/ModernImGui/EditorEmptyState.h"
#include "AgentOS/UI/AgentOSPanel.h"

#include "Analysis/AnalyzerManager.h"

#include <chrono>

void EditorService::Initialize(EditorServiceContext context) {
	debugLogSystem = context.debugLogSystem;
	resourceService = context.resourceService;
	sceneManager = context.sceneManager;
	llamaService = context.llamaService;
	icons.Initialize(resourceService);

	analyzer = new AnalyzerManager();
	if (analyzer) {
		AnalyzerManagerContext ctx;
		ctx.debug = debugLogSystem;
		analyzer->Initialize(ctx);
	}

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
	// AgentOSPanel: BRAIN後継のLLMエージェント基盤UI。
	// 表示トグルは MenuBar::showBRAIN を共有する。
	UIs.push_back({"BRAIN", new agentos::AgentOSPanel()});

	m_CurrentPanelTimings.clear();
	m_CompletedPanelTimings.clear();
	m_CurrentPanelTimings.reserve(UIs.size());
	m_CompletedPanelTimings.reserve(UIs.size());

	for (auto& panel : UIs) {
		if(panel.ui){
			panel.ui->Initialize(this);
		}
	}
}

void EditorService::Draw(EditorDrawContext ctx) {
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

	Hierarchy* hierarchy = GetUI<Hierarchy>();
	if(!hierarchy || !hierarchy->selectedEntity || !hierarchy->sceneContext){
		MImGui::DrawInspectorEmptyState(icons);
	}

	m_CompletedPanelTimings = m_CurrentPanelTimings;
}

void EditorService::Shutdown() {
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
	icons.Shutdown();
	m_CurrentPanelTimings.clear();
	m_CompletedPanelTimings.clear();
}
