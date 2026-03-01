#pragma once

#include "GameApplication.h"

#include "Editor/editorService.h"
#include "Editor/InterFace/IEditorUI.h"

#include <unordered_set>
#include <string>
#include "Entity/Entity.h"

struct SceneContext;

class Hierarchy: public IEditorUI{

public:
	void Initialize(EditorService* editor) override{
		m_editor = editor;
	}
	void Finalize() override{}
	void Draw(const EditorDrawContext ctx) override;

	Entity selectedEntity = 0;
	SceneContext* sceneContext = nullptr;

private:
	void DrawHierarchyNode(Entity entity, SceneContext* context, const std::unordered_set<Entity>& allEntities);
	void DestroyEntityRecursive(Entity entity, SceneContext* context);
	void SetParent(Entity child, Entity newParent, SceneContext* context);
	Entity DuplicateEntityRecursive(Entity src, Entity newParent, SceneContext* context);

	EditorService* m_editor = nullptr;

	Entity deleteEntity = 0;
	Entity pendingRenameEntity = 0;

	char renameBuffer[256] = "";
};
