// =======================================================================
// 
// SystemSetting.h
// 
// =======================================================================
#pragma once
#include "GameApplication.h"

#include "Editor/editorService.h"
#include "Editor/InterFace/IEditorUI.h"

#include <unordered_set>
#include <string>
#include "Entity/Entity.h"

struct SceneContext;

// システム設定UI
class SystemSetting : public IEditorUI {

public:
	void Initialize(EditorService* editor) override {
		m_editor = editor;
	}
	void Finalize() override {}
	void Draw(const EditorDrawContext ctx) override;

private:
	EditorService* m_editor = nullptr;
};
