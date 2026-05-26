// =======================================================================
// 
// Hierarchy.h
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

// ヒエラルキー（シーン階層）表示UI
class Hierarchy: public IEditorUI{

public:
	void Initialize(EditorService* editor) override{
		editor = editor;
	}
	void Finalize() override{}
	void Draw(const EditorDrawContext ctx) override;

	Entity selectedEntity = 0;
	SceneContext* sceneContext = nullptr;

private:
	void DrawHierarchyNode(Entity entity, SceneContext* context, const std::unordered_set<Entity>& allEntities);

	EditorService* editor = nullptr;

	Entity pendingRenameEntity = 0;

	char searchBuffer[256] = "";
	char renameBuffer[256] = "";
};
