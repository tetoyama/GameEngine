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
		m_pEditor = editor;
	}
	void Finalize() override{}
	void Draw(const EditorDrawContext ctx) override;

	Entity selectedEntity = 0;
	SceneContext* sceneContext = nullptr;

private:
	void DrawHierarchyNode(Entity entity, SceneContext* context, const std::unordered_set<Entity>& allEntities);

	EditorService* m_pEditor = nullptr;

	Entity m_PendingRenameEntity= 0;

	char m_SearchBuffer[256] = "";
	char m_RenameBuffer[256] = "";
};
