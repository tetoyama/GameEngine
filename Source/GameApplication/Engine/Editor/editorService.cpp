#include "editorService.h"
#include "Editor/InterFace/IEditorUI.h"
#include "UI/ImGuiMainManuBar.h"
#include "UI/DebugWindow.h"

void EditorService::Initialize() {
	UIs.clear();

	UIs.push_back(new Manubar());
	UIs.push_back(new DebugWindow());

	for (auto ui : UIs) {
		ui->Initialize();
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

Manubar* EditorService::GetManubar() {
	for (IEditorUI* ui : UIs) {
		if (auto* m = dynamic_cast<Manubar*>(ui)) {
			return (Manubar*)ui;
		}
	}
	return nullptr;
}

