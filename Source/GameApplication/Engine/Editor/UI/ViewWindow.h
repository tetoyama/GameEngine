// =======================================================================
// 
// ViewWindow.h
// 
// =======================================================================
#pragma once
#include "GameApplication.h"

#include "Editor/editorService.h"
#include "Editor/InterFace/IEditorUI.h"
#include "Scene/sceneManager.h"

#include <unordered_set>
#include <string>
#include "Entity/Entity.h"

#include <Backends/myVector3.h>
#include <memory>
#include <DirectXMath.h>

struct TextureData;
struct SceneContext;

// ビューウィンドウ（シーンプレビュー）UI
class ViewWindow : public IEditorUI {

public:
	void Initialize(EditorService* editor) override;
	void Finalize() override {}
	void Draw(const EditorDrawContext ctx) override;

	Vector3 m_EditorCameraPosition = Vector3(0.0f, 5.0f, -20.0f);
	Vector3 m_editorCameraRotation = Vector3(0.0f, 0.0f, 0.0f);

	bool mouseOnEditor = false;
	float cameraMoveSpeed = 10.0f;

private:
	EditorService* m_editor = nullptr;

	void EditorView(const EditorDrawContext ctx);

	void ControlButton();
	void DrawRenderLayerToggleUI();

	float m_MouseWheel = 0.0f;

	// ImGuizmo ドラッグ追跡（Undo/Redo 用）
	bool               m_wasUsingGizmo = false;
	Entity             m_gizmoEntity   = 0;
	SceneManagerState  m_gizmoStartState = SceneManagerState::STOPPED; // ドラッグ開始時のシーン状態
	Vector3            m_gizmoStartPos;
	DirectX::XMFLOAT4  m_gizmoStartRot{0.f, 0.f, 0.f, 1.f};
	Vector3            m_gizmoStartScale{1.f, 1.f, 1.f};

	std::shared_ptr<TextureData> PlayButtonTexture;
	std::shared_ptr<TextureData> PauseButtonTexture;
	std::shared_ptr<TextureData> StopButtonTexture;
	std::shared_ptr<TextureData> StepButtonTexture;
};
