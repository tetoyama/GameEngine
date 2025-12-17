#include "editorService.h"
#include "Editor/InterFace/IEditorUI.h"
#include "UI/MenuBar.h"
#include "UI/PerformanceMonitor.h"
#include "UI/Hierarchy.h"
#include "UI/Inspector.h"
#include "UI/AssetsBrowser.h"
#include "UI/DebugLogWindow.h"

void EditorService::Initialize(DebugLogSystem* debug, ResourceService* resource, SceneManager* manager) {

	debugLogSystem = debug;
	resourceService = resource;
	sceneManager = manager;

	UIs.clear();
	UIs.push_back(new MenuBar());
	UIs.push_back(new PerformanceMonitor());
	UIs.push_back(new Hierarchy());
	UIs.push_back(new Inspector());
	UIs.push_back(new AssetsBrowser());
	UIs.push_back(new DebugLogWindow());

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
	for (auto ui : UIs) {
		ui->Finalize();
		delete ui;
		ui = nullptr;
	}
	UIs.clear();
}

