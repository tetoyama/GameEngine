// Scene/System/renderSystem.cpp
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
#include "RenderPass/PlayerView/PlayerPass.h"
#include "RenderPass/EditorView/EditorPass.h"
#include "Renderable/Wave/RenderableWave.h"
#include <Editor/UI/ViewWindow.h>
#include "Renderable/Effect/RenderableEffect.h"

#include "Service/Config/configSystem.h"
#include "Service/Config/appConfig.h"

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
	m_renderables.push_back(std::make_shared<RenderableEffect>());

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

	DefferredPS = m_context->resource->Load<PixelShaderData>("Asset\\Shader\\DeferredRenderingPS.cso");
	ForwardPS = m_context->resource->Load<PixelShaderData>("Asset\\Shader\\ForwardRenderingPS.cso");

#ifdef _EDITOR
	//ReCompilePixelShaders();
#endif // _EDITOR

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

	m_context->debug->LOG_DEBUG("RenderSystemの終了処理が完了しました。");
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
			auto* mr = context->component->GetComponent<ModelRendererComponent>(entity);

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
			const bool useGPUSkinning = mr->model->m_Bones.size() <= BONE_MAX_COUNT;

			if(useGPUSkinning){
				mr->model->UpdateAndDispatchSkinning(m_context->graphics, mr->dynamicVertexBuffers);
			}else{
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

bool RenderSystem::decode(const YAML::Node& node){
	if(node["ShaderPath"])
		ShaderPath = node["ShaderPath"].as<std::string>();

	if(node["ShaderMaterial"]){
		ShaderMaterials.clear();
		YAML::Node materialsNode = node["ShaderMaterial"];
		for(auto material : materialsNode){
			ShaderMaterial shaderMaterial;
			shaderMaterial.filePath = material.first.as<std::string>();
			shaderMaterial.entryPoint = material.second.as<std::string>();
			ShaderMaterials.push_back(shaderMaterial);
		}
	}

	return true;
}

YAML::Node RenderSystem::encode(){

	YAML::Node node;
	node["ShaderPath"] = ShaderPath;

	YAML::Node materialsNode;
	for(const auto& material : ShaderMaterials){
		materialsNode[material.filePath] = material.entryPoint;
	}

	node["ShaderMaterial"] = materialsNode;

	return node;
}

void RenderSystem::SystemSetting() {

	float width = ImGui::GetContentRegionAvail().x;

	if (ImGui::TreeNode("RenderDebug")) {
		if (ImGui::TreeNode("Editor View")) {
			width = ImGui::GetContentRegionAvail().x;
			ImGui::Image(
				m_EditorPass->result,
				ImVec2(width, m_EditorPass->editorRenderTarget->size.y / m_EditorPass->editorRenderTarget->size.x * width)
			);

			ImGui::Text("ShadowMap Render Target");
			ImGui::Image(
				m_EditorPass->shadowMapPass->shadowRenderTarget->srv.Get(),
				ImVec2(width, m_EditorPass->shadowMapPass->shadowRenderTarget->size.y / m_EditorPass->shadowMapPass->shadowRenderTarget->size.x * width)
			);
			ImGui::TreePop();
		}
		ImGui::Separator();
		if (ImGui::TreeNode("Player View")) {
			width = ImGui::GetContentRegionAvail().x;

			ImGui::Image(
				m_PlayerPass->result,
				ImVec2(width, m_PlayerPass->playerRenderTarget->size.y / m_PlayerPass->playerRenderTarget->size.x * width)
			);

			ImGui::Text("ShadowMap Render Target");
			ImGui::Image(
				m_PlayerPass->shadowMapPass->shadowRenderTarget->srv.Get(),
				ImVec2(width, m_PlayerPass->shadowMapPass->shadowRenderTarget->size.y / m_PlayerPass->shadowMapPass->shadowRenderTarget->size.x * width)
			);
			ImGui::TreePop();
		}
		ImGui::Separator();
		if (ImGui::TreeNode("GBuffer Render Targets")) {
			width = ImGui::GetContentRegionAvail().x;

			for (auto RT : m_PlayerPass->gBufferPass->pRenderTargets) {
				if (RT->type == RenderTargetType::RENDERTARGET_TYPE_COLOR_NO_DSV && RT->srv) {
					ImGui::Image(
						RT->srv.Get(),
						ImVec2(width, m_PlayerPass->playerRenderTarget->size.y / m_PlayerPass->playerRenderTarget->size.x * width)
					);
				}
			}
			ImGui::TreePop();
		}
		ImGui::TreePop();
	}
	if(ImGui::TreeNode("ShaderMaterials")){

		APPCONFIG& config = m_context->config->appConfig;
		const float childHeight = 300.0f;
		const float childChildHeight = 200.0f;

		// Safety checks
		if(ShaderMaterials.empty()){
			ImGui::TextDisabled("No shader materials configured.");
			if(ImGui::Button("Add Material")){
				ShaderMaterial def;
				def.filePath = "NewShader.hlsli";
				def.entryPoint = "ShadeMaterial_New";
				ShaderMaterials.push_back(def);
			}
			return;
		}

		static int selectedIndex = 0;

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
				char label[256];
				snprintf(label, sizeof(label), "%02d: %s", i, mat.filePath.c_str());
				if(ImGui::Selectable(label, selectedIndex == i)){
					selectedIndex = i;
				}
			}
			ImGui::EndChild();
			ImGui::Separator();

			if(ImGui::Button("Add")){
				ShaderMaterial def;
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
				{
					char buf[512] = {};
					strncpy(buf, mat.filePath.c_str(), sizeof(buf) - 1);
					if(ImGui::InputText("File Path", buf, sizeof(buf))){
						mat.filePath = buf;
					}
				}

				// Entry Point
				{
					char buf[128] = {};
					strncpy(buf, mat.entryPoint.c_str(), sizeof(buf) - 1);
					if(ImGui::InputText("Entry Point", buf, sizeof(buf))){
						mat.entryPoint = buf;
					}
				}

				ImGui::Separator();

				if(ImGui::Button("Save Config")){
					m_context->config->SaveApplicationConfig(APPLICATION_CONFIG_PATH);
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
		cameraData.ref = EntityRef(entities[0], context);
		cameraData.CameraComponent = context->component->GetComponent<CameraComponent>(cameraData.ref.GetEntityID());
		cameraData.transformComponent = context->component->GetComponent<TransformComponent>(cameraData.ref.GetEntityID());
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
	if(!cameraData.CameraComponent){
		ImGui::Text("No CameraBuffer Component found.");
		ImGui::End();
		return;
	}
	// 利用可能な領域サイズを取得
	ImVec2 avail = ImGui::GetContentRegionAvail();

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

	ImGui::Image((ImTextureRef)m_PlayerPass->result, avail);
	ImGui::End();

	m_context->graphics->GetDeviceContext()->OMSetRenderTargets(
		1, m_context->graphics->GetpRenderTargetView(), m_context->graphics->GetDepthStencilView()
	);
}

void RenderSystem::ReCompilePixelShaders() {
	const auto& config = m_context->config->appConfig;

	// Shader/AutoGen
	const std::filesystem::path shaderDir = ShaderPath;
	std::filesystem::create_directories(shaderDir);

	// ============================================================
	// Deferred Rendering PS
	// ============================================================
	{
		const std::filesystem::path outputPath = shaderDir / "DeferredRenderingPS.hlsl";
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
    MaterialInput input = GetMaterialInput(In);
    float4 Result = float4(1, 0, 1, 1);

)";

		// [branch] if を使用してマテリアルごとに隔離生成
		for (size_t i = 0; i < ShaderMaterials.size(); ++i) {
			const auto& mat = ShaderMaterials[i];
			if (i == 0) {
				ofs << "    [branch] if (input.materialID == " << i << ")\n";
			} else {
				ofs << "    else if (input.materialID == " << i << ")\n";
			}
			ofs << "    {\n"
				"        Result = " << mat.entryPoint << "(input);\n"
				"    }\n";
		}

		ofs << R"(    else { /* default */ }

    return Result;
}
)";
		ofs.close();

		DefferredPS.reset();
		m_context->resource->Unload<PixelShaderData>(outputPath.string().c_str());
		auto newPS = m_context->resource->Load<PixelShaderData>(outputPath.string().c_str());

		if (!newPS) {
			OutputDebugStringA("DeferredRenderingPS compile failed\n");
			return;
		}
		DefferredPS = newPS;
	}

	// ============================================================
	// Forward Rendering PS
	// ============================================================
	{
		const std::filesystem::path outputPath = shaderDir / "ForwardRenderingPS.hlsl";
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
    MaterialInput input = GetMaterialInput(In);
    float4 Result = float4(1, 0, 1, 1);

)";

		// Forward側も同様に [branch] if で生成
		for (size_t i = 0; i < ShaderMaterials.size(); ++i) {
			const auto& mat = ShaderMaterials[i];
			if (i == 0) {
				ofs << "    [branch] if (input.materialID == " << i << ")\n";
			} else {
				ofs << "    else if (input.materialID == " << i << ")\n";
			}
			ofs << "    {\n"
				"        Result = " << mat.entryPoint << "(input);\n"
				"    }\n";
		}

		ofs << R"(    else { /* default */ }

    if (Result.a <= ALPHA_CLIP_THRESHOLD)
    {
        discard;
    }

    return Result;
}
)";
		ofs.close();

		ForwardPS.reset();
		m_context->resource->Unload<PixelShaderData>(outputPath.string().c_str());
		auto newPS = m_context->resource->Load<PixelShaderData>(outputPath.string().c_str());

		if (!newPS) {
			OutputDebugStringA("ForwardRenderingPS compile failed\n");
			return;
		}
		ForwardPS = newPS;
	}
}


void RenderSystem::EditorView(){

	GraphicsContext* graphicsContext = m_context->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	ViewWindow* viewWindow = m_context->editor->GetUI<ViewWindow>();

	TransformComponent editorCameraTransform;
	editorCameraTransform.position = viewWindow->m_EditorCameraPosition;
	editorCameraTransform.SetRotationEuler(viewWindow->m_editorCameraRotation);

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
	cameraData.CameraComponent = &editorCamera;
	cameraData.transformComponent = &editorCameraTransform;

	RenderPassContext renderPassContext(
		RenderPhase::PHASE_GBUFFER,
		editorRenderLayerVisible,
		cameraData,
		m_context->EditorScreenSize
	);
	m_EditorPass->Execute(renderPassContext);
}

