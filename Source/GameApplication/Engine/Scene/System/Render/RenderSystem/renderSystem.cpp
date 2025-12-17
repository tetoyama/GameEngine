// Scene/System/renderSystem.cpp
#include "renderSystem.h"
#include "buildSetting.h"

#include <algorithm>
#include <filesystem>
#include <queue>

#include <DirectXMath.h>

#include "Backends/DirectX11/DirectXTex.h"
#include "Backends/ImGui/ImGui.h"
#include "Backends/ImGui/ImGuizmo.h"
#include "Backends/myVector3.h"

#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "Backends/Assimp/cimport.h"
#include "Backends/Assimp/postprocess.h"
#include "Backends/Assimp/matrix4x4.h"

#include "Backends/myMath.h"

#include "DebugTools/debugSystem.h"

#include "Resources/Data/modelData.h"

#include "Registry/entityRegistry.h"
#include "Registry/systemRegistry.h"
#include "Registry/componentRegistry.h"

#include "Graphics/mainRenderer.h"

#include "Resources/resourceService.h"
#include "Resources/Data/vertexShaderData.h"
#include "Resources/Data/pixelShaderData.h"

#include "Scene.h"
#include "SceneManager.h"

#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"

#include "System/Physic/physicSystem.h"

#include "Component/RenderLayerComponent.h"
#include "Component/transformComponent.h"
#include "Component/cameraComponent.h"
#include <Component/modelRendererComponent.h>
#include <Component/LightComponent.h>

#include "cameraEntityData.h"
#include "renderPhase.h"
#include "RenderTarget/renderTarget.h"
#include "Renderable/Mesh/RenderableMesh.h"
#include "Renderable/Model/RenderableModel.h"
#include "Renderable/BillBoard/RenderableBillBoard.h"
#include "Renderable/Sprite/RenderableSprite.h"
#include "Renderable/Particle/RenderableParticle.h"
#include "Renderable/Terrain/RenderableTerrain.h"
#include "RenderPass/IRenderPass.h"

#include "RenderPass/GBuffer/GBufferPass.h"
#include "RenderPass/ShadowMap/ShadowMapPass.h"
#include "RenderPass/PlayerView/PlayerPass.h"
#include "RenderPass/EditorView/EditorPass.h"
#include "Renderable/Wave/RenderableWave.h"

void RenderSystem::Initialize(){
	m_context->debug->LOG_DEBUG("RenderSystemを初期化中...");

	m_renderables.clear();
	m_renderables.push_back(std::make_shared<RenderableModel>());
	m_renderables.push_back(std::make_shared<RenderableMesh>());
	m_renderables.push_back(std::make_shared<RenderableBillBoard>());
	m_renderables.push_back(std::make_shared<RenderableSprite>());
	m_renderables.push_back(std::make_shared<RenderableParticle>());
	m_renderables.push_back(std::make_shared<RenderableTerrain>());
	m_renderables.push_back(std::make_shared<RenderableWave>());

	for(auto renderable : m_renderables){
		renderable->Initialize(m_context);
	}

	m_PlayerPass = new PlayerPass();
	m_PlayerPass->Initialize(this, m_context);


	m_EditorPass = new EditorPass();
	m_EditorPass->Initialize(this, m_context);

#ifdef _EDITOR
	showPlayer = &m_context->editor->GetUI<MenuBar>()->showPlayerView;
	showEditor = &m_context->editor->GetUI<MenuBar>()->showEditorView;

	PlayButtonTexture = m_context->resource->Load<TextureData>("Asset/Texture/UI/Control/Play.png");
	PauseButtonTexture = m_context->resource->Load<TextureData>("Asset/Texture/UI/Control/Pause.png");
	StopButtonTexture = m_context->resource->Load<TextureData>("Asset/Texture/UI/Control/Stop.png");
	StepButtonTexture = m_context->resource->Load<TextureData>("Asset/Texture/UI/Control/Step.png");
#else
	showPlayer = nullptr;
	showEditor = nullptr;
#endif // _EDITOR


	auto m_FullScreenVS = m_context->resource->Load<VertexShaderData>("Asset\\Shader\\PostEffectVS.cso");
	auto m_FullScreenPS = m_context->resource->Load<PixelShaderData>("Asset\\Shader\\PostEffectPS.cso");

	copyShader.m_VS = m_FullScreenVS->m_VertexShader; // フルスクリーンVS
	copyShader.m_PS = m_FullScreenPS->m_PixelShader; // 単純に SRV → out を返す PS
	copyShader.m_InputLayout = m_FullScreenVS->m_VertexLayout;
}

void RenderSystem::Finalize(){

	m_EditorPass->Finalize();
	delete m_EditorPass;
	m_EditorPass = nullptr;

	m_PlayerPass->Finalize();
	delete m_PlayerPass;
	m_PlayerPass = nullptr;

	for(auto renderable : m_renderables){
		renderable->Finalize();
	}
	m_renderables.clear();
}

void RenderSystem::Update(float deltaTime) {

	// コンポーネントを持つエンティティの検索
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		const auto& modelEntities = context->component->FindEntitiesWithComponent<ModelRendererComponent>();
		if (modelEntities.empty()) {
			continue;
		} else {
			for (Entity entity : modelEntities) {
				auto* modelRenderer = context->component->GetComponent<ModelRendererComponent>(entity);
				modelRenderer->animationTime += deltaTime * 60.0f;
			}
		}
	}
}

void RenderSystem::EditorUpdate(float deltaTime) {

	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		const auto& modelEntities = context->component->FindEntitiesWithComponent<ModelRendererComponent>();
		if (modelEntities.empty()) {
			continue;
		} else {
			for (Entity entity : modelEntities) {
				auto* modelRenderer = context->component->GetComponent<ModelRendererComponent>(entity);
				if (modelRenderer->model) {
					modelRenderer->model->Update(modelRenderer->animationTime, m_context->graphics);
				}
			}
		}
	}

	if (mouseOnEditor) {
		ImGuiIO& io = ImGui::GetIO();
		// マウス右クリックで操作有効
		static bool isCameraActive = false;
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			isCameraActive = true;
		} else if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
			isCameraActive = false;
		}
		// 入力で動かすベクトル
		Vector3 velocity = { 0,0,0 };
		float speed = 1.0f;
		if (isCameraActive) {
			// ------ 1. マウス移動で回転 ------
			float mouseSensitivity = 0.005f;
			m_EditorCameraRotation.y += io.MouseDelta.x * mouseSensitivity; // yaw
			m_EditorCameraRotation.x += io.MouseDelta.y * mouseSensitivity; // pitch
			// ピッチ制限
			const float pitchLimit = DirectX::XM_PIDIV2 - 0.01f;
			if (m_EditorCameraRotation.x > pitchLimit) m_EditorCameraRotation.x = pitchLimit;
			if (m_EditorCameraRotation.x < -pitchLimit) m_EditorCameraRotation.x = -pitchLimit;
			// 回転から方向ベクトル取得
			TransformComponent transform;
			transform.position = m_EditorCameraPosition;
			transform.SetRotationEuler(m_EditorCameraRotation);
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
				m_EditorCameraPosition += velocity.normalize() * speed * deltaTime;
			}
		} else {
			TransformComponent transform;
			transform.position = m_EditorCameraPosition;
			transform.SetRotationEuler(m_EditorCameraRotation);
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
}


void RenderSystem::Draw(){

	if(showEditor && *showEditor){
		EditorView();
	}
	if(showPlayer && *showPlayer){

		PlayerView();

	} else{

		RenderPassContext renderPassContext(
			RenderPhase::PHASE_GBUFFER,
			playerRenderLayerVisible,
			FindCameraEntity(),
			Vector2(
				(float)m_context->renderer->GetGraphicsContext()->m_width,
				(float)m_context->renderer->GetGraphicsContext()->m_height
			)
		);
		m_PlayerPass->Execute(renderPassContext);
		if (!m_PlayerPass->result) {
			return;
		}

		float clearColor[4] = { 1.0f,1.0f,1.0f,1.0f };
		m_context->graphics->ResetViewport();
		m_context->graphics->Clear(clearColor);
		m_context->graphics->GetDeviceContext()->OMSetRenderTargets(1, m_context->graphics->GetpRenderTargetView(), m_context->graphics->GetDepthStencilView());
		m_context->graphics->DrawQuad(&copyShader, m_PlayerPass->result);
	}
	m_context->graphics->ResetViewport();
	m_context->graphics->GetDeviceContext()->OMSetRenderTargets(1, m_context->graphics->GetpRenderTargetView(), m_context->graphics->GetDepthStencilView());
}

void RenderSystem::DrawRenderLayerToggleUI() {

	ImGui::SameLine();

	std::string previewText;
	for (int i = 0; i < (int)RenderLayer::MaxRenderLayer; ++i) {
		if (editorRenderLayerVisible[i]) {
			if (!previewText.empty()) previewText += ", ";
			previewText += GetRenderLayerName(static_cast<RenderLayer>(i));
		}
	}
	if (previewText.empty()) previewText = "None";

	if (ImGui::BeginCombo("##Visible Layers", previewText.c_str())) {
		for (int i = 0; i < (int)RenderLayer::MaxRenderLayer; ++i) {
			ImGui::Selectable(
				GetRenderLayerName(static_cast<RenderLayer>(i)),
				&editorRenderLayerVisible[i],
				ImGuiSelectableFlags_DontClosePopups
			);
		}
		ImGui::EndCombo();
	}
}

const CameraEntityData RenderSystem::FindCameraEntity() {

	CameraEntityData cameraData{};
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		// カメラを取得
		auto entities = context->component->FindEntitiesWithComponent<CameraComponent>();
		if (entities.empty()) {
			continue;
		}
		cameraData.sceneContext = context;
		cameraData.entity = entities[0];
		cameraData.cameraComponent = context->component->GetComponent<CameraComponent>(cameraData.entity);
		cameraData.transformComponent = context->component->GetComponent<TransformComponent>(cameraData.entity);
		return cameraData;
	}
	return cameraData;
}

void RenderSystem::ControllButton(){

	if(!PlayButtonTexture){
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
	if(m_context->sceneManager->State == SceneManagerState::Stopped){
		StopButtonColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
	}
	ImGui::BeginDisabled(m_context->sceneManager->State == SceneManagerState::Stopped);

	if(ImGui::ImageButton("Stop", Stop, ImVec2(20, 20),ImVec2(0,0),ImVec2(1,1),ImVec4(0,0,0,0), StopButtonColor)){
		if(m_context->sceneManager->State != SceneManagerState::Stopped){

			m_context->sceneManager->State = SceneManagerState::Stopped; // シーンの状態をエディタに戻す
			ImGui::SetWindowFocus("Editor View");
		}
	}
	ImGui::EndDisabled();

	ImGui::SameLine();

	// ツールバー内容
	if(m_context->sceneManager->State == SceneManagerState::Playing){
		if(ImGui::ImageButton("Pause", Pause, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), DefaultButtonColor)){
			m_context->sceneManager->State = SceneManagerState::Paused; // シーンの状態を 一時停止に変更
		}
	} else{
		if(ImGui::ImageButton("Play", Play, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), DefaultButtonColor)){
			m_context->sceneManager->State = SceneManagerState::Playing; // シーンの状態を再生中に変更
			ImGui::SetWindowFocus("Play View");
		}
	}
	ImGui::SameLine();
	if(ImGui::ImageButton("Step", Step, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), DefaultButtonColor)){
		m_context->sceneManager->State = SceneManagerState::Step; // シーンの状態を ステップに変更
	}
}

void RenderSystem::PlayerView(){


	ImGui::Begin("Play View", showPlayer, 0);

	// 共通UI（元のControllButtonやSeparatorなど）
	ControllButton();
	ImGui::Separator();

	// カメラコンポーネントを持つエンティティ取得
	const CameraEntityData& cameraData = FindCameraEntity();
	if(!cameraData.cameraComponent){
		ImGui::Text("No Camera Component found.");
		ImGui::End();
		return;
	}
	// 利用可能な領域サイズを取得
	ImVec2 avail = ImGui::GetContentRegionAvail();

	RenderPassContext renderPassContext(
		RenderPhase::PHASE_GBUFFER,
		playerRenderLayerVisible,
		cameraData,
		Vector2(avail.x, avail.y)
	);
	m_PlayerPass->Execute(renderPassContext);

	ImGui::Image((ImTextureRef)m_PlayerPass->result, avail);
	ImGui::End();

	m_context->graphics->GetDeviceContext()->OMSetRenderTargets(
		1, m_context->graphics->GetpRenderTargetView(), m_context->graphics->GetDepthStencilView()
	);
}
void RenderSystem::EditorView(){

	GraphicsContext* graphicsContext = m_context->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();
	ImGuiWindowFlags toolbar_window_flags = 0;
	//toolbar_window_flags |= ImGuiWindowFlags_NoCollapse;
	ImGui::Begin("Editor View",showEditor, toolbar_window_flags);

	ControllButton();
	DrawRenderLayerToggleUI();

	ImGui::Separator();

	ImVec2 avail = ImGui::GetContentRegionAvail(); // ウィンドウ内の利用可能サイズ
	float availAspect = avail.x / avail.y;

	TransformComponent editorCameraTransform;
	editorCameraTransform.position = m_EditorCameraPosition;
	editorCameraTransform.SetRotationEuler(m_EditorCameraRotation);

	CameraComponent editorCamera;
	editorCamera.FOV = DirectX::XM_PIDIV4;
	editorCamera.NearClip = 0.01f;
	editorCamera.FarClip = 1000.0f;
	editorCamera.isLock = false;
	editorCamera.viewMatrix = DirectX::XMMatrixLookAtLH(
		editorCameraTransform.position.ToXMVECTOR(),
		(editorCameraTransform.position + editorCameraTransform.front()).ToXMVECTOR(),
		{ 0.0f, 1.0f, 0.0f }
	);
	CameraEntityData cameraData;
	cameraData.entity = 0;
	cameraData.sceneContext = nullptr;
	cameraData.cameraComponent = &editorCamera;
	cameraData.transformComponent = &editorCameraTransform;

	m_context->EditorScreenSize = Vector2(avail.x, avail.y);

	RenderPassContext renderPassContext(
		RenderPhase::PHASE_GBUFFER,
		editorRenderLayerVisible,
		cameraData,
		Vector2(avail.x, avail.y)
	);
	ImVec2 cursor = ImGui::GetCursorPos();

	// ImGuizmo の描画領域を設定
	ImGuizmo::SetRect(
		ImGui::GetWindowPos().x + cursor.x,
		ImGui::GetWindowPos().y + cursor.y,
		avail.x,
		avail.y
	);
	ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

	m_EditorPass->Execute(renderPassContext);
	ImGui::Image((ImTextureRef)m_EditorPass->result, avail);

	// ImGuiIO を取得
	ImGuiIO& io = ImGui::GetIO();

	// マウスホイールの値をリセット
	m_MouseWheel = 0.0f;
	mouseOnEditor = false;

	// マウスカーソルの位置を取得
	POINT cursorPos;
	if(GetCursorPos(&cursorPos)){
		// 現在のウィンドウのハンドルを取得
		HWND hwnd = GetActiveWindow();
		if(hwnd){
			// スクリーン座標をウィンドウのクライアント座標に変換
			ScreenToClient(hwnd, &cursorPos);

			// 現在のウィンドウの描画リストを取得
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 mousePos = io.MousePos;

			// マウスカーソルがウィンドウ内にあるかを判定
			if(drawList->GetClipRectMin().x <= mousePos.x && mousePos.x <= drawList->GetClipRectMax().x &&
			   drawList->GetClipRectMin().y <= mousePos.y && mousePos.y <= drawList->GetClipRectMax().y){

				mouseOnEditor = true;

				// マウスホイールの入力を取得
				m_MouseWheel = io.MouseWheel;
			}
		}
	}

	ImGui::End();

	graphicsContext->GetDeviceContext()->OMSetRenderTargets(1, graphicsContext->GetpRenderTargetView(), graphicsContext->GetDepthStencilView());
}

