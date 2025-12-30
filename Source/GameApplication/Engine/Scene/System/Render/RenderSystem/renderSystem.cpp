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

#include "Graphics/graphicsContext.h"
#include "Graphics/mainRenderer.h"

#include "Resources/resourceService.h"
#include "Resources/Data/vertexShaderData.h"
#include "Resources/Data/pixelShaderData.h"

#include "Scene.h"
#include "SceneManager.h"

#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"

#include "System/Physic/physicSystem.h"

#include "System/Render/RenderSystem/renderLayer.h"

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
#include <Editor/UI/ViewWindow.h>

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

	PlayButtonTexture = m_context->resource->Load<TextureData> ("Asset/Texture/UI/Control/Play.png");
	PauseButtonTexture = m_context->resource->Load<TextureData>("Asset/Texture/UI/Control/Pause.png");
	StopButtonTexture = m_context->resource->Load<TextureData> ("Asset/Texture/UI/Control/Stop.png");
	StepButtonTexture = m_context->resource->Load<TextureData> ("Asset/Texture/UI/Control/Step.png");
#else
	showPlayer = nullptr;
	showEditor = nullptr;
#endif // _EDITOR


	auto m_FullScreenVS = m_context->resource->Load<VertexShaderData>("Asset\\Shader\\PostEffectVS.cso");
	auto m_FullScreenPS = m_context->resource->Load<PixelShaderData>("Asset\\Shader\\PostEffectPS.cso");

	copyShader = new PostEffectShader();
	copyShader->m_VS = m_FullScreenVS->m_VertexShader; // フルスクリーンVS
	copyShader->m_PS = m_FullScreenPS->m_PixelShader; // 単純に SRV → out を返す PS
	copyShader->m_InputLayout = m_FullScreenVS->m_VertexLayout;
}

void RenderSystem::Finalize(){

	if(copyShader){
		delete copyShader;
		copyShader = nullptr;
	}

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

void RenderSystem::EditorUpdate(float deltaTime)
{
	auto* dc = m_context->graphics->GetDeviceContext();

	for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
		auto context = scene->GetSceneContext();
		const auto& modelEntities =
			context->component->FindEntitiesWithComponent<ModelRendererComponent>();

		for(Entity entity : modelEntities){
			auto* mr =
				context->component->GetComponent<ModelRendererComponent>(entity);

			if(!mr->model){
				mr->CreateModel(context);
				continue;
			}

			if(mr->blendedAnimations.empty()){
				continue;
			}

			// ----------------------------------------
			// 1) Update skeleton (共通)
			// ----------------------------------------
			mr->model->UpdateBoneAnimation(
				mr->blendedAnimations,
				mr->animationTime
			);

			// ----------------------------------------
			// 2) Decide CPU or GPU skinning
			// ----------------------------------------
			const bool useGPUSkinning =
				mr->model->m_Bones.size() <= BONE_MAX_COUNT;

			// ========================================
			// GPU SKINNING PATH
			// ========================================
			if(useGPUSkinning){

				// Compute Shader dispatch
				mr->model->UpdateAndDispatchSkinning(m_context->graphics, mr->dynamicVertexBuffers);

				// ※ ここでは Map / Unmap / CPU_Skinning は一切しない
				// ※ 描画時に m_SkinOutputVB を VertexBuffer として使う

			}
			// ========================================
			// CPU SKINNING PATH (fallback)
			// ========================================
			else{

				for(size_t i = 0; i < mr->dynamicVertexBuffers.size(); i++){
					D3D11_MAPPED_SUBRESOURCE mapped{};
					HRESULT hr = dc->Map(
						mr->dynamicVertexBuffers[i],
						0,
						D3D11_MAP_WRITE_DISCARD,
						0,
						&mapped
					);
					if(FAILED(hr)){
						continue;
					}

					mr->model->CPU_Skinning(
						mr->model->m_DeformVertex[i],
						mr->model->AiScene->mMeshes[i],
						static_cast<VERTEX_3D*>(mapped.pData)
					);

					dc->Unmap(mr->dynamicVertexBuffers[i], 0);
				}
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
		m_context->graphics->DrawQuad(copyShader, m_PlayerPass->result);
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

	if(!PlayButtonTexture.get()->pTexture){
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

	ViewWindow* viewWindow = m_context->editor->GetUI<ViewWindow>();

	TransformComponent editorCameraTransform;
	editorCameraTransform.position = viewWindow->m_EditorCameraPosition;
	editorCameraTransform.SetRotationEuler(viewWindow->m_EditorCameraRotation);

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

	RenderPassContext renderPassContext(
		RenderPhase::PHASE_GBUFFER,
		editorRenderLayerVisible,
		cameraData,
		m_context->EditorScreenSize
	);
	m_EditorPass->Execute(renderPassContext);
}

