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
#include <System/Render/RenderSystem/RenderTarget/renderTarget.h>
#include "System/Render/RenderSystem/RenderPass/GBuffer/GBufferPass.h"

void ViewWindow::Initialize(EditorService* editor) {
	ResourceService* resourceService = editor->resourceService;

	m_editor = editor;
	// ボタンテクスチャ読み込み
	PlayButtonTexture = resourceService->Load<TextureData> ("Asset/Texture/UI/Control/Play.png");
	PauseButtonTexture = resourceService->Load<TextureData>("Asset/Texture/UI/Control/Pause.png");
	StopButtonTexture = resourceService->Load<TextureData> ("Asset/Texture/UI/Control/Stop.png");
	StepButtonTexture = resourceService->Load<TextureData> ("Asset/Texture/UI/Control/Step.png");

	ImGui::SetWindowFocus("Editor View");
}

void ViewWindow::Draw(const EditorDrawContext ctx) {

	EditorView(ctx);
}

void ViewWindow::EditorView(const EditorDrawContext ctx){

	GraphicsContext* graphicsContext = m_editor->sceneManager->GetContext()->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext ? graphicsContext->GetDeviceContext() : nullptr;
	SystemRegistry* registry = m_editor->sceneManager->GetSystemRegistry();
	RenderSystem* renderSystem = registry ? registry->GetSystem<RenderSystem>() : nullptr;
	if(!graphicsContext || !deviceContext || !renderSystem || !renderSystem->m_EditorPass) return;

	bool* showEditor = &m_editor->GetUI<MenuBar>()->showEditorView;

	if(!*showEditor){
		return;
	}

	ImGuiWindowFlags toolbar_window_flags = 0;
	ImGui::Begin("Editor View", showEditor, toolbar_window_flags);

	ControlButton();
	DrawRenderLayerToggleUI();
	ImGui::SameLine();
	std::string speedText = ("CameraSpeed:" + std::to_string(cameraMoveSpeed));
	ImGui::Text(speedText.c_str());

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

	// レンダーターゲットの表示
	ImGui::Image((ImTextureRef)renderSystem->m_EditorPass->result, avail);

	// --- 修正ポイント：クリック判定の情報を一時保存（ギズモ干渉防止のため） ---
	bool isImageClicked = ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
	ImVec2 clickedMousePos = ImGui::GetMousePos();
	ImVec2 imagePos = ImGui::GetItemRectMin();
	ImVec2 imageSize = ImGui::GetItemRectSize();

	// ImGuiIO を取得
	ImGuiIO& io = ImGui::GetIO();

	// マウスホイールの値をリセット
	m_MouseWheel = 0.0f;
	mouseOnEditor = false;

	// マウスカーソルの位置を取得
	POINT cursorPos;
	if(GetCursorPos(&cursorPos)){
		HWND hwnd = GetActiveWindow();
		if(hwnd){
			ScreenToClient(hwnd, &cursorPos);
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 mousePos = io.MousePos;

			// マウスカーソルがウィンドウ内にあるかを判定
			if(drawList->GetClipRectMin().x <= mousePos.x && mousePos.x <= drawList->GetClipRectMax().x &&
			   drawList->GetClipRectMin().y <= mousePos.y && mousePos.y <= drawList->GetClipRectMax().y){

				mouseOnEditor = true;
				m_MouseWheel = io.MouseWheel;
			}
		}
	}

	ImGui::End();

	// --- カメラ操作ロジック ---
	if(mouseOnEditor){
		ImGuiIO& io = ImGui::GetIO();
		static bool isCameraBufferActive = false;
		if(ImGui::IsMouseClicked(ImGuiMouseButton_Right)){
			isCameraBufferActive = true;
		} else if(!ImGui::IsMouseDown(ImGuiMouseButton_Right)){
			isCameraBufferActive = false;
		}

		Vector3 velocity = {0,0,0};
		float speed = cameraMoveSpeed;
		if(isCameraBufferActive){
			float mouseSensitivity = 0.005f;
			m_editorCameraRotation.y += io.MouseDelta.x * mouseSensitivity;
			m_editorCameraRotation.x += io.MouseDelta.y * mouseSensitivity;

			const float pitchLimit = DirectX::XM_PIDIV2 - 0.01f;
			if(m_editorCameraRotation.x > pitchLimit) m_editorCameraRotation.x = pitchLimit;
			if(m_editorCameraRotation.x < -pitchLimit) m_editorCameraRotation.x = -pitchLimit;

			TransformComponent transform;
			transform.position = m_EditorCameraPosition;
			transform.SetRotationEuler(m_editorCameraRotation);
			Vector3 front = transform.front();
			Vector3 right = transform.right();
			Vector3 up = Vector3(0, 1, 0);

			if(ImGui::IsKeyDown(ImGuiKey_W)) velocity += front;
			if(ImGui::IsKeyDown(ImGuiKey_S)) velocity -= front;
			if(ImGui::IsKeyDown(ImGuiKey_A)) velocity -= right;
			if(ImGui::IsKeyDown(ImGuiKey_D)) velocity += right;
			if(ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_LeftShift)) velocity -= up;
			if(ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_Space)) velocity += up;
			if(velocity.length() > 0.0f){
				m_EditorCameraPosition += velocity.normalize() * speed * (float)(ctx.UpdateTime + ctx.DrawTime);
			}

			if(m_MouseWheel != 0.0f){
				cameraMoveSpeed += m_MouseWheel * 0.25f;
			}
		} else{
			TransformComponent transform;
			transform.position = m_EditorCameraPosition;
			transform.SetRotationEuler(m_editorCameraRotation);
			Vector3 front = transform.front();
			Vector3 right = transform.right();
			Vector3 up = transform.up();
			if(ImGui::IsMouseDown(ImGuiMouseButton_Middle)){
				float panSensitivity = 0.1f;
				m_EditorCameraPosition -= right * io.MouseDelta.x * panSensitivity;
				m_EditorCameraPosition += up * io.MouseDelta.y * panSensitivity;
			}
			if(m_MouseWheel != 0.0f){
				m_EditorCameraPosition += front * m_MouseWheel * 2.0f;
			}
		}
	}

	// --- ギズモおよび選択オブジェクトの更新 ---
	auto hierarchy = m_editor->GetUI<Hierarchy>();
	if(hierarchy){
		Entity selectedEntity = hierarchy->selectedEntity;

		if(hierarchy->sceneContext && selectedEntity != 0){
			ComponentRegistry* registry = hierarchy->sceneContext->component;
			TransformComponent* transform = registry->GetComponent<TransformComponent>(selectedEntity);

			if(transform && m_editor->GetUI<MenuBar>()->showEditorView){
				DirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, registry);
				DirectX::XMMATRIX modelMatrix;

				auto* sprite = registry->GetComponent<SpriteRendererComponent>(selectedEntity);
				if(sprite){
					TransformComponent temp = transform->CalculateRectTransform(Vector2(avail.x, avail.y), *sprite, *transform);
					DirectX::XMVECTOR scaling = DirectX::XMVectorSet(temp.scale.x, temp.scale.y, 1.0f, 0.0f);
					DirectX::XMVECTOR rotationOrigin = DirectX::XMVectorSet(sprite->pivot.x, sprite->pivot.y, 0.0f, 0.0f);
					float rotationZ = temp.GetRotationEuler().z;
					DirectX::XMVECTOR translation = DirectX::XMVectorSet(temp.position.x, temp.position.y, temp.position.z, 0.0f);

					DirectX::XMMATRIX model2D = DirectX::XMMatrixAffineTransformation2D(scaling, rotationOrigin, rotationZ, translation);
					World = model2D;
					modelMatrix = m_editor->sceneManager->GetContext()->imgui->RenderGizmo2D(World, DirectX::XMFLOAT2(avail.x, avail.y));
				} else{
					modelMatrix = m_editor->sceneManager->GetContext()->imgui->RenderGizmo(World);
				}

				// 親の逆行列を適用してローカル座標系に戻す
				Entity Parent = transform->parent;
				while(Parent != 0){
					auto* ParentTransform = registry->GetComponent<TransformComponent>(Parent);
					if(ParentTransform){
						DirectX::XMMATRIX ParentWorld = ParentTransform->CalculateWorldMatrix(ParentTransform, registry);
						modelMatrix = modelMatrix * DirectX::XMMatrixInverse(nullptr, ParentWorld);
						Parent = ParentTransform->parent;
					} else{
						Parent = 0;
					}
				}

				bool isUsingNow = ImGuizmo::IsUsing();

				if(isUsingNow && !m_wasUsingGizmo){
					m_gizmoEntity = selectedEntity;
					m_gizmoStartPos = transform->position;
					m_gizmoStartRot = transform->GetRotation();
					m_gizmoStartScale = transform->scale;
					m_gizmoStartState = m_editor->sceneManager->State;
				}

				if(isUsingNow){
					DirectX::XMVECTOR scale, rotationQuat, translation;
					DirectX::XMMatrixDecompose(&scale, &rotationQuat, &translation, modelMatrix);

					DirectX::XMFLOAT3 scale3, translation3;
					DirectX::XMStoreFloat3(&scale3, scale);
					DirectX::XMStoreFloat3(&translation3, translation);
					DirectX::XMFLOAT4 quat;
					DirectX::XMStoreFloat4(&quat, rotationQuat);

					if(sprite){
						TransformComponent edited;
						edited.position = translation3;
						edited.SetRotation(quat);
						edited.scale = scale3;
						edited = transform->ReverseCalculateRectTransform(Vector2(avail.x, avail.y), *sprite, edited);
						transform->position = edited.position;
						transform->SetRotation(edited.GetRotation());
						transform->scale = edited.scale;
					} else{
						transform->position = translation3;
						transform->SetRotation(quat);
						transform->scale = scale3;
					}
				}

				if(!isUsingNow && m_wasUsingGizmo && m_gizmoEntity != 0){
					if(m_gizmoStartState == SceneManagerState::Stopped && m_editor->sceneManager->State == SceneManagerState::Stopped){
						auto cmd = std::make_unique<TransformChangeCommand>(
							hierarchy->sceneContext, m_gizmoEntity,
							m_gizmoStartPos, m_gizmoStartRot, m_gizmoStartScale,
							transform->position, transform->GetRotation(), transform->scale);
						m_editor->commandManager.Push(std::move(cmd));
					}
					m_gizmoEntity = 0;
				}
				m_wasUsingGizmo = isUsingNow;
			}
		}
	}

	// --- ギズモ操作が確定した後に最終的な Pick 処理を行う ---
	if(isImageClicked && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()){

		Vector2 uv;
		uv.x = (clickedMousePos.x - imagePos.x) / imageSize.x;
		uv.y = (clickedMousePos.y - imagePos.y) / imageSize.y;

		auto gBufferPass = renderSystem->m_EditorPass->gBufferPass;
		if(gBufferPass){
			RenderTarget* paramTarget = gBufferPass->pRenderTargets[GBufferSlot_Param];
			PickResult result = paramTarget->Pick(uv, graphicsContext);

			uint32_t sceneID_val = result.u[0];  // x : SceneID
			uint32_t objectID_val = result.u[1]; // y : ObjectID

			if(hierarchy){
				// 管理テーブルから安全にコンテキストを復元
				SceneContext* recoveredContext = m_editor->sceneManager->GetContextFromID(sceneID_val);

				if(recoveredContext){
					const Entity pickedEntity = recoveredContext->entity->Resolve(objectID_val);
					if(!pickedEntity) return;

					hierarchy->sceneContext = recoveredContext;
					hierarchy->selectedEntity = pickedEntity;

					// 選択した Entity にカメラを向ける
					TransformComponent* transform = recoveredContext->component->GetComponent<TransformComponent>(hierarchy->selectedEntity);
					if(transform && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){
						Vector3 targetPos = transform->position;
						Vector3 direction = targetPos - m_EditorCameraPosition;
						float distance = direction.length();
						if(distance > 0.001f){
							direction = direction.normalize();
							m_editorCameraRotation.y = atan2f(direction.x, direction.z);
							m_editorCameraRotation.x = asinf(-direction.y);
						}
					}
				}
			}
		}
	}
}
void ViewWindow::ControlButton() {

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

