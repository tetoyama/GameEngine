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
#include "UI/BRAIN.h"

#include "Analysis/AnalyzerManager.h"

void EditorService::Initialize(EditorServiceContext context) {

	debugLogSystem = context.debugLogSystem;
	resourceService = context.resourceService;
	sceneManager = context.sceneManager;
	llamaService = context.llamaService;

	analyzer = new AnalyzerManager();
	if (analyzer) {

		AnalyzerManagerContext ctx;
		ctx.debug = debugLogSystem;

		analyzer->Initialize(ctx);
	}

	UIs.clear();
	UIs.push_back(new MenuBar());
	UIs.push_back(new PerformanceMonitor());
	UIs.push_back(new Hierarchy());
	UIs.push_back(new Inspector());
	UIs.push_back(new AssetsBrowser());
	UIs.push_back(new DebugLogWindow());
	UIs.push_back(new ViewWindow());
	UIs.push_back(new SystemSetting());
	UIs.push_back(new BRAIN());

	for (auto ui : UIs) {
		ui->Initialize(this);
	}
}

void EditorService::Draw(EditorDrawContext ctx) {
	for (auto ui : UIs) {
		ui->Draw(ctx);
	}
}

void EditorService::Shutdown() {

	if (analyzer) {
		analyzer->Finalize();
		delete analyzer;
		analyzer = nullptr;
	}

	for (auto ui : UIs) {
		ui->Finalize();
		delete ui;
		ui = nullptr;
	}
	UIs.clear();
}

