#pragma once
#include "GameApplication.h"

#include "Editor/editorService.h"
#include "Editor/InterFace/IEditorUI.h"

#include <unordered_set>
#include <string>
#include "Entity/Entity.h"

#include <Backends/myVector3.h>
#include <memory>

struct TextureData;
struct SceneContext;

class ViewWindow : public IEditorUI {

public:
	void Initialize(EditorService* editor) override;
	void Finalize() override {}
	void Draw(const EditorDrawContext ctx) override;

	Vector3 m_EditorCameraPosition = Vector3(0.0f, 5.0f, -20.0f);
	Vector3 m_EditorCameraRotation = Vector3(0.0f, 0.0f, 0.0f);

	bool mouseOnEditor = false;

private:
	EditorService* m_editor = nullptr;

	void EditorView(const EditorDrawContext ctx);

	void ControllButton();
	void DrawRenderLayerToggleUI();

	float m_MouseWheel = 0.0f;

	std::shared_ptr<TextureData> PlayButtonTexture;
	std::shared_ptr<TextureData> PauseButtonTexture;
	std::shared_ptr<TextureData> StopButtonTexture;
	std::shared_ptr<TextureData> StepButtonTexture;
};
