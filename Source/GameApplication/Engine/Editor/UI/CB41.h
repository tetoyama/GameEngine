#pragma once
#include "GameApplication.h"

#include "Editor/editorService.h"
#include "Editor/InterFace/IEditorUI.h"

struct SceneManagerContext;

class resourceService;

class CB41 : public IEditorUI{

public:
	void Initialize(EditorService* editor) override;
	void Finalize() override;
	void Draw(const EditorDrawContext ctx) override;

private:

	char m_InputBuffer[2048]{};

	EditorService* m_pEditor = nullptr;
	ResourceService* resourceService = nullptr;
};
