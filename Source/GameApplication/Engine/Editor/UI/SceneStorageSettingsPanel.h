#pragma once
#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"
#include "Editor/UI/SceneStorageSettingsView.h"
#include "Editor/UI/SceneStorageRuntimeTelemetryView.h"

class SceneStorageSettingsPanel final : public IEditorUI {
public:
	void Initialize(EditorService* editor) override { m_editor = editor; }
	void Draw(const EditorDrawContext) override {
		MenuBar* menu = m_editor ? m_editor->GetUI<MenuBar>() : nullptr;
		bool* open = menu ? &menu->showSceneSettings : nullptr;
		SceneStorageSettingsView::Draw(m_editor, open);

		const bool visible = open && *open;
		SceneStorageRuntimeTelemetryView::Draw(
			m_editor,
			visible,
			SceneStorageSettingsView::GetState().selectedSceneName
		);
	}
	void Finalize() override {}
private:
	EditorService* m_editor = nullptr;
};
