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
	ResourceService* pResourceService = editor->pResourceService;

	m_pEditor = editor;
	// ボタンテクスチャ読み込み
	m_PlayButtonTexture = pResourceService->Load<TextureData> ("Asset/Texture/UI/Control/Play.png");
	m_PauseButtonTexture = pResourceService->Load<TextureData>("Asset/Texture/UI/Control/Pause.png");
	m_StopButtonTexture = pResourceService->Load<TextureData> ("Asset/Texture/UI/Control/Stop.png");
	m_StepButtonTexture = pResourceService->Load<TextureData> ("Asset/Texture/UI/Control/Step.png");
}

void ViewWindow::Draw(const EditorDrawContext ctx) {

	EditorView(ctx);
}

void ViewWindow::EditorView(const EditorDrawContext ctx){

	GraphicsContext* graphicsContext = m_pEditor->pSceneManager->GetContext()->pGraphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();
	RenderSystem* renderSystem = m_pEditor->pSceneManager->pSystemRegistry->GetSystem<RenderSystem>();

	bool* pShowEditor = &m_pEditor->GetUI<MenuBar>()->showEditorView;

	if(!*pShowEditor){
		return;
	}

	ImGuiWindowFlags toolbarWindowFlags= 0;
	ImGui::Begin("Editor View", pShowEditor, toolbarWindowFlags);

	ControlButton();
	DrawRenderLayerToggleUI();
	ImGui::SameLine();
	std::string speedText= ("CameraSpeed:" + std::to_string(cameraMoveSpeed));
	ImGui::Text(speedText.c_str());

	ImGui::Separator();

	ImVec2 avail= ImGui::GetContentRegionAvail(); // ウィンドウ内の利用可能サイズ
	float availAspect= avail.x / avail.y;

	m_pEditor->pSceneManager->GetContext()->editorScreenSize = Vector2(avail.x, avail.y);

	ImVec2 cursor= ImGui::GetCursorPos();

	// ImGuizmo の描画領域を設定
	ImGuizmo::SetRect(
		ImGui::GetWindowPos().x + cursor.x,
		ImGui::GetWindowPos().y + cursor.y,
		avail.x,
		avail.y
	);
	ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

	// レンダーターゲットの表示
	ImGui::Image((ImTextureRef)renderSystem->m_EditorPass->pResult, avail);

	// --- 修正ポイント：クリック判定の情報を一時保存（ギズモ干渉防止のため） ---
	bool m_IsImageClicked= ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
	ImVec2 m_ClickedMousePos= ImGui::GetMousePos();
	ImVec2 m_ImagePos= ImGui::GetItemRectMin();
	ImVec2 m_ImageSize= ImGui::GetItemRectSize();

	// ImGuiIO を取得
	ImGuiIO& io = ImGui::GetIO();

	// マウスホイールの値をリセット
	m_MouseWheel = 0.0f;
	mouseOnEditor = false;

	// マウスカーソルの位置を取得
	POINT cursorPos;
	if(GetCursorPos(&cursorPos)){
		HWND hwnd= GetActiveWindow();
		if(hwnd){
			ScreenToClient(hwnd, &cursorPos);
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 mousePos= io.MousePos;

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
		static bool isCameraBufferActive= false;
		if(ImGui::IsMouseClicked(ImGuiMouseButton_Right)){
			isCameraBufferActive = true;
		} else if(!ImGui::IsMouseDown(ImGuiMouseButton_Right)){
			isCameraBufferActive = false;
		}

		Vector3 velocity= {0,0,0};
		float speed= cameraMoveSpeed;
		if(isCameraBufferActive){
			float mouseSensitivity= 0.005f;
			editorCameraRotation.y += io.MouseDelta.x * mouseSensitivity;
			editorCameraRotation.x += io.MouseDelta.y * mouseSensitivity;

			const float pitchLimit= DirectX::XM_PIDIV2 - 0.01f;
			if(editorCameraRotation.x > pitchLimit) editorCameraRotation.x = pitchLimit;
			if(editorCameraRotation.x < -pitchLimit) editorCameraRotation.x = -pitchLimit;

			TransformComponent transform;
			transform.position = editorCameraPosition;
			transform.SetRotationEuler(editorCameraRotation);
			Vector3 m_Front= transform.front();
			Vector3 m_Right= transform.right();
			Vector3 m_Up= Vector3(0, 1, 0);

			if(ImGui::IsKeyDown(ImGuiKey_W)) velocity += m_Front;
			if(ImGui::IsKeyDown(ImGuiKey_S)) velocity -= m_Front;
			if(ImGui::IsKeyDown(ImGuiKey_A)) velocity -= m_Right;
			if(ImGui::IsKeyDown(ImGuiKey_D)) velocity += m_Right;
			if(ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_LeftShift)) velocity -= m_Up;
			if(ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_Space)) velocity += m_Up;
			if(velocity.length() > 0.0f){
				editorCameraPosition += velocity.normalize() * speed * (float)(ctx.updateTime + ctx.drawTime);
			}

			if(m_MouseWheel != 0.0f){
				cameraMoveSpeed += m_MouseWheel * 0.25f;
			}
		} else{
			TransformComponent transform;
			transform.position = editorCameraPosition;
			transform.SetRotationEuler(editorCameraRotation);
			Vector3 front= transform.front();
			Vector3 right= transform.right();
			Vector3 up= transform.up();
			if(ImGui::IsMouseDown(ImGuiMouseButton_Middle)){
				float panSensitivity= 0.1f;
				editorCameraPosition -= right * io.MouseDelta.x * panSensitivity;
				editorCameraPosition += up * io.MouseDelta.y * panSensitivity;
			}
			if(m_MouseWheel != 0.0f){
				editorCameraPosition += front * m_MouseWheel * 2.0f;
			}
		}
	}

	// --- ギズモおよび選択オブジェクトの更新 ---
	auto hierarchy= m_pEditor->GetUI<Hierarchy>();
	if(hierarchy){
		Entity selectedEntity= hierarchy->selectedEntity;

		if(hierarchy->pSceneContext && selectedEntity != 0){
			ComponentRegistry* registry = hierarchy->pSceneContext->pComponent;
			TransformComponent* transform = registry->GetComponent<TransformComponent>(selectedEntity);

			if(transform && m_pEditor->GetUI<MenuBar>()->showEditorView){
				DirectX::XMMATRIX m_World= transform->CalculateWorldMatrix(transform, registry);
				DirectX::XMMATRIX m_ModelMatrix;

				auto* sprite = registry->GetComponent<SpriteRendererComponent>(selectedEntity);
				if(sprite){
					TransformComponent temp= transform->CalculateRectTransform(Vector2(avail.x, avail.y), *sprite, *transform);
					DirectX::XMVECTOR scaling= DirectX::XMVectorSet(temp.scale.x, temp.scale.y, 1.0f, 0.0f);
					DirectX::XMVECTOR rotationOrigin= DirectX::XMVectorSet(sprite->pivot.x, sprite->pivot.y, 0.0f, 0.0f);
					float rotationZ= temp.GetRotationEuler().z;
					DirectX::XMVECTOR translation= DirectX::XMVectorSet(temp.position.x, temp.position.y, temp.position.z, 0.0f);
					DirectX::XMMATRIX model2D= DirectX::XMMatrixAffineTransformation2D(scaling, rotationOrigin, rotationZ, translation);
					m_World = model2D;
					m_ModelMatrix = m_pEditor->pSceneManager->GetContext()->pImGui->RenderGizmo2D(m_World, DirectX::XMFLOAT2(avail.x, avail.y));
				} else{
					m_ModelMatrix = m_pEditor->pSceneManager->GetContext()->pImGui->RenderGizmo(m_World);
				}

				// 親の逆行列を適用してローカル座標系に戻す
				Entity Parent = transform->parent;
				while(Parent != 0){
					auto* ParentTransform = registry->GetComponent<TransformComponent>(Parent);
					if(ParentTransform){
						DirectX::XMMATRIX m_ParentWorld= ParentTransform->CalculateWorldMatrix(ParentTransform, registry);
						m_ModelMatrix = m_ModelMatrix * DirectX::XMMatrixInverse(nullptr, m_ParentWorld);
						Parent = ParentTransform->parent;
					} else{
						Parent = 0;
					}
				}

				bool isUsingNow= ImGuizmo::IsUsing();

				if(isUsingNow && !m_WasUsingGizmo){
					m_GizmoEntity = selectedEntity;
					m_GizmoStartPos = transform->position;
					m_GizmoStartRot = transform->GetRotation();
					m_GizmoStartScale = transform->scale;
					m_GizmoStartState = m_pEditor->pSceneManager->State;
				}

				if(isUsingNow){
					DirectX::XMVECTOR scale, rotationQuat, translation;
					DirectX::XMMatrixDecompose(&scale, &rotationQuat, &translation, m_ModelMatrix);

					DirectX::XMFLOAT3 scale3, m_Translation3;
					DirectX::XMStoreFloat3(&scale3, scale);
					DirectX::XMStoreFloat3(&m_Translation3, translation);
					DirectX::XMFLOAT4 m_Quat;
					DirectX::XMStoreFloat4(&m_Quat, rotationQuat);

					if(sprite){
						TransformComponent edited;
						edited.position = m_Translation3;
						edited.SetRotation(m_Quat);
						edited.scale = scale3;
						edited = transform->ReverseCalculateRectTransform(Vector2(avail.x, avail.y), *sprite, edited);
						transform->position = edited.position;
						transform->SetRotation(edited.GetRotation());
						transform->scale = edited.scale;
					} else{
						transform->position = m_Translation3;
						transform->SetRotation(m_Quat);
						transform->scale = scale3;
					}
				}

				if(!isUsingNow && m_WasUsingGizmo && m_GizmoEntity != 0){
					if(m_GizmoStartState == SceneManagerState::Stopped && m_pEditor->pSceneManager->State == SceneManagerState::Stopped){
						auto cmd= std::make_unique<TransformChangeCommand>(
							hierarchy->pSceneContext, m_GizmoEntity,
							m_GizmoStartPos, m_GizmoStartRot, m_GizmoStartScale,
							transform->position, transform->GetRotation(), transform->scale);
						m_pEditor->commandManager.Push(std::move(cmd));
					}
					m_GizmoEntity = 0;
				}
				m_WasUsingGizmo = isUsingNow;
			}
		}
	}

	// --- ギズモ操作が確定した後に最終的な Pick 処理を行う ---
	if(m_IsImageClicked && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()){

		Vector2 m_Uv;
		m_Uv.x = (m_ClickedMousePos.x - m_ImagePos.x) / m_ImageSize.x;
		m_Uv.y = (m_ClickedMousePos.y - m_ImagePos.y) / m_ImageSize.y;

		auto m_GBufferPass= renderSystem->m_EditorPass->pGBufferPass;
		if(m_GBufferPass){
			RenderTarget* paramTarget = m_GBufferPass->pRenderTargets[GBufferSlot_Param];
			PickResult m_Result= paramTarget->Pick(m_Uv, graphicsContext);

			uint32_t sceneIdVal= m_Result.u[0];  // x : SceneID
			uint32_t objectIdVal= m_Result.u[1]; // y : ObjectID
			if(hierarchy){
				// 管理テーブルから安全にコンテキストを復元
				SceneContext* recoveredContext = m_pEditor->pSceneManager->GetContextFromID(sceneIdVal);
				if(recoveredContext){
					hierarchy->pSceneContext = recoveredContext;
					hierarchy->selectedEntity = (Entity)objectIdVal;

					// 選択した Entity にカメラを向ける
					TransformComponent* transform = recoveredContext->pComponent->GetComponent<TransformComponent>(hierarchy->selectedEntity);
					if(transform && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){
						Vector3 m_TargetPos= transform->position;
						Vector3 m_Direction= m_TargetPos - editorCameraPosition;
						float m_Distance= m_Direction.length();
						if(m_Distance > 0.001f){
							m_Direction = m_Direction.normalize();
							editorCameraRotation.y = atan2f(m_Direction.x, m_Direction.z);
							editorCameraRotation.x = asinf(-m_Direction.y);
						}
					}
				}
			}
		}
	}
}
void ViewWindow::ControlButton() {

	if (!m_PlayButtonTexture) {
		return;
	}

	ImTextureRef play;
	play._TexID = (ImTextureID)m_PlayButtonTexture.get()->pTexture.Get();
	ImTextureRef pause;
	pause._TexID = (ImTextureID)m_PauseButtonTexture.get()->pTexture.Get();
	ImTextureRef stop;
	stop._TexID = (ImTextureID)m_StopButtonTexture.get()->pTexture.Get();
	ImTextureRef step;
	step._TexID = (ImTextureID)m_StepButtonTexture.get()->pTexture.Get();

	ImVec4 m_DefaultButtonColor= ImVec4(1.0f, 1.0f, 1.0f, 0.8f);
	ImVec4 m_StopButtonColor= ImVec4(1.0f, 1.0f, 1.0f, 0.8f);
	if (m_pEditor->pSceneManager->State == SceneManagerState::Stopped) {
		m_StopButtonColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
	}
	ImGui::BeginDisabled(m_pEditor->pSceneManager->State == SceneManagerState::Stopped);

	if (ImGui::ImageButton("Stop", stop, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), m_StopButtonColor)) {
		if (m_pEditor->pSceneManager->State != SceneManagerState::Stopped) {

			m_pEditor->pSceneManager->State = SceneManagerState::Stopped; // シーンの状態をエディタに戻す
			ImGui::SetWindowFocus("Editor View");
		}
	}
	ImGui::EndDisabled();

	ImGui::SameLine();

	// ツールバー内容
	if (m_pEditor->pSceneManager->State == SceneManagerState::Playing) {
		if (ImGui::ImageButton("Pause", pause, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), m_DefaultButtonColor)) {
			m_pEditor->pSceneManager->State = SceneManagerState::Paused; // シーンの状態を 一時停止に変更
		}
	} else {
		if (ImGui::ImageButton("Play", play, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), m_DefaultButtonColor)) {
			m_pEditor->pSceneManager->State = SceneManagerState::Playing; // シーンの状態を再生中に変更
			ImGui::SetWindowFocus("Play View");
		}
	}
	ImGui::SameLine();
	if (ImGui::ImageButton("Step", step, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), m_DefaultButtonColor)) {
		m_pEditor->pSceneManager->State = SceneManagerState::Step; // シーンの状態を ステップに変更
	}
}

void ViewWindow::DrawRenderLayerToggleUI() {
	RenderSystem* renderSystem = m_pEditor->pSceneManager->pSystemRegistry->GetSystem<RenderSystem>();

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

