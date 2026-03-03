// =======================================================================
// 
// ViewWindow.cpp
// 
// =======================================================================
#include "ViewWindow.h"
#include "Backends/ImGui/imgui.h"
#include "Backends/ImGui/ImGuizmo.h"
#include "DebugTools/ImGuiSystem.h"
#include <d3d11.h>
#include "scene.h"
#include "sceneManager.h"
#include "Service/Graphics/graphicsContext.h"
#include "Editor/UI/MenuBar.h"
#include <Component/RenderLayerComponent.h>
#include "Registry/systemRegistry.h"
#include "System/Render/RenderSystem/renderSystem.h"
#include "System/Render/RenderSystem/RenderPass/EditorView/EditorPass.h"
#include "Resources/resourceService.h"
#include "Resources/Loader/textureLoader.h"
#include "Hierarchy.h"
#include "Registry/componentRegistry.h"
#include "Component/transformComponent.h"
#include "Editor/Command/TransformChangeCommand.h"

void ViewWindow::Initialize(EditorService* editor) {
	ResourceService* resourceService = editor->resourceService;

	m_editor = editor;
	// ボタンテクスチャ読み込み
	PlayButtonTexture = resourceService->Load<TextureData> ("Asset/Texture/UI/Control/Play.png");
	PauseButtonTexture = resourceService->Load<TextureData>("Asset/Texture/UI/Control/Pause.png");
	StopButtonTexture = resourceService->Load<TextureData> ("Asset/Texture/UI/Control/Stop.png");
	StepButtonTexture = resourceService->Load<TextureData> ("Asset/Texture/UI/Control/Step.png");
}

void ViewWindow::Draw(const EditorDrawContext ctx) {

	EditorView(ctx);
}

void ViewWindow::EditorView(const EditorDrawContext ctx) {

	GraphicsContext* graphicsContext = m_editor->sceneManager->GetContext()->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();
	RenderSystem* renderSystem = m_editor->sceneManager->systemRegistry->GetSystem<RenderSystem>();

	bool* showEditor = &m_editor->GetUI<MenuBar>()->showEditorView;

	if (!*showEditor) {
		return;
	}

	ImGuiWindowFlags toolbar_window_flags = 0;
	//toolbar_window_flags |= ImGuiWindowFlags_NoCollapse;
	ImGui::Begin("Editor View", showEditor, toolbar_window_flags);

	ControllButton();
	DrawRenderLayerToggleUI();

	ImGui::Separator();

	ImVec2 avail = ImGui::GetContentRegionAvail(); // ウィンドウ内の利用可能サイズ
	float availAspect = avail.x / avail.y;

	m_editor->sceneManager->GetContext()->EditorScreenSize = Vector2(avail.x, avail.y);

	ImVec2 cursor = ImGui::GetCursorPos();

	// ImGuizmo の描画領域を設定
	ImGuizmo::SetRect(
		ImGui::GetWindowPos().x + cursor.x,
		ImGui::GetWindowPos().y + cursor.y,
		avail.x,
		avail.y
	);
	ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

	ImGui::Image((ImTextureRef)renderSystem->m_EditorPass->result, avail);

	// ImGuiIO を取得
	ImGuiIO& io = ImGui::GetIO();

	// マウスホイールの値をリセット
	m_MouseWheel = 0.0f;
	mouseOnEditor = false;

	// マウスカーソルの位置を取得
	POINT cursorPos;
	if (GetCursorPos(&cursorPos)) {
		// 現在のウィンドウのハンドルを取得
		HWND hwnd = GetActiveWindow();
		if (hwnd) {
			// スクリーン座標をウィンドウのクライアント座標に変換
			ScreenToClient(hwnd, &cursorPos);

			// 現在のウィンドウの描画リストを取得
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 mousePos = io.MousePos;

			// マウスカーソルがウィンドウ内にあるかを判定
			if (drawList->GetClipRectMin().x <= mousePos.x && mousePos.x <= drawList->GetClipRectMax().x &&
				drawList->GetClipRectMin().y <= mousePos.y && mousePos.y <= drawList->GetClipRectMax().y) {

				mouseOnEditor = true;

				// マウスホイールの入力を取得
				m_MouseWheel = io.MouseWheel;
			}
		}
	}

	ImGui::End();

	if (mouseOnEditor) {
		ImGuiIO& io = ImGui::GetIO();
		// マウス右クリックで操作有効
		static bool isCameraBufferActive = false;
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			isCameraBufferActive = true;
		} else if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
			isCameraBufferActive = false;
		}
		// 入力で動かすベクトル
		Vector3 velocity = { 0,0,0 };
		float speed = 1.0f;
		if (isCameraBufferActive) {
			// ------ 1. マウス移動で回転 ------
			float mouseSensitivity = 0.005f;
			m_editorCameraRotation.y += io.MouseDelta.x * mouseSensitivity; // yaw
			m_editorCameraRotation.x += io.MouseDelta.y * mouseSensitivity; // pitch
			// ピッチ制限
			const float pitchLimit = DirectX::XM_PIDIV2 - 0.01f;
			if (m_editorCameraRotation.x > pitchLimit) m_editorCameraRotation.x = pitchLimit;
			if (m_editorCameraRotation.x < -pitchLimit) m_editorCameraRotation.x = -pitchLimit;
			// 回転から方向ベクトル取得
			TransformComponent transform;
			transform.position = m_EditorCameraPosition;
			transform.SetRotationEuler(m_editorCameraRotation);
			Vector3 front = transform.front();
			Vector3 right = transform.right();
			Vector3 up = Vector3(0, 1, 0);
			if (ImGui::IsKeyDown(ImGuiKey_W)) velocity += front;
			if (ImGui::IsKeyDown(ImGuiKey_S)) velocity -= front;
			if (ImGui::IsKeyDown(ImGuiKey_A)) velocity -= right;
			if (ImGui::IsKeyDown(ImGuiKey_D)) velocity += right;
			if (ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_LeftShift)) velocity -= up;
			if (ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_Space)) velocity += up;
			if (velocity.length() > 0.0f) {
				m_EditorCameraPosition += velocity.normalize() * speed * (float)(ctx.UpdateTime + ctx.DrawTime);
			}
		} else {
			TransformComponent transform;
			transform.position = m_EditorCameraPosition;
			transform.SetRotationEuler(m_editorCameraRotation);
			Vector3 front = transform.front();
			Vector3 right = transform.right();
			Vector3 up = transform.up();
			if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
				float panSensitivity = 0.1f;
				m_EditorCameraPosition -= right * io.MouseDelta.x * panSensitivity;
				m_EditorCameraPosition += up * io.MouseDelta.y * panSensitivity;
			}
			if (m_MouseWheel != 0.0f) {
				m_EditorCameraPosition += front * m_MouseWheel * 2.0f;
			}
		}
	}

	auto hierarchy = m_editor->GetUI<Hierarchy>();
	if (hierarchy) {
		Entity selectedEntity = hierarchy->selectedEntity;

		if (!hierarchy->sceneContext) {
			return;
		}

		ComponentRegistry* registry = hierarchy->sceneContext->component;

		if (selectedEntity == 0 || !registry) {
			return;
		}

		TransformComponent* transform = registry->GetComponent<TransformComponent>(selectedEntity);

		if (transform && m_editor->GetUI<MenuBar>()->showEditorView) {

			DirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, hierarchy->sceneContext->component);

			DirectX::XMMATRIX modelMatrix;

			auto* sprite = registry->GetComponent<SpriteRendererComponent>(selectedEntity);

			if(sprite){
				// temp は CalculateRectTransform の結果（position: ピクセル位置, scale: ピクセルスケール, rotation: Euler）
				TransformComponent temp = transform->CalculateRectTransform(Vector2(avail.x, avail.y), *sprite, *transform);

				// 2D 用のアフィン変換を使う（ピボットを origin にして回転・スケールを適用）
				// 前提（重要）: sprite のローカル頂点は 0..1 の範囲に定義されている（左上が (0,0)、右下が (1,1) 等）。
				// もし頂点定義が異なれば pivot の扱いを頂点定義に合わせてください。
				DirectX::XMVECTOR scaling = DirectX::XMVectorSet(temp.scale.x, temp.scale.y, 1.0f, 0.0f); // ピクセルスケール
				DirectX::XMVECTOR rotationOrigin = DirectX::XMVectorSet(sprite->pivot.x, sprite->pivot.y, 0.0f, 0.0f); // ローカル単位（0..1）
				float rotationZ = temp.GetRotationEuler().z; // 2D 回転は Z 軸回り（ラジアン）
				DirectX::XMVECTOR translation = DirectX::XMVectorSet(temp.position.x, temp.position.y, temp.position.z, 0.0f);

				// XMMatrixAffineTransformation2D(Scaling, RotationOrigin, Rotation, Translation)
				DirectX::XMMATRIX model2D = DirectX::XMMatrixAffineTransformation2D(scaling, rotationOrigin, rotationZ, translation);

				// World はこの model2D（imGuizmo 用行列）
				World = model2D;

				// Gizmo 描画（RenderGizmo2D は ortho と SetRect を使う前提）
				modelMatrix = m_editor->sceneManager->GetContext()->imgui->RenderGizmo2D(World, DirectX::XMFLOAT2(avail.x, avail.y));
			} else{
				modelMatrix = m_editor->sceneManager->GetContext()->imgui->RenderGizmo(World);
			}
			Entity Parent = transform->parent;
			while (Parent != 0) {
				auto* ParentTransform = registry->GetComponent<TransformComponent>(Parent);
				if (ParentTransform) {

					DirectX::XMMATRIX ParentWorld = ParentTransform->CalculateWorldMatrix(ParentTransform, hierarchy->sceneContext->component);

					modelMatrix = modelMatrix * DirectX::XMMatrixInverse(nullptr, ParentWorld);

					Parent = ParentTransform->parent;
				} else {
					Parent = 0;
				}
			}
			bool isUsingNow = ImGuizmo::IsUsing();

			// ギズモ開始時：変更前の Transform をキャプチャ
			if (isUsingNow && !m_wasUsingGizmo) {
				m_gizmoEntity     = selectedEntity;
				m_gizmoStartPos   = transform->position;
				m_gizmoStartRot   = transform->GetRotation();
				m_gizmoStartScale = transform->scale;
				m_gizmoStartState = m_editor->sceneManager->State; // ドラッグ開始時の状態を記録
			}

			if (isUsingNow) {
				// スケール、回転、並進を格納する変数
				DirectX::XMVECTOR scale, rotationQuat, translation;

				// 行列を分解
				DirectX::XMMatrixDecompose(&scale, &rotationQuat, &translation, modelMatrix);

				// XMVECTOR から XMFLOAT3 に変換
				DirectX::XMFLOAT3 scale3, translation3;
				DirectX::XMStoreFloat3(&scale3, scale);
				DirectX::XMStoreFloat3(&translation3, translation);

				// rotationQuat はクォータニオンなのでそのまま XMFLOAT4 に書き出し
				DirectX::XMFLOAT4 quat;
				DirectX::XMStoreFloat4(&quat, rotationQuat);

				if (sprite) {
					TransformComponent edited;
					edited.position = translation3;
					edited.SetRotation(quat);     // クォータニオンを代入
					edited.scale = scale3;

					edited = transform->ReverseCalculateRectTransform(Vector2(avail.x, avail.y), *sprite, edited);

					transform->position = edited.position;
					transform->SetRotation(edited.GetRotation());
					transform->scale = edited.scale;
				} else {
					transform->position = translation3;
					transform->SetRotation(quat); // クォータニオンを保持
					transform->scale = scale3;
				}
			}

			// ギズモ終了時：変更をコマンドスタックに積む（Execute は既に適用済み）
			// エディタ停止状態でのみ記録（プレイ中ドラッグ→停止後 Undo エラーを防止）
			if (!isUsingNow && m_wasUsingGizmo && m_gizmoEntity != 0) {
				if (m_gizmoStartState == SceneManagerState::Stopped &&
					m_editor->sceneManager->State == SceneManagerState::Stopped) {
					TransformComponent* t = registry->GetComponent<TransformComponent>(m_gizmoEntity);
					if (t && hierarchy->sceneContext) {
						auto cmd = std::make_unique<TransformChangeCommand>(
							hierarchy->sceneContext, m_gizmoEntity,
							m_gizmoStartPos, m_gizmoStartRot, m_gizmoStartScale,
							t->position,     t->GetRotation(), t->scale);
						m_editor->commandManager.Push(std::move(cmd));
					}
				}
				m_gizmoEntity = 0;
			}

			m_wasUsingGizmo = isUsingNow;
		}
	}
}

void ViewWindow::ControllButton() {

	if (!PlayButtonTexture) {
		return;
	}

	ImTextureRef Play;
	Play._TexID = (ImTextureID)PlayButtonTexture.get()->pTexture.Get();
	ImTextureRef Pause;
	Pause._TexID = (ImTextureID)PauseButtonTexture.get()->pTexture.Get();
	ImTextureRef Stop;
	Stop._TexID = (ImTextureID)StopButtonTexture.get()->pTexture.Get();
	ImTextureRef Step;
	Step._TexID = (ImTextureID)StepButtonTexture.get()->pTexture.Get();

	ImVec4 DefaultButtonColor = ImVec4(1.0f, 1.0f, 1.0f, 0.8f);
	ImVec4 StopButtonColor = ImVec4(1.0f, 1.0f, 1.0f, 0.8f);
	if (m_editor->sceneManager->State == SceneManagerState::Stopped) {
		StopButtonColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
	}
	ImGui::BeginDisabled(m_editor->sceneManager->State == SceneManagerState::Stopped);

	if (ImGui::ImageButton("Stop", Stop, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), StopButtonColor)) {
		if (m_editor->sceneManager->State != SceneManagerState::Stopped) {

			m_editor->sceneManager->State = SceneManagerState::Stopped; // シーンの状態をエディタに戻す
			ImGui::SetWindowFocus("Editor View");
		}
	}
	ImGui::EndDisabled();

	ImGui::SameLine();

	// ツールバー内容
	if (m_editor->sceneManager->State == SceneManagerState::Playing) {
		if (ImGui::ImageButton("Pause", Pause, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), DefaultButtonColor)) {
			m_editor->sceneManager->State = SceneManagerState::Paused; // シーンの状態を 一時停止に変更
		}
	} else {
		if (ImGui::ImageButton("Play", Play, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), DefaultButtonColor)) {
			m_editor->sceneManager->State = SceneManagerState::Playing; // シーンの状態を再生中に変更
			ImGui::SetWindowFocus("Play View");
		}
	}
	ImGui::SameLine();
	if (ImGui::ImageButton("Step", Step, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), DefaultButtonColor)) {
		m_editor->sceneManager->State = SceneManagerState::Step; // シーンの状態を ステップに変更
	}
}

void ViewWindow::DrawRenderLayerToggleUI() {
	RenderSystem* renderSystem = m_editor->sceneManager->systemRegistry->GetSystem<RenderSystem>();

	ImGui::SameLine();

	std::string previewText;
	for (int i = 0; i < (int)RenderLayer::MaxRenderLayer; ++i) {
		if (renderSystem->editorRenderLayerVisible[i]) {
			if (!previewText.empty()) previewText += ", ";
			previewText += GetRenderLayerName(static_cast<RenderLayer>(i));
		}
	}
	if (previewText.empty()) previewText = "None";

	if (ImGui::BeginCombo("##Visible Layers", previewText.c_str())) {
		for (int i = 0; i < (int)RenderLayer::MaxRenderLayer; ++i) {
			ImGui::Selectable(
				GetRenderLayerName(static_cast<RenderLayer>(i)),
				&renderSystem->editorRenderLayerVisible[i],
				ImGuiSelectableFlags_DontClosePopups
			);
		}
		ImGui::EndCombo();
	}
}

