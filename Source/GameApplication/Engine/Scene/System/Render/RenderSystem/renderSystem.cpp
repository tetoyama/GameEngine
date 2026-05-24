// =======================================================================
// 
// renderSystem.cpp
// 
// =======================================================================
#include "renderSystem.h"
#include "buildSetting.h"

#include <algorithm>
#include <filesystem>
#include <queue>
#include <thread>

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
#include "Resources/Data/textureData.h"

#include "Scene.h"
#include "SceneManager.h"

#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"

#include "System/Physic/physicSystem.h"

#include "System/Render/RenderSystem/renderLayer.h"

#include "Component/RenderLayerComponent.h"
#include "Component/transformComponent.h"
#include "Component/CameraComponent.h"
#include <Component/modelRendererComponent.h>
#include <Component/LightComponent.h>
#include <Component/textureComponent.h>
#include <Component/environmentMapComponent.h>

#include "CameraEntityData.h"
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
#include "RenderPass/LightingPass/LightingPass.h"
#include "RenderPass/PlayerView/PlayerPass.h"
#include "RenderPass/EditorView/EditorPass.h"
#include "Renderable/Wave/RenderableWave.h"
#include <Editor/UI/ViewWindow.h>
#include "Renderable/Effect/RenderableEffect.h"

#include "Service/Config/configSystem.h"
#include "Service/Config/appConfig.h"
#include "Backends/ImGuiFunc.h"
#include "Editor/Command/CommandManager.h"
#include "Editor/Command/PropertyChangeCommand.h"

void RenderSystem::Initialize(){
	m_pContext->pDebug->LOG_DEBUG("RenderSystemを初期化中...");

	m_renderables.clear();
	m_renderables.push_back(std::make_shared<RenderableModel>());
	m_renderables.push_back(std::make_shared<RenderableMesh>());
	m_renderables.push_back(std::make_shared<RenderableBillBoard>());
	m_renderables.push_back(std::make_shared<RenderableSprite>());
	m_renderables.push_back(std::make_shared<RenderableParticle>());
	m_renderables.push_back(std::make_shared<RenderableTerrain>());
	m_renderables.push_back(std::make_shared<RenderableWave>());
	m_renderables.push_back(std::make_shared<RenderableEffect>());

	for(auto renderable : m_renderables){
		renderable->Initialize(m_pContext);
	}

	m_PlayerPass = new PlayerPass();
	m_PlayerPass->Initialize(this, m_pContext);


	m_EditorPass = new EditorPass();
	m_EditorPass->Initialize(this, m_pContext);

#ifdef _EDITOR
	pShowPlayer = &m_pContext->pEditor->GetUI<MenuBar>()->showPlayerView;
	pShowEditor = &m_pContext->pEditor->GetUI<MenuBar>()->showEditorView;

	PlayButtonTexture = m_pContext->pResource->Load<TextureData> ("Asset/Texture/UI/Control/Play.png");
	PauseButtonTexture = m_pContext->pResource->Load<TextureData>("Asset/Texture/UI/Control/Pause.png");
	StopButtonTexture = m_pContext->pResource->Load<TextureData> ("Asset/Texture/UI/Control/Stop.png");
	StepButtonTexture = m_pContext->pResource->Load<TextureData> ("Asset/Texture/UI/Control/Step.png");
#else
	pShowPlayer = nullptr;
	pShowEditor = nullptr;
#endif // _EDITOR

	auto m_FullScreenVS = m_pContext->pResource->Load<VertexShaderData>("Asset\\Shader\\PostEffectVS.cso");
	auto m_FullScreenPS = m_pContext->pResource->Load<PixelShaderData>("Asset\\Shader\\PostEffectPS.cso");

	DeferredPS = m_pContext->pResource->Load<PixelShaderData>("Asset\\Shader\\DeferredRenderingPS.cso");
	ForwardPS = m_pContext->pResource->Load<PixelShaderData>("Asset\\Shader\\ForwardRenderingPS.cso");

#ifdef _EDITOR
	//ReCompilePixelShaders();
#endif // _EDITOR

	pCopyShader = new PostEffectShader();
	pCopyShader->m_VS = m_FullScreenVS->m_VertexShader; // フルスクリーンVS
	pCopyShader->m_PS = m_FullScreenPS->m_PixelShader; // 単純に SRV → out を返す PS
	pCopyShader->m_InputLayout = m_FullScreenVS->m_VertexLayout;
}

void RenderSystem::Finalize(){

	if(copyShader){
		delete m_CopyShader;
		pCopyShader = nullptr;
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

	m_pContext->pDebug->LOG_DEBUG("RenderSystemの終了処理が完了しました。");
}

void RenderSystem::Update(float deltaTime) {
	
	// コンポーネントを持つエンティティの検索
	for (auto& [name, scene] : m_pContext->pSceneManager->GetActiveScenes()) {
		auto m_Context= scene->GetSceneContext();
		const auto& modelEntities = pContext->pComponent->FindEntitiesWithComponent<ModelRendererComponent>();
		if (modelEntities.empty()) {
			continue;
		} else {
			for (Entity entity : modelEntities) {
				auto* modelRenderer = context->pComponent->GetComponent<ModelRendererComponent>(entity);
				modelRenderer->animationTime += deltaTime * 60.0f;
			}
		}
	}
}

void RenderSystem::EditorUpdate(float deltaTime)
{
	auto* dc = m_pContext->pGraphics->GetDeviceContext();

	for(auto& [name, scene] : m_pContext->pSceneManager->GetActiveScenes()){
		auto m_Context= scene->GetSceneContext();
		const auto& modelEntities =
			context->pComponent->FindEntitiesWithComponent<ModelRendererComponent>();

		for(Entity entity : modelEntities){
			auto* mr = context->pComponent->GetComponent<ModelRendererComponent>(entity);

			if(!mr->model){
				mr->CreateModel(context);
				continue;
			}
			if(mr->blendedAnimations.empty()){
				continue;
			}
			mr->model->UpdateBoneAnimation(
				mr->blendedAnimations,
				mr->animationTime
			);
			const bool m_UseGpuskinning= mr->model->m_Bones.size() <= BONE_MAX_COUNT;

			if(useGPUSkinning){
				mr->model->UpdateAndDispatchSkinning(m_pContext->pGraphics, mr->dynamicVertexBuffers);
			}else{
				for(size_t i = 0; i < mr->dynamicVertexBuffers.size(); i++){
					D3D11_MAPPED_SUBRESOURCE m_Mapped{};
					HRESULT m_Hr= dc->Map(
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


std::shared_ptr<TextureData> RenderSystem::GetEnvironmentMap() const {
	if(m_PlayerPass && m_PlayerPass->pLightingPass)
		return m_PlayerPass->pLightingPass->m_EnvironmentMap;
	return m_Nullptr;
}

ID3D11SamplerState* RenderSystem::GetEnvMapSampler() const {
	if(m_PlayerPass && m_PlayerPass->pLightingPass)
		return m_PlayerPass->pLightingPass->m_EnvMapSampler;
	return m_Nullptr;
}


void RenderSystem::UpdateSkyBoxEnvironmentMap() {
	if(!m_PlayerPass || !m_PlayerPass->pLightingPass){
		return;
	}

	// EnvironmentMapComponent を持つエンティティの TextureComponent を環境マップとして設定する
	std::shared_ptr<TextureData> m_EnvTexture;
	bool m_Found= false;
	for(auto& [sceneName, scene] : m_pContext->pSceneManager->GetActiveScenes()){
		if(found) break;
		auto m_Ctx= scene->GetSceneContext();
		auto m_Entities= ctx->pComponent->FindEntitiesWithComponent<EnvironmentMapComponent>();
		for(Entity ent : entities){
			auto* envComp = ctx->pComponent->GetComponent<EnvironmentMapComponent>(ent);
			if(!envComp || !envComp->enabled) continue;

			auto* texComp = ctx->pComponent->GetComponent<TextureComponent>(ent);
			if(texComp && texComp->m_TextureData){
				envTexture = texComp->m_TextureData;
			}
			found = true;
			break;
		}
	}

	// PlayerPass と EditorPass の両方に同期
	if(m_PlayerPass->pLightingPass->m_EnvironmentMap != envTexture){
		m_PlayerPass->pLightingPass->m_EnvironmentMap = envTexture;
	}
	if(m_EditorPass && m_EditorPass->pLightingPass){
		if(m_EditorPass->pLightingPass->m_EnvironmentMap != envTexture){
			m_EditorPass->pLightingPass->m_EnvironmentMap = envTexture;
		}
	}
}


void RenderSystem::Draw(){

	// スカイスフィアの環境マップを自動更新
	UpdateSkyBoxEnvironmentMap();

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
				(float)m_pContext->pRenderer->GetGraphicsContext()->m_width,
				(float)m_pContext->pRenderer->GetGraphicsContext()->m_height
			)
		);
		m_PlayerPass->Execute(renderPassContext);
		if (!m_PlayerPass->pResult) {
			return;
		}

		float m_ClearColor[4] = { 1.0f,1.0f,1.0f,1.0f };
		m_pContext->pGraphics->ResetViewport();
		m_pContext->pGraphics->Clear(clearColor);
		m_pContext->pGraphics->GetDeviceContext()->OMSetRenderTargets(1, m_pContext->pGraphics->GetpRenderTargetView(), m_pContext->pGraphics->GetDepthStencilView());
		m_pContext->pGraphics->DrawQuad(copyShader, m_PlayerPass->pResult);
	}
	m_pContext->pGraphics->ResetViewport();
	m_pContext->pGraphics->GetDeviceContext()->OMSetRenderTargets(1, m_pContext->pGraphics->GetpRenderTargetView(), m_pContext->pGraphics->GetDepthStencilView());
}

bool RenderSystem::decode(const YAML::Node& node){
	if(node["ShaderPath"])
		ShaderPath = node["ShaderPath"].as<std::string>();

	if(node["ShaderMaterial"]){
		ShaderMaterials.clear();
		YAML::Node m_MaterialsNode= node["ShaderMaterial"];
		for(auto material : materialsNode){
			ShaderMaterial m_ShaderMaterial;
			shaderMaterial.filePath = material.first.as<std::string>();
			shaderMaterial.entryPoint = material.second.as<std::string>();
			ShaderMaterials.push_back(shaderMaterial);
		}
	}

	return m_True;
}

YAML::Node RenderSystem::encode(){

	YAML::Node m_Node;
	node["ShaderPath"] = ShaderPath;

	YAML::Node m_MaterialsNode;
	for(const auto& material : ShaderMaterials){
		materialsNode[material.filePath] = material.entryPoint;
	}

	node["ShaderMaterial"] = materialsNode;

	return m_Node;
}

void RenderSystem::SystemSetting() {

	float m_Width= ImGui::GetContentRegionAvail().x;

	// EnvironmentMap settings (スカイスフィアから自動取得)
	if(ImGui::TreeNode("EnvironmentMap")){
		std::shared_ptr<TextureData> m_EnvTex= m_PlayerPass ? m_PlayerPass->pLightingPass->m_EnvironmentMap : nullptr;
		if(envTex && envTex->pTexture){
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Using environment map texture:");
			ImGui::TextWrapped("%s", envTex->FilePath.c_str());
			float m_PreviewWidth= ImGui::GetContentRegionAvail().x;
			ImGui::Image(
				(ImTextureID)envTex->pTexture.Get(),
				ImVec2(previewWidth, previewWidth * 0.5f)
			);
		} else {
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "No environment map source found in scene.");
			ImGui::TextDisabled("Add EnvironmentMapComponent + TextureComponent to an entity.");
		}
		ImGui::TreePop();
	}
	ImGui::Separator();

	if (ImGui::TreeNode("RenderDebug")) {
		if (ImGui::TreeNode("Editor View")) {
			width = ImGui::GetContentRegionAvail().x;
			ImGui::Image(
				m_EditorPass->pResult,
				ImVec2(width, m_EditorPass->pLightingPass->pRenderTarget->size.y / m_EditorPass->pLightingPass->pRenderTarget->size.x * width)
			);

			ImGui::Text("ShadowMap Render Target");
			ImGui::Image(
				m_EditorPass->pShadowMapPass->pShadowRenderTarget->srv.Get(),
				ImVec2(width, m_EditorPass->pShadowMapPass->pShadowRenderTarget->size.y / m_EditorPass->pShadowMapPass->pShadowRenderTarget->size.x * width)
			);
			ImGui::TreePop();
		}
		ImGui::Separator();
		if (ImGui::TreeNode("Player View")) {
			width = ImGui::GetContentRegionAvail().x;

			ImGui::Image(
				m_PlayerPass->pResult,
				ImVec2(width, m_PlayerPass->pPlayerRenderTarget->size.y / m_PlayerPass->pPlayerRenderTarget->size.x * width)
			);

			ImGui::Text("ShadowMap Render Target");
			ImGui::Image(
				m_PlayerPass->pShadowMapPass->pShadowRenderTarget->srv.Get(),
				ImVec2(width, m_PlayerPass->pShadowMapPass->pShadowRenderTarget->size.y / m_PlayerPass->pShadowMapPass->pShadowRenderTarget->size.x * width)
			);
			ImGui::TreePop();
		}
		ImGui::Separator();
		if (ImGui::TreeNode("GBuffer Render Targets")) {
			width = ImGui::GetContentRegionAvail().x;

			for (auto RT : m_PlayerPass->pGBufferPass->pRenderTargets) {
				if (RT->type == RenderTargetType::RENDERTARGET_TYPE_COLOR_NO_DSV && RT->srv) {
					ImGui::Image(
						RT->srv.Get(),
						ImVec2(width, m_PlayerPass->pPlayerRenderTarget->size.y / m_PlayerPass->pPlayerRenderTarget->size.x * width)
					);
				}
			}
			ImGui::TreePop();
		}
		ImGui::TreePop();
	}
	if(ImGui::TreeNode("ShaderMaterials")){

		APPCONFIG& pConfig = m_pContext->pConfig->appConfig;
		const float m_ChildHeight= 300.0f;
		const float m_ChildChildHeight= 200.0f;

		// Safety checks
		if(ShaderMaterials.empty()){
			ImGui::TextDisabled("No shader materials configured.");
			if(ImGui::Button("Add Material")){
				ShaderMaterial m_Def;
				def.filePath = "NewShader.hlsli";
				def.entryPoint = "ShadeMaterial_New";
				ShaderMaterials.push_back(def);
			}
			return;
		}

		static int m_SelectedIndex= 0;

		//------------------------------------
		// 2カラム Table レイアウト
		//------------------------------------
		if(ImGui::BeginTable("MaterialTable", 2,
							 ImGuiTableFlags_Resizable |
							 ImGuiTableFlags_BordersInnerV |
							 ImGuiTableFlags_SizingStretchProp)){
			// 左：固定幅
			ImGui::TableSetupColumn("List", ImGuiTableColumnFlags_WidthFixed, 260.0f);
			// 右：残り全部
			ImGui::TableSetupColumn("Editor", ImGuiTableColumnFlags_WidthStretch);

			ImGui::TableNextRow();

			//====================================
			// 左ペイン：Material List
			//====================================
			ImGui::TableSetColumnIndex(0);
			ImGui::BeginChild("MaterialList", ImVec2(0, childHeight), true);
			ImGui::BeginChild("Materials", ImVec2(0, childChildHeight), true);

			for(int i = 0; i < (int)ShaderMaterials.size(); ++i){
				const auto& mat = ShaderMaterials[i];
				char m_Label[256];
				snprintf(label, sizeof(label), "%02d: %s", i, mat.filePath.c_str());
				if(ImGui::Selectable(label, selectedIndex == i)){
					selectedIndex = i;
				}
			}
			ImGui::EndChild();
			ImGui::Separator();

			if(ImGui::Button("Add")){
				ShaderMaterial m_Def;
				def.filePath = "NewShader.hlsli";
				def.entryPoint = "ShadeMaterial_New";
				ShaderMaterials.push_back(def);
				selectedIndex = (int)ShaderMaterials.size() - 1;
			}
			ImGui::SameLine();
			if(ImGui::Button("Remove") &&
			   selectedIndex >= 0 &&
			   selectedIndex < (int)ShaderMaterials.size()){
				ShaderMaterials.erase(
					ShaderMaterials.begin() + selectedIndex);
				selectedIndex = std::clamp(
					selectedIndex - 1, 0,
					(int)ShaderMaterials.size() - 1);
			}
			ImGui::SameLine();

			if(ImGui::Button("Duplicate") &&
			   selectedIndex >= 0 &&
			   selectedIndex < (int)ShaderMaterials.size()){
				ShaderMaterials.push_back(
					ShaderMaterials[selectedIndex]);
			}

			ImGui::Separator();

			if(ImGui::Button("Up") && selectedIndex > 0){
				std::swap(ShaderMaterials[selectedIndex],
						  ShaderMaterials[selectedIndex - 1]);
				--selectedIndex;
			}
			ImGui::SameLine();
			if(ImGui::Button("Down") &&
			   selectedIndex + 1 < (int)ShaderMaterials.size()){
				std::swap(ShaderMaterials[selectedIndex],
						  ShaderMaterials[selectedIndex + 1]);
				++selectedIndex;
			}

			ImGui::EndChild();

			//====================================
			// 右ペイン：Editor
			//====================================
			ImGui::TableSetColumnIndex(1);
			ImGui::BeginChild("MaterialEditor", ImVec2(0, childHeight), true);

			if(selectedIndex >= 0 &&
			   selectedIndex < (int)ShaderMaterials.size()){
				auto& mat = ShaderMaterials[selectedIndex];

				ImGui::Text("Index: %d", selectedIndex);
				ImGui::Separator();

				// File Path
				ImGui::UndoInputText("File Path",   &mat.filePath,   512);

				// Entry Point
				ImGui::UndoInputText("Entry Point", &mat.entryPoint, 128);

				ImGui::Separator();

				if(ImGui::Button("Save Config")){
					m_pContext->pConfig->SaveApplicationConfig(APPLICATION_CONFIG_PATH);
				}
				ImGui::SameLine();
				if(ImGui::Button("Recompile")){
					ReCompilePixelShaders();
				}
			} else{
				ImGui::TextDisabled("No selection");
			}

			ImGui::EndChild();

			ImGui::EndTable();
		}
		ImGui::TreePop();
	}
}

void RenderSystem::DrawRenderLayerToggleUI() {

	ImGui::SameLine();

	std::string m_PreviewText;
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

	CameraEntityData m_CameraData{};
	for (auto& [name, scene] : m_pContext->pSceneManager->GetActiveScenes()) {
		auto m_Context= scene->GetSceneContext();
		// カメラを取得
		auto m_Entities= pContext->pComponent->FindEntitiesWithComponent<CameraComponent>();
		if (entities.empty()) {
			continue;
		}
		cameraData.ref = EntityRef(entities[0], pContext);
		cameraData.pCameraComponent = pContext->pComponent->GetComponent<CameraComponent>(cameraData.ref.GetEntityID());
		cameraData.pTransformComponent = pContext->pComponent->GetComponent<TransformComponent>(cameraData.ref.GetEntityID());
		return m_CameraData;
	}
	return m_CameraData;
}

void RenderSystem::ControlButton(){

	if(!PlayButtonTexture.get()->pTexture){
		return;
	}

	ImTextureRef m_Play;
	Play._TexID = (ImTextureID)PlayButtonTexture.get()->pTexture.Get();
	ImTextureRef m_Pause;
	Pause._TexID = (ImTextureID)PauseButtonTexture.get()->pTexture.Get();
	ImTextureRef m_Stop;
	Stop._TexID = (ImTextureID)StopButtonTexture.get()->pTexture.Get();
	ImTextureRef m_Step;
	Step._TexID = (ImTextureID)StepButtonTexture.get()->pTexture.Get();

	ImVec4 m_DefaultButtonColor= ImVec4(1.0f, 1.0f, 1.0f, 0.8f);
	ImVec4 m_StopButtonColor= ImVec4(1.0f, 1.0f, 1.0f, 0.8f);
	if(m_pContext->pSceneManager->State == SceneManagerState::Stopped){
		StopButtonColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
	}
	ImGui::BeginDisabled(m_pContext->pSceneManager->State == SceneManagerState::Stopped);

	if(ImGui::ImageButton("Stop", Stop, ImVec2(20, 20),ImVec2(0,0),ImVec2(1,1),ImVec4(0,0,0,0), StopButtonColor)){
		if(m_pContext->pSceneManager->State != SceneManagerState::Stopped){

			m_pContext->pSceneManager->State = SceneManagerState::Stopped; // シーンの状態をエディタに戻す
			ImGui::SetWindowFocus("Editor View");
		}
	}
	ImGui::EndDisabled();

	ImGui::SameLine();

	// ツールバー内容
	if(m_pContext->pSceneManager->State == SceneManagerState::Playing){
		if(ImGui::ImageButton("Pause", Pause, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), DefaultButtonColor)){
			m_pContext->pSceneManager->State = SceneManagerState::Paused; // シーンの状態を 一時停止に変更
		}
	} else{
		if(ImGui::ImageButton("Play", Play, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), DefaultButtonColor)){
			m_pContext->pSceneManager->State = SceneManagerState::Playing; // シーンの状態を再生中に変更
			ImGui::SetWindowFocus("Play View");
		}
	}
	ImGui::SameLine();
	if(ImGui::ImageButton("Step", Step, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), DefaultButtonColor)){
		m_pContext->pSceneManager->State = SceneManagerState::Step; // シーンの状態を ステップに変更
	}
}

void RenderSystem::PlayerView(){


	ImGui::Begin("Play View", showPlayer, 0);

	// 共通UI（元のControlButtonやSeparatorなど）
	ControlButton();
	ImGui::Separator();

	// カメラコンポーネントを持つエンティティ取得
	const CameraEntityData& cameraData = FindCameraEntity();
	if(!cameraData.pCameraComponent){
		ImGui::Text("No CameraBuffer Component found.");
		ImGui::End();
		return;
	}
	// 利用可能な領域サイズを取得
	ImVec2 m_Avail= ImGui::GetContentRegionAvail();

	if (avail.x <= 0.0f || avail.y <= 0.0f) {
		ImGui::End();
		return;
	}

	RenderPassContext renderPassContext(
		RenderPhase::PHASE_GBUFFER,
		playerRenderLayerVisible,
		cameraData,
		Vector2(avail.x, avail.y)
	);
	m_PlayerPass->Execute(renderPassContext);

	ImGui::Image((ImTextureRef)m_PlayerPass->pResult, avail);
	ImGui::End();

	m_pContext->pGraphics->GetDeviceContext()->OMSetRenderTargets(
		1, m_pContext->pGraphics->GetpRenderTargetView(), m_pContext->pGraphics->GetDepthStencilView()
	);
}

void RenderSystem::ReCompilePixelShaders() {
	const auto& pConfig = m_pContext->pConfig->appConfig;

	// Shader/AutoGen
	const std::filesystem::path m_ShaderDir= ShaderPath;
	std::filesystem::create_directories(shaderDir);

	// ============================================================
	// Deferred Rendering PS
	// ============================================================
	{
		const std::filesystem::path m_OutputPath= shaderDir / "DeferredRenderingPS.hlsl";
		std::ofstream ofs(outputPath, std::ios::trunc);
		if (!ofs) {
			OutputDebugStringA("Failed to create DeferredRenderingPS.hlsl\n");
			return;
		}

		ofs << R"(#include "../commonDefine.h"
#include "../common.hlsl"
#include "../Material/MaterialDefine.hlsli"
#include "../Material/DeferredFunc.hlsli"
#include "../Material/MaterialFunc.hlsli"

)";

		for (const auto& mat : ShaderMaterials) {
			ofs << "#include \"../Material/" << mat.filePath << "\"\n";
		}

		ofs << R"(
float4 main(PS_IN In) : SV_Target
{
    MaterialInput m_Input= GetMaterialInput(In);
    float4 m_Result= float4(1, 0, 1, 1);

)";

		// [branch] if を使用してマテリアルごとに隔離生成
		for (size_t i = 0; i < ShaderMaterials.size(); ++i) {
			const auto& mat = ShaderMaterials[i];
			if (i == 0) {
				ofs << "    [branch] if (pInput.materialID == " << i << ")\n";
			} else {
				ofs << "    else if (pInput.materialID == " << i << ")\n";
			}
			ofs << "    {\n"
				"        Result = " << mat.entryPoint << "(pInput);\n"
				"    }\n";
		}

		ofs << R"(    else { /* default */ }

    return m_Result;
}
)";
		ofs.close();

		DeferredPS.reset();
		m_pContext->pResource->Unload<PixelShaderData>(outputPath.string().c_str());
		auto m_NewPs= m_pContext->pResource->Load<PixelShaderData>(outputPath.string().c_str());

		if (!newPS) {
			OutputDebugStringA("DeferredRenderingPS compile failed\n");
			return;
		}
		DeferredPS = newPS;
	}

	// ============================================================
	// Forward Rendering PS
	// ============================================================
	{
		const std::filesystem::path m_OutputPath= shaderDir / "ForwardRenderingPS.hlsl";
		std::ofstream ofs(outputPath, std::ios::trunc);
		if (!ofs) {
			OutputDebugStringA("Failed to create ForwardRenderingPS.hlsl\n");
			return;
		}

		ofs << R"(#include "../commonDefine.h"
#include "../common.hlsl"
#include "../Material/MaterialDefine.hlsli"
#include "../Material/FowardFunc.hlsli"
#include "../Material/MaterialFunc.hlsli"

)";

		for (const auto& mat : ShaderMaterials) {
			ofs << "#include \"../Material/" << mat.filePath << "\"\n";
		}

		ofs << R"(
float4 main(PS_IN In) : SV_Target
{
    MaterialInput m_Input= GetMaterialInput(In);
    float4 m_Result= float4(1, 0, 1, 1);

)";

		// Forward側も同様に [branch] if で生成
		for (size_t i = 0; i < ShaderMaterials.size(); ++i) {
			const auto& mat = ShaderMaterials[i];
			if (i == 0) {
				ofs << "    [branch] if (pInput.materialID == " << i << ")\n";
			} else {
				ofs << "    else if (pInput.materialID == " << i << ")\n";
			}
			ofs << "    {\n"
				"        Result = " << mat.entryPoint << "(pInput);\n"
				"    }\n";
		}

		ofs << R"(    else { /* default */ }

    if (Result.a <= ALPHA_CLIP_THRESHOLD)
    {
        discard;
    }

    return m_Result;
}
)";
		ofs.close();

		ForwardPS.reset();
		m_pContext->pResource->Unload<PixelShaderData>(outputPath.string().c_str());
		auto m_NewPs= m_pContext->pResource->Load<PixelShaderData>(outputPath.string().c_str());

		if (!newPS) {
			OutputDebugStringA("ForwardRenderingPS compile failed\n");
			return;
		}
		ForwardPS = newPS;
	}
}


void RenderSystem::EditorView(){

	GraphicsContext* graphicsContext = m_pContext->pGraphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	ViewWindow* viewWindow = m_pContext->pEditor->GetUI<ViewWindow>();

	TransformComponent m_EditorCameraTransform;
	editorCameraTransform.position = viewWindow->m_EditorCameraPosition;
	editorCameraTransform.SetRotationEuler(viewWindow->m_editorCameraRotation);

	CameraComponent m_EditorCamera;
	editorCamera.FOV = DirectX::XM_PIDIV4;
	editorCamera.NearClip = 0.01f;
	editorCamera.FarClip = 1000.0f;
	editorCamera.isLock = false;
	editorCamera.viewMatrix = DirectX::XMMatrixLookAtLH(
		editorCameraTransform.position.ToXMVECTOR(),
		(editorCameraTransform.position + editorCameraTransform.front()).ToXMVECTOR(),
		{ 0.0f, 1.0f, 0.0f }
	);
	CameraEntityData m_CameraData;
	cameraData.pCameraComponent = &editorCamera;
	cameraData.pTransformComponent = &editorCameraTransform;

	RenderPassContext renderPassContext(
		RenderPhase::PHASE_GBUFFER,
		editorRenderLayerVisible,
		cameraData,
		m_pContext->EditorScreenSize
	);
	m_EditorPass->Execute(renderPassContext);
}
