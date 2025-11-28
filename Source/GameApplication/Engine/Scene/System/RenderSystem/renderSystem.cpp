// Engine/Scene/System/renderSystem.cpp
#include "renderSystem.h"

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

#include "Engine/DebugTools/debugSystem.h"

#include "Engine/Resources/Data/modelData.h"

#include "Registry/entityRegistry.h"
#include "Registry/systemRegistry.h"
#include "Registry/componentRegistry.h"

#include "Engine/Graphics/mainRenderer.h"

#include "Engine/Resources/resourceService.h"
#include "Engine/Resources/Data/vertexShaderData.h"
#include "Engine/Resources/Data/pixelShaderData.h"

#include "Scene.h"
#include "SceneManager.h"

#include "System/physicSystem.h"


#include "Component/bumpMapComponent.h"
#include "Component/2DspriteRendererComponent.h"
#include "Component/RenderLayerComponent.h"
#include "Component/particleComponent.h"
#include "Component/outlineComponent.h"
#include "Component/EffectComponent.h"
#include "Component/terrainComponent.h"
#include "Component/waveComponent.h"
#include "Component/transformComponent.h"
#include "Component/textureComponent.h"
#include "Component/meshRendererComponent.h"
#include "Component/modelRendererComponent.h"
#include "Component/BillBoardRendererComponent.h"
#include "Component/cameraComponent.h"


#include "cameraEntityData.h"
#include "renderPhase.h"
#include "RenderTarget/renderTarget.h"
#include "Renderable/RenderableContext.h"
#include "Renderable/Mesh/RenderableMesh.h"
#include "Renderable/Model/RenderableModel.h"
#include "Renderable/BillBoard/RenderableBillBoard.h"
#include "Renderable/Sprite/RenderableSprite.h"
#include "Renderable/Particle/RenderableParticle.h"
#include "Renderable/Terrain/RenderableTerrain.h"
#include <Component/LightComponent.h>

constexpr int maxLineCount = 99999;



Effekseer::Matrix44 ConvertXMMATRIXToMatrix44(const DirectX::XMMATRIX& matrix){
	Effekseer::Matrix44 result;
	DirectX::XMFLOAT4X4 tempMatrix;
	DirectX::XMStoreFloat4x4(&tempMatrix, matrix);

	//行列の各成分をEffekseerの行列にコピー
	for(int row = 0; row < 4; ++row){
		for(int col = 0; col < 4; ++col){
			result.Values[row][col] = tempMatrix.m[row][col];
		}
	}
	return result;
}

RenderLayer GetRenderLayerFromEntity(Entity entity, ComponentRegistry* registry) {
	auto* layerComponent = registry->GetComponent<RenderLayerComponent>(entity);
	if (layerComponent) {
		return layerComponent->layer;
	}
	if(registry->HasComponent<SpriteRendererComponent>(entity)){
		return RenderLayer::OverlayUI;
	}
	if(registry->HasComponent<BillBoardRendererComponent>(entity)){
		return RenderLayer::Transparent3D;
	}
	if(registry->HasComponent<ParticleComponent>(entity)){
		return RenderLayer::SortTransparent3D;
	}
	auto* texture = registry->GetComponent<TextureComponent>(entity);
	if (texture && texture->Material.Diffuse.w < 1.0f) {
		return RenderLayer::SortTransparent3D;
	}
	return RenderLayer::Opaque3D;
}

void RenderSystem::Initialize(){
	m_context->debug->LOG_DEBUG("RenderSystemを初期化中...");

	m_renderables.clear();
	m_renderables.push_back(std::make_shared<RenderableModel>());
	m_renderables.push_back(std::make_shared<RenderableMesh>());
	m_renderables.push_back(std::make_shared<RenderableBillBoard>());
	m_renderables.push_back(std::make_shared<RenderableSprite>());
	m_renderables.push_back(std::make_shared<RenderableParticle>());
	m_renderables.push_back(std::make_shared<RenderableTerrain>());
	for(auto renderable : m_renderables){
		renderable->Initialize(m_context);
	}

	ID3D11Device* device = m_context->graphics->GetDevice();

	D3D11_SAMPLER_DESC desc = {};
	desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;

	device->CreateSamplerState(&desc, &shadowSampler);

	showPlayer = &m_context->imgui->GetManubar()->showPlayerView;
	showEditor = &m_context->imgui->GetManubar()->showEditorView;

	PlayButtonTexture = m_context->resource->Load<TextureData>("Asset/Texture/UI/Control/Play.png");
	PauseButtonTexture = m_context->resource->Load<TextureData>("Asset/Texture/UI/Control/Pause.png");
	StopButtonTexture = m_context->resource->Load<TextureData>("Asset/Texture/UI/Control/Stop.png");
	StepButtonTexture = m_context->resource->Load<TextureData>("Asset/Texture/UI/Control/Step.png");

	m_RenderTargetPlayer = new RenderTarget(m_context->PlayerScreenSize, m_context->graphics,RenderTargetType::RENDERTARGET_TYPE_COLOR);
	m_RenderTargetEditor = new RenderTarget(m_context->EditorScreenSize, m_context->graphics, RenderTargetType::RENDERTARGET_TYPE_COLOR);
	m_RenderTargetShadow = new RenderTarget(Vector2(SHADOWMAP_SIZE, SHADOWMAP_SIZE), m_context->graphics, RenderTargetType::RENDERTARGET_TYPE_DEPTH);

	m_ShadowPixelShader = m_context->resource->Load<PixelShaderData>("Asset\\Shader\\shadowPS.cso");
	m_LineVertexShader = m_context->resource->Load<VertexShaderData>("Asset\\Shader\\DebugLineVS.cso");
	m_LinePixelShader = m_context->resource->Load<PixelShaderData>("Asset\\Shader\\DebugLinePS.cso");


	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(VERTEX_3D) * maxLineCount * 2;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = device->CreateBuffer(&bd, nullptr, &pPhysicsDebugLineVB);
	if(FAILED(hr)){
		throw std::runtime_error("Failed to create physics debug line vertex buffer.");
	}

	auto m_FullScreenVS = m_context->resource->Load<VertexShaderData>("Asset\\Shader\\PostEffectVS.cso");
	auto m_FullScreenPS = m_context->resource->Load<PixelShaderData>("Asset\\Shader\\PostEffectPS.cso");

	copyShader.m_VS = m_FullScreenVS->m_VertexShader; // フルスクリーンVS
	copyShader.m_PS = m_FullScreenPS->m_PixelShader; // 単純に SRV → out を返す PS
	copyShader.m_InputLayout = m_FullScreenVS->m_VertexLayout;
}

void RenderSystem::Finalize(){
	pPhysicsDebugLineVB->Release();
	pPhysicsDebugLineVB = nullptr;
	shadowSampler->Release();
	shadowSampler = nullptr;

	for(auto renderable : m_renderables){
		renderable->Finalize();
	}

	delete m_RenderTargetShadow;
	delete m_RenderTargetEditor;
	delete m_RenderTargetPlayer;
}

void RenderSystem::Update(float deltaTime) {

	// コンポーネントを持つエンティティの検索
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		const auto& modelEntities = context->component->FindEntitiesWithComponent<ModelRendererComponent>();
		if (modelEntities.empty()) {
			return;
		} else {
			for (Entity entity : modelEntities) {
				auto* modelRenderer = context->component->GetComponent<ModelRendererComponent>(entity);
				modelRenderer->animationTime += deltaTime * 60.0f;
			}
		}
	}
}

void RenderSystem::Draw(){

	if(*showPlayer){
		PlayerView();
	} else{

		m_context->graphics->ResetViewport();
		m_context->graphics->GetDeviceContext()->OMSetRenderTargets(1, m_context->graphics->GetpRenderTargetView(), m_context->graphics->GetDepthStencilView());

		Vector2 ScreenSize = Vector2(
			(float)m_context->renderer->GetGraphicsContext()->m_width,
			(float)m_context->renderer->GetGraphicsContext()->m_height
		);

		m_context->PlayerScreenSize = ScreenSize;

		CameraEntityData cameraEntity = FindCameraEntity();
		if (!cameraEntity.cameraComponent) {
			return;
		}

		RenderableContext renderPassContext(
			RenderPhase::PHASE_GBUFFER,
			playerRenderLayerVisible,
			nullptr,
			nullptr,
			cameraEntity,
			ScreenSize
		);


		DirectX::XMFLOAT4 clearColor = { 0,0,0,1 };
		m_context->graphics->Clear(&clearColor.x);

		ID3D11ShaderResourceView* finalSRV = RenderSceneWithPostEffects(m_context->graphics->GetRenderTargetSRV(), renderPassContext);

		ID3D11RenderTargetView* rtv = m_context->graphics->GetRenderTargetView(); // バックバッファ
		m_context->graphics->GetDeviceContext()->OMSetRenderTargets(1, &rtv, m_context->graphics->GetDepthStencilView());
		m_context->graphics->DrawQuad(&copyShader, finalSRV);
	}
	if(*showEditor){
		EditorView();
	}

	m_context->graphics->ResetViewport();
	m_context->graphics->GetDeviceContext()->OMSetRenderTargets(1, m_context->graphics->GetpRenderTargetView(), m_context->graphics->GetDepthStencilView());

}

void RenderSystem::EditorUpdate(float deltaTime) {




	ImGuiIO& io = ImGui::GetIO();

	// マウス右クリックで操作有効
	static bool isCameraActive = false;
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		isCameraActive = true;
	} else if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		isCameraActive = false;
	}
	// 入力で動かすベクトル
	Vector3 velocity = {0,0,0};
	float speed = 20.0f;

	if(isCameraActive){
		// ------ 1. マウス移動で回転 ------
		float mouseSensitivity = 0.005f;
		m_EditorCameraRotation.y += io.MouseDelta.x * mouseSensitivity; // yaw
		m_EditorCameraRotation.x += io.MouseDelta.y * mouseSensitivity; // pitch

		// ピッチ制限
		const float pitchLimit = DirectX::XM_PIDIV2 - 0.01f;
		if(m_EditorCameraRotation.x > pitchLimit) m_EditorCameraRotation.x = pitchLimit;
		if(m_EditorCameraRotation.x < -pitchLimit) m_EditorCameraRotation.x = -pitchLimit;

		// 回転から方向ベクトル取得
		TransformComponent transform;
		transform.position = m_EditorCameraPosition;
		transform.SetRotationEuler(m_EditorCameraRotation);
		Vector3 front = transform.front();
		Vector3 right = transform.right();
		Vector3 up = Vector3(0,1,0);

		if(ImGui::IsKeyDown(ImGuiKey_W)) velocity += front;
		if(ImGui::IsKeyDown(ImGuiKey_S)) velocity -= front;
		if(ImGui::IsKeyDown(ImGuiKey_A)) velocity -= right;
		if(ImGui::IsKeyDown(ImGuiKey_D)) velocity += right;
		if(ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_LeftShift)) velocity -= up;
		if(ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_Space)) velocity += up;
		if(velocity.length() > 0.0f){
			m_EditorCameraPosition += velocity.normalize() * speed * deltaTime;
		}
	} else if(mouseOnEditor){

		TransformComponent transform;
		transform.position = m_EditorCameraPosition;
		transform.SetRotationEuler(m_EditorCameraRotation);

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

void RenderSystem::ShadowPass(RenderableContext renderPassContext){

	//return;

	RenderableContext newContext = renderPassContext;
	GraphicsContext* graphicsContext = m_context->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
	deviceContext->PSSetShaderResources(2, 1, nullSRV); // t2 をクリア（あなたの slot に合わせる）

	// シャドウマップレンダーターゲットに切り替え
	if (m_RenderTargetShadow->type == RenderTargetType::RENDERTARGET_TYPE_DEPTH) {

		deviceContext->ClearDepthStencilView(m_RenderTargetShadow->dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		deviceContext->OMSetRenderTargets(0, nullptr, m_RenderTargetShadow->dsv.Get());

	} else {
		float color[4] = { 1.0f,1.0f,1.0f,1.0f };
		deviceContext->ClearRenderTargetView(m_RenderTargetShadow->rtv.Get(), color);
		deviceContext->ClearDepthStencilView(m_RenderTargetShadow->dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		deviceContext->OMSetRenderTargets(1, m_RenderTargetShadow->rtv.GetAddressOf(), m_RenderTargetShadow->dsv.Get());
	}
	// シャドウマップ用カメラ
	LIGHT light = m_context->renderer->GetGraphicsContext()->GetLight()[0];
	//newContext.cameraPosition = DirectX::XMFLOAT4(light.Position.x, light.Position.y, light.Position.z, 0.0f);
	newContext.passPhase = RenderPhase::PHASE_SHADOW;
	newContext.screenSize = Vector2(SHADOWMAP_SIZE, SHADOWMAP_SIZE);

	for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
		auto context = scene->GetSceneContext();
		// ライトコンポーネントを持つエンティティの検索
		const auto& lightEntities = context->component->FindEntitiesWithComponent<LightComponent>();
		if(lightEntities.empty()){
			continue;
		}
		for(Entity entity : lightEntities){
			LightComponent* light = context->component->GetComponent<LightComponent>(entity);
			if(!light){
				continue;
			}
			TransformComponent* transform = context->component->GetComponent<TransformComponent>(entity);
			if(transform){
				newContext.cameraPosition = DirectX::XMFLOAT4(light->light.Position.x, light->light.Position.y, light->light.Position.z, 0.0f);

				Vector3 front = transform->front();
				Vector3 up = transform->up();
				newContext.viewMatrix = DirectX::XMMatrixLookAtLH(
					transform->position.ToXMVECTOR(),
					(transform->position + front * 100.0f).ToXMVECTOR(),
					(up).ToXMVECTOR()
				);
				newContext.projectionMatrix = DirectX::XMMatrixOrthographicLH(
					100.0f,
					100.0f,
					0.01f,
					100.0f
				);
				break;
				break;
			}
		}
	}
	newContext.pixelShader = m_ShadowPixelShader;

	DrawEntities(newContext);

	// 元のレンダーターゲットに戻す
	deviceContext->OMSetRenderTargets(1, graphicsContext->GetpRenderTargetView(), graphicsContext->GetDepthStencilView());
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



	RenderableContext renderPassContext(
		RenderPhase::PHASE_GBUFFER,
		playerRenderLayerVisible,
		nullptr,
		nullptr,
		cameraData,
		m_RenderTargetPlayer->size
	);
	ShadowPass(renderPassContext);

	float clearCol[4] = {0.0f, 1.0f, 0.0f, 1.0f};
	m_RenderTargetPlayer->Resize(Vector2(avail.x, avail.y), m_context->graphics);
	m_RenderTargetPlayer->Clear(m_context->graphics->GetDeviceContext(), clearCol);
	m_context->graphics->GetDeviceContext()->OMSetRenderTargets(1, m_RenderTargetPlayer->rtv.GetAddressOf(), m_RenderTargetPlayer->dsv.Get());

	m_context->graphics->GetDeviceContext()->PSSetShaderResources(2, 1, m_RenderTargetShadow->srv.GetAddressOf());
	m_context->graphics->GetDeviceContext()->PSSetSamplers(1, 1, &shadowSampler);

	ID3D11ShaderResourceView* finalSRV = RenderSceneWithPostEffects(m_RenderTargetPlayer->srv.Get(), renderPassContext);
	if(!finalSRV){
		ImGui::Text("finalSRV is NULL");
	}
	ImGui::Image((ImTextureID)finalSRV, avail, ImVec2(0, 0), ImVec2(1, 1), ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 0.0f));
	ImGui::End();

	// バックバッファへ戻す
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
	m_RenderTargetEditor->Resize(Vector2(avail.x, avail.y),m_context->graphics);

	RenderableContext renderPassContext(
		RenderPhase::PHASE_GBUFFER,
		editorRenderLayerVisible,
		nullptr,
		nullptr,
		cameraData,
		m_RenderTargetEditor->size
	);
	ShadowPass(renderPassContext);


	float clearCol[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
	deviceContext->ClearRenderTargetView(m_RenderTargetEditor->rtv.Get(), clearCol);
	deviceContext->ClearDepthStencilView(m_RenderTargetEditor->dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	graphicsContext->GetDeviceContext()->OMSetRenderTargets(1, m_RenderTargetEditor->rtv.GetAddressOf(), m_RenderTargetEditor->dsv.Get());

	 //シャドウマップをピクセルシェーダーの t2 にセット
	deviceContext->PSSetShaderResources(2, 1, m_RenderTargetShadow->srv.GetAddressOf());
	deviceContext->PSSetSamplers(1, 1, &shadowSampler);
	DrawEntities(renderPassContext);

	ImVec2 cursor = ImGui::GetCursorPos();
	ImGui::Image((ImTextureID)m_RenderTargetEditor->srv.Get(), avail, ImVec2(0, 0), ImVec2(1, 1));

	// ImGuizmo の描画領域を設定
	ImGuizmo::SetRect(
		ImGui::GetWindowPos().x + cursor.x,
		ImGui::GetWindowPos().y + cursor.y,
		avail.x,
		avail.y
	);
	ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

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

	//ImGui::Begin("Shadow Map View", nullptr, toolbar_window_flags);
	//avail = ImGui::GetContentRegionAvail(); // ウィンドウ内の利用可能サイズ
	//float availMin;

	//if(avail.x < avail.y){
	//	availMin = avail.x;
	//} else{
	//	availMin = avail.y;
	//}
	//ImGui::Image((ImTextureID)m_RenderTargetShadow->srv.Get(), ImVec2(availMin, availMin), ImVec2(0, 0), ImVec2(1, 1));
	//ImGui::End();

	//graphicsContext->GetDeviceContext()->OMSetRenderTargets(1, graphicsContext->GetpRenderTargetView(), graphicsContext->GetDepthStencilView());
}


std::vector<int> TopologicalSortPostEffects(CameraComponent* camera){
	std::vector<int> sortedIndices;
	if(!camera) return sortedIndices;

	int n = static_cast<int>(camera->postEffects.size());
	int INPUT_NODE = n;     // -1 の代わり
	int OUTPUT_NODE = n + 1;   // -2 の代わり

	std::unordered_map<int, std::vector<int>> adj;
	std::unordered_map<int, int> indegree;
	std::unordered_set<int> nodes;

	for(const auto& link : camera->postEffectLinks){
		int start = link.startNode;
		int end = link.endNode;

		if(start == -1) start = INPUT_NODE;
		if(end == -2) end = OUTPUT_NODE;

		if(start >= 0 && start <= OUTPUT_NODE && end >= 0 && end <= OUTPUT_NODE){
			adj[start].push_back(end);
			indegree[end]++;
			nodes.insert(start);
			nodes.insert(end);
		}
	}

	std::queue<int> q;
	for(int node : nodes){
		if(indegree.find(node) == indegree.end()) q.push(node);
	}

	while(!q.empty()){
		int node = q.front(); q.pop();
		sortedIndices.push_back(node);

		for(int neighbor : adj[node]){
			indegree[neighbor]--;
			if(indegree[neighbor] == 0) q.push(neighbor);
		}
	}

	// サイクル検出
	if(sortedIndices.size() != nodes.size()){
		OutputDebugStringA("Warning: post effect graph has cycles!\n");
		for(int node : nodes){
			if(std::find(sortedIndices.begin(), sortedIndices.end(), node) == sortedIndices.end()){
				sortedIndices.push_back(node);
			}
		}
	}

	// 仮IDを元に戻す
	for(auto& idx : sortedIndices){
		if(idx == INPUT_NODE) idx = -1;
		if(idx == OUTPUT_NODE) idx = -2;
	}

	return sortedIndices;
}

ID3D11ShaderResourceView* RenderSystem::RenderSceneWithPostEffects(ID3D11ShaderResourceView* initialSRV, const RenderableContext& renderPassContext) {
	GraphicsContext* graphics = m_context->graphics;

	DrawEntities(renderPassContext);

	std::vector<PostProcessNode> postNodes;
	std::unordered_map<int, int> effectIndexToPostNodeIndex; // camera->postEffects idx → postNodes idx

	DirectX::XMFLOAT4 clearColor = { 0,0,0,1 };

	CameraComponent* camera = renderPassContext.cameraData.cameraComponent;

	if (camera) {
		auto sortedIndices = TopologicalSortPostEffects(camera);

		for (int idx : sortedIndices) {
			if(idx < 0) continue; // -1/-2 は描画対象としてノード作らない

			auto& e = camera->postEffects[idx];
			if (!e.enabled || !e.ps || !e.vs) continue;

			PostProcessNode node{};
			node.id = idx;
			node.shader.m_VS = e.vs->m_VertexShader;
			node.shader.m_PS = e.ps->m_PixelShader;
			node.shader.m_InputLayout = e.vs->m_VertexLayout;
			node.param = e.Param;

			e.ResizeTexture(graphics->GetDevice(), renderPassContext.screenSize);
			e.Clear(graphics->GetDeviceContext(), &clearColor.x);
			node.rtv = e.rtv.GetAddressOf();
			node.srv = e.srv.Get();
			node.tex = e.tex.Get();

			// リンクから入力ノードを決定
			node.inputs.clear();
			// ノード追加前にインデックスを記録しておく
			int postNodeIndex = static_cast<int>(postNodes.size());
			effectIndexToPostNodeIndex[idx] = postNodeIndex;

			postNodes.push_back(std::move(node));
		}

		// リンクを後から追加（マッピングが揃った後に行う）
		for (auto& node : postNodes) {
			int effectIdx = node.id;
			for (auto& link : camera->postEffectLinks) {
				if (link.endNode == effectIdx) {
					if (link.startNode < 0) {
						node.inputs.push_back(-2); // 初期SRV
					} else {
						auto it = effectIndexToPostNodeIndex.find(link.startNode);
						if (it != effectIndexToPostNodeIndex.end()) {
							node.inputs.push_back(it->second);
						}
						// else: 無効な startNode は無視（スキップされたノードの可能性あり）
					}
				}
			}
		}
	}

	// 3. ApplyPostProcessChain 側で -1 を初期SRVに置き換えるようにする（念のための処理）
	if (!postNodes.empty()) {
		for (auto& node : postNodes) {
			for (size_t i = 0; i < node.inputs.size(); ++i) {
				if (node.inputs[i] == -1) {
					node.inputs[i] = -2; // 特殊値 -2 を初期SRV扱いとして ApplyPostProcessChain で処理
				}
			}
		}

		graphics->GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);
		graphics->ApplyPostProcessChain(postNodes, initialSRV);
		graphics->GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);

		return graphics->m_CurrentSRV;
	}

	return initialSRV;
}



void RenderSystem::DrawEntities(const RenderableContext& renderPassContext){

	bool* pRenderLayer = renderPassContext.renderLayerVisibility;

	// コンテキストの取得
	GraphicsContext* graphicsContext = m_context->renderer->GetGraphicsContext();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	CAMERA camera{};
	camera.CameraPosition = renderPassContext.cameraPosition;
	graphicsContext->SetCamera(camera);

	graphicsContext->SetViewMatrix(renderPassContext.viewMatrix);
	graphicsContext->SetProjectionMatrix(renderPassContext.projectionMatrix);

	D3D11_VIEWPORT vp = {};
	vp.Width = renderPassContext.screenSize.x;
	vp.Height = renderPassContext.screenSize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	deviceContext->RSSetViewports(1, &vp);

	m_context->imgui->SetViewProjectionMatrix(renderPassContext.viewMatrix, renderPassContext.projectionMatrix);

	for (int i = 0; i < (int)RenderLayer::MaxRenderLayer; i++) {

		if(renderPassContext.renderLayerVisibility[i] == false){
			continue;
		}

		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();

			// コンポーネントを持つエンティティの検索
			std::vector<Entity> entities = context->component->FindEntitiesWithComponent<TransformComponent>();

			if (entities.empty()) {

				continue;

			} else {

				for (Entity entity : entities) {

					RenderLayer layer = GetRenderLayerFromEntity(entity, context->component);

					if ((int)layer != i) {
						continue;
					}

					TransformComponent* transform = context->component->GetComponent<TransformComponent>(entity);
					if (!transform) {
						continue;
					}
					OutlineComponent* outline = context->component->GetComponent<OutlineComponent>(entity);
					BumpMapComponent* bumpMap = context->component->GetComponent<BumpMapComponent>(entity);
					if (bumpMap) {
						// バンプマップの設定
						if (bumpMap->m_TextureData) {
							deviceContext->PSSetShaderResources(1, 1, bumpMap->m_TextureData->pTexture.GetAddressOf());
						} else {
							ID3D11ShaderResourceView* nullSRV = nullptr;
							deviceContext->PSSetShaderResources(1, 1, &nullSRV);
						}
					} else {
						ID3D11ShaderResourceView* nullSRV = nullptr;
						deviceContext->PSSetShaderResources(1, 1, &nullSRV);
					}

					TextureComponent* texture = context->component->GetComponent<TextureComponent>(entity);
					if (texture) {
						// マテリアル設定
						MATERIAL material = texture->Material;
						material.DiffuseTextureEnable = ((bool)texture->m_TextureData);
						if (texture->m_TextureData) {
							deviceContext->PSSetShaderResources(0, 1, texture->m_TextureData->pTexture.GetAddressOf());
						}

						graphicsContext->SetMaterial(material);

						UVMatrix uv;
						if (texture->UV_Slice_X != 0 && texture->UV_Slice_Y != 0) {
							uv.Start.x = (float)(texture->AnimationNum % texture->UV_Slice_X) * 1.0f / (float)texture->UV_Slice_X;
							uv.Start.y = (float)(texture->AnimationNum / texture->UV_Slice_X) * 1.0f / (float)texture->UV_Slice_Y;

							uv.End.x = (float)uv.Start.x + 1.0f / (float)texture->UV_Slice_X;
							uv.End.y = (float)uv.Start.y + 1.0f / (float)texture->UV_Slice_Y;
						}
						graphicsContext->SetUVMatrix(uv);

					} else {
						// マテリアル設定
						MATERIAL material;
						material.DiffuseTextureEnable = false;
						material.Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
						graphicsContext->SetMaterial(material);

						UVMatrix uv;
						graphicsContext->SetUVMatrix(uv);
					}

					for(auto renderable : m_renderables){
						renderable->Execute(renderPassContext, context ,entity);
					}


					
				}
			}
		}
	}
	//Effekseer
	Effekseer::Matrix44 effekseerProjectionMatrix = ConvertXMMATRIXToMatrix44(renderPassContext.projectionMatrix);
	Effekseer::Matrix44 effekseerViewMatrix = ConvertXMMATRIXToMatrix44(renderPassContext.viewMatrix);

	m_context->graphics->GetEffectRenderer()->SetProjectionMatrix(effekseerProjectionMatrix);
	m_context->graphics->GetEffectRenderer()->SetCameraMatrix(effekseerViewMatrix);

	m_context->graphics->GetEffectRenderer()->BeginRendering();
	m_context->graphics->GetEffectManager()->Draw();
	m_context->graphics->GetEffectRenderer()->EndRendering();

	//PhysX
	if(pRenderLayer[(int)RenderLayer::Debug] && renderPassContext.passPhase != RenderPhase::PHASE_SHADOW){
		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();
			auto physics = m_context->systemRegistry->GetSystem<PhysicSystem>();
			const physx::PxRenderBuffer& rb = physics->GetRenderBuffer();

			// 色変換関数
			auto ConvertColor = [](physx::PxU32 c) {
				float a = ((c >> 24) & 0xFF) / 255.0f;
				float r = ((c >> 16) & 0xFF) / 255.0f;
				float g = ((c >> 8) & 0xFF) / 255.0f;
				float b = ((c >> 0) & 0xFF) / 255.0f;
				return DirectX::XMFLOAT4(r, g, b, a);
			};

			std::vector<VERTEX_3D> vertices;
			for (physx::PxU32 i = 0; i < rb.getNbLines(); i++) {
				const physx::PxDebugLine& line = rb.getLines()[i];

				VERTEX_3D v0;
				v0.Position = DirectX::XMFLOAT3(line.pos0.x, line.pos0.y, line.pos0.z);
				v0.Diffuse = ConvertColor(line.color0);

				VERTEX_3D v1;
				v1.Position = DirectX::XMFLOAT3(line.pos1.x, line.pos1.y, line.pos1.z);
				v1.Diffuse = ConvertColor(line.color1);

				vertices.push_back(v0);
				vertices.push_back(v1);
			}

			if (vertices.empty() || vertices.size() >= maxLineCount) continue;

			ID3D11Device* device = graphicsContext->GetDevice();
			ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

			// 頂点バッファ更新
			D3D11_MAPPED_SUBRESOURCE mapped;
			deviceContext->Map(pPhysicsDebugLineVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
			memcpy(mapped.pData, vertices.data(), sizeof(VERTEX_3D) * vertices.size());
			deviceContext->Unmap(pPhysicsDebugLineVB, 0);

			graphicsContext->SetWorldMatrix(DirectX::XMMatrixIdentity());

			UINT stride = sizeof(VERTEX_3D);
			UINT offset = 0;
			deviceContext->IASetVertexBuffers(0, 1, &pPhysicsDebugLineVB, &stride, &offset);
			deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

			// シェーダーをセット（通常のカラー付き頂点用のものを使用）
			deviceContext->VSSetShader(m_LineVertexShader->m_VertexShader.Get(), nullptr, 0);
			deviceContext->PSSetShader(m_LinePixelShader->m_PixelShader.Get(), nullptr, 0);

			// 描画
			deviceContext->Draw(static_cast<UINT>(vertices.size()), 0);
		}
	}
}

