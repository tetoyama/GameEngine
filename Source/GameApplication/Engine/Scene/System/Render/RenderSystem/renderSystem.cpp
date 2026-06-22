// =======================================================================
// 
// renderSystem.cpp
// 
// =======================================================================
#include "renderSystem.h"
#include "buildSetting.h"

#include <algorithm>
#include <array>
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
#include <Component/materialComponent.h>
#include <Component/meshRendererComponent.h>
#include <Component/BillBoardRendererComponent.h>
#include <Component/2DspriteRendererComponent.h>
#include <Component/terrainComponent.h>
#include <Component/waveComponent.h>
#include <Component/particleComponent.h>
#include <Component/EffectComponent.h>
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
#include "RenderPass/PlayerView/PlayerViewRefreshPolicy.h"
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
	m_context->debug->LOG_DEBUG("RenderSystemを初期化中...");

	lazyTimer = 0.0f;

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
	lazyTimer += deltaTime;

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


std::shared_ptr<TextureData> RenderSystem::GetEnvironmentMap() const {
	if(m_PlayerPass && m_PlayerPass->lightingPass)
		return m_PlayerPass->lightingPass->m_EnvironmentMap;
	return nullptr;
}

ID3D11SamplerState* RenderSystem::GetEnvMapSampler() const {
	if(m_PlayerPass && m_PlayerPass->lightingPass)
		return m_PlayerPass->lightingPass->m_EnvMapSampler;
	return nullptr;
}


void RenderSystem::UpdateSkyBoxEnvironmentMap() {
	if(!m_PlayerPass || !m_PlayerPass->lightingPass){
		return;
	}

	// EnvironmentMapComponent を持つエンティティの TextureComponent を環境マップとして設定する
	std::shared_ptr<TextureData> envTexture;
	bool found = false;
	for(auto& [sceneName, scene] : m_context->sceneManager->GetActiveScenes()){
		if(found) break;
		auto ctx = scene->GetSceneContext();
		auto entities = ctx->component->FindEntitiesWithComponent<EnvironmentMapComponent>();
		for(Entity ent : entities){
			auto* envComp = ctx->component->GetComponent<EnvironmentMapComponent>(ent);
			if(!envComp || !envComp->enabled) continue;

			auto* texComp = ctx->component->GetComponent<TextureComponent>(ent);
			if(texComp && texComp->m_TextureData){
				envTexture = texComp->m_TextureData;
			}
			found = true;
			break;
		}
	}

	// PlayerPass と EditorPass の両方に同期
	if(m_PlayerPass->lightingPass->m_EnvironmentMap != envTexture){
		m_PlayerPass->lightingPass->m_EnvironmentMap = envTexture;
	}
	if(m_EditorPass && m_EditorPass->lightingPass){
		if(m_EditorPass->lightingPass->m_EnvironmentMap != envTexture){
			m_EditorPass->lightingPass->m_EnvironmentMap = envTexture;
		}
	}
}


void RenderSystem::Draw(){

	// スカイスフィアの環境マップを自動更新
	UpdateSkyBoxEnvironmentMap();

	if(showEditor && *showEditor){
		if(m_context->sceneManager->State == SceneManagerState::Playing){
			if(lazyTimer >= 0.1f){
				lazyTimer = 0.0f;
				EditorView();
			}
		} else{
			EditorView();
		}
	}
	if(showPlayer && *showPlayer){
		PlayerView();
	} else if(showPlayer && !*showPlayer && ((m_context->sceneManager->State == SceneManagerState::Playing) || !(showEditor && *showEditor))){

		const Vector2 fullScreenSize(
			static_cast<float>(m_context->renderer->GetGraphicsContext()->m_width),
			static_cast<float>(m_context->renderer->GetGraphicsContext()->m_height)
		);
		m_context->PlayerScreenSize = fullScreenSize;

		RenderPassContext renderPassContext(
			RenderPhase::PHASE_GBUFFER,
			playerRenderLayerVisible,
			FindCameraEntity(),
			fullScreenSize
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


void RenderSystem::BuildRenderPackets(){
	std::array<RenderPacketWorkerBuffer, 1> workerBuffers{
		RenderPacketWorkerBuffer(0)
	};
	RenderPacketWorkerBuffer& worker = workerBuffers[0];
	uint64_t stableSequence = 0;

	struct SceneEntry {
		uint32_t contextID = 0;
		std::string name;
		SceneContext* context = nullptr;
	};

	std::vector<SceneEntry> scenes;
	for(const auto& [sceneName, scene] : m_context->sceneManager->GetActiveScenes()){
		if(!scene) continue;
		SceneContext* context = scene->GetSceneContext();
		if(!context || !context->component || context->contextID == 0) continue;
		scenes.push_back({context->contextID, sceneName, context});
	}
	std::sort(
		scenes.begin(),
		scenes.end(),
		[](const SceneEntry& lhs, const SceneEntry& rhs){
			if(lhs.contextID != rhs.contextID) return lhs.contextID < rhs.contextID;
			return lhs.name < rhs.name;
		}
	);

	for(const SceneEntry& sceneEntry : scenes){
		ComponentRegistry* components = sceneEntry.context->component;
		auto entities = components->FindEntitiesWithComponent<TransformComponent>();
		std::sort(
			entities.begin(),
			entities.end(),
			[](Entity lhs, Entity rhs){
				return lhs.GetPackedValue() < rhs.GetPackedValue();
			}
		);

		worker.Reserve(worker.Packets().size() + entities.size());
		for(Entity entity : entities){
			const TransformComponent* transform =
				components->GetComponent<TransformComponent>(entity);
			if(!transform) continue;

			const RenderLayerComponent* layerComponent =
				components->GetComponent<RenderLayerComponent>(entity);
			const OrderInLayerComponent* orderComponent =
				components->GetComponent<OrderInLayerComponent>(entity);
			const MaterialComponent* materialComponent =
				components->GetComponent<MaterialComponent>(entity);

			const RenderLayer layer = layerComponent
				? layerComponent->layer
				: RenderLayer::Opaque3D;
			const int32_t orderInLayer = orderComponent
				? orderComponent->order
				: 0;
			const uint32_t materialKey = materialComponent
				? static_cast<uint32_t>((std::max)(0, materialComponent->ShaderID))
				: 0u;

			RenderPacketTransformSnapshot snapshot;
			snapshot.position[0] = transform->position.x;
			snapshot.position[1] = transform->position.y;
			snapshot.position[2] = transform->position.z;
			const DirectX::XMFLOAT4& rotation = transform->GetRotation();
			snapshot.rotation[0] = rotation.x;
			snapshot.rotation[1] = rotation.y;
			snapshot.rotation[2] = rotation.z;
			snapshot.rotation[3] = rotation.w;
			snapshot.scale[0] = transform->scale.x;
			snapshot.scale[1] = transform->scale.y;
			snapshot.scale[2] = transform->scale.z;

			auto appendPacket = [&](RenderPacketKind kind){
				RenderPacket packet;
				packet.sceneContextID = sceneEntry.contextID;
				packet.entity = entity;
				packet.kind = kind;
				packet.layer = layer;
				packet.passMask = RenderPacketPassesForLayer(layer);
				packet.materialKey = materialKey;
				packet.orderInLayer = orderInLayer;
				packet.sortKey = MakeRenderPacketSortKey(
					layer,
					kind,
					materialKey,
					orderInLayer
				);
				packet.stableSequence = stableSequence++;
				packet.transform = snapshot;
				worker.Add(std::move(packet));
			};

			if(components->GetComponent<ModelRendererComponent>(entity)){
				appendPacket(RenderPacketKind::Model);
			}
			if(components->GetComponent<MeshRendererComponent>(entity)){
				appendPacket(RenderPacketKind::Mesh);
			}
			if(components->GetComponent<SpriteRendererComponent>(entity)){
				appendPacket(RenderPacketKind::Sprite);
			}
			if(components->GetComponent<BillBoardRendererComponent>(entity)){
				appendPacket(RenderPacketKind::Billboard);
			}
			if(components->GetComponent<ParticleComponent>(entity)){
				appendPacket(RenderPacketKind::Particle);
			}
			if(components->GetComponent<TerrainComponent>(entity)){
				appendPacket(RenderPacketKind::Terrain);
			}
			if(components->GetComponent<WaveComponent>(entity)){
				appendPacket(RenderPacketKind::Wave);
			}
			if(components->GetComponent<EffectComponent>(entity)){
				appendPacket(RenderPacketKind::Effect);
			}
		}
	}

	m_renderPacketBuffer.BeginFrame(++m_renderPacketGeneration);
	m_renderPacketBuffer.Merge(workerBuffers);
}

void RenderSystem::SubmitRenderPackets(){
	m_lastSubmittedPacketGeneration = m_renderPacketBuffer.Generation();
	Draw();
}

void RenderSystem::RegisterTasks(SystemScheduleBuilder& builder){

	using RenderUpdateQuery = ECSQuery::ComponentQueryView<
		ECSQuery::Read<TransformComponent>,
		ECSQuery::Write<ModelRendererComponent>
	>;

	builder.AddQueryTask<RenderUpdateQuery>(
		"RenderSystem.AnimationTime.Commit",
		SystemTaskDomain::Frame,
		SystemPhase::Late,
		0,
		StructuralAccess::None,
		ThreadAffinity::AnyWorker,
		[this](const SystemTaskContext& context){
			Update(context.deltaTime);
		}
	);

	builder.AddTask(
		"RenderSystem.Animation.Upload",
		SystemTaskDomain::Editor,
		SystemPhase::Earliest,
		0,
		SystemAccess::LegacyExclusive(),
		ThreadAffinity::MainThread,
		[this](const SystemTaskContext& context){
			EditorUpdate(context.deltaTime);
		}
	);

	SystemAccess packetBuildAccess;
	packetBuildAccess
		.ReadComponent<TransformComponent>()
		.ReadComponent<RenderLayerComponent>()
		.ReadComponent<OrderInLayerComponent>()
		.ReadComponent<MaterialComponent>()
		.ReadComponent<ModelRendererComponent>()
		.ReadComponent<MeshRendererComponent>()
		.ReadComponent<SpriteRendererComponent>()
		.ReadComponent<BillBoardRendererComponent>()
		.ReadComponent<ParticleComponent>()
		.ReadComponent<TerrainComponent>()
		.ReadComponent<WaveComponent>()
		.ReadComponent<EffectComponent>()
		.ReadResource<SceneManager>()
		.WriteResource<RenderPacketFrameBuffer>();

	builder.AddTask(
		"RenderSystem.Packet.Build",
		SystemTaskDomain::Render,
		SystemPhase::Early,
		0,
		std::move(packetBuildAccess),
		ThreadAffinity::AnyWorker,
		[this](const SystemTaskContext&){
			BuildRenderPackets();
		}
	);

	SystemAccess submitAccess = SystemAccess::LegacyExclusive();
	submitAccess.ReadResource<RenderPacketFrameBuffer>();
	builder.AddTask(
		"RenderSystem.Command.Submit",
		SystemTaskDomain::Render,
		SystemPhase::Late,
		0,
		std::move(submitAccess),
		ThreadAffinity::MainThread,
		[this](const SystemTaskContext&){
			SubmitRenderPackets();
		}
	);
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

	// 登録済みマテリアルごとに専用PSを生成・ロードする
	// (DeferredPSList / ForwardPSList / ForwardPSDebug を構築)
	ReCompilePixelShaders();
	
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

	// EnvironmentMap settings (スカイスフィアから自動取得)
	if(ImGui::TreeNode("EnvironmentMap")){
		std::shared_ptr<TextureData> envTex = m_PlayerPass ? m_PlayerPass->lightingPass->m_EnvironmentMap : nullptr;
		if(envTex && envTex->pTexture){
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Using environment map texture:");
			ImGui::TextWrapped("%s", envTex->FilePath.c_str());
			float previewWidth = ImGui::GetContentRegionAvail().x;
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
				m_EditorPass->result,
				ImVec2(width, m_EditorPass->lightingPass->pRenderTarget->size.y / m_EditorPass->lightingPass->pRenderTarget->size.x * width)
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
				ImGui::UndoInputText("File Path",   &mat.filePath,   512);

				// Entry Point
				ImGui::UndoInputText("Entry Point", &mat.entryPoint, 128);

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
		cameraData.cameraComponent = context->component->GetComponent<CameraComponent>(cameraData.ref.GetEntityID());
		cameraData.transformComponent = context->component->GetComponent<TransformComponent>(cameraData.ref.GetEntityID());
		return cameraData;
	}
	return cameraData;
}

void RenderSystem::ControlButton(){

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

	auto restoreMainRenderTarget = [this](){
		m_context->graphics->GetDeviceContext()->OMSetRenderTargets(
			1,
			m_context->graphics->GetpRenderTargetView(),
			m_context->graphics->GetDepthStencilView()
		);
	};

	if(!ImGui::Begin("Play View", showPlayer, 0)){
		ImGui::End();
		restoreMainRenderTarget();
		return;
	}

	ControlButton();
	ImGui::Separator();

	const CameraEntityData cameraData = FindCameraEntity();
	if(!cameraData.cameraComponent){
		ImGui::Text("No Camera Component found.");
		ImGui::End();
		restoreMainRenderTarget();
		return;
	}

	const ImVec2 avail = ImGui::GetContentRegionAvail();
	if(avail.x <= 0.0f || avail.y <= 0.0f){
		ImGui::End();
		restoreMainRenderTarget();
		return;
	}

	const Vector2 playerSize(avail.x, avail.y);
	m_context->PlayerScreenSize = playerSize;

	RenderPassContext renderPassContext(
		RenderPhase::PHASE_GBUFFER,
		playerRenderLayerVisible,
		cameraData,
		playerSize
	);

	const SceneManagerState state = m_context->sceneManager->State;
	const bool throttledState =
		state == SceneManagerState::Stopped ||
		state == SceneManagerState::Paused;
	const bool shouldRender = PlayerViewRefreshPolicy::ShouldRender(
		throttledState,
		lazyTimer,
		m_PlayerPass->result != nullptr
	);

	if(shouldRender){
		if(throttledState) lazyTimer = 0.0f;
		m_PlayerPass->Execute(renderPassContext);
	}

	if(m_PlayerPass->result){
		ImGui::Image((ImTextureRef)m_PlayerPass->result, avail);
	}else{
		ImGui::TextDisabled("Player render output is not available.");
	}
	ImGui::End();
	restoreMainRenderTarget();
}

void RenderSystem::ReCompilePixelShaders(){
	namespace fs = std::filesystem;
	using ShaderPtr = decltype(DeferredPSList)::value_type;

	const fs::path shaderDir = ShaderPath;
	fs::create_directories(shaderDir);

	const fs::path shaderRoot = shaderDir.parent_path().empty() ? shaderDir : shaderDir.parent_path();
	const fs::path cachePath = shaderDir / "ShaderCompileCache.yaml";

	// 旧リソースを退避。変更なしなら再利用する
	const auto oldDeferred = DeferredPSList;
	const auto oldForward = ForwardPSList;
	const ShaderPtr oldDeferredDebug =
		(oldDeferred.size() > ShaderMaterials.size()) ? oldDeferred.back() : ShaderPtr{};
	const ShaderPtr oldForwardDebug = ForwardPSDebug;

	DeferredPSList.clear();
	ForwardPSList.clear();
	DeferredPSList.resize(ShaderMaterials.size());
	ForwardPSList.resize(ShaderMaterials.size());

	auto toStamp = [](const fs::file_time_type& t) -> long long{
		return static_cast<long long>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch()).count());
		};

	auto getStamp = [&](const fs::path& p) -> long long{
		std::error_code ec;
		if(!fs::exists(p, ec)){
			return -1;
		}
		auto ft = fs::last_write_time(p, ec);
		if(ec){
			return -1;
		}
		return toStamp(ft);
		};

	auto readCache = [&]() -> YAML::Node{
		try{
			if(fs::exists(cachePath)){
				return YAML::LoadFile(cachePath.string());
			}
		} catch(const std::exception&){
		}
		return YAML::Node();
		};

	YAML::Node cacheRoot = readCache();

	auto getSeqEntry = [](const YAML::Node& seq, size_t index) -> YAML::Node{
		if(seq && seq.IsSequence() && index < seq.size()){
			return seq[index];
		}
		return YAML::Node();
		};

	auto makeDeps = [&](const fs::path& materialPath, bool forward, bool includeMaterialCount){
		std::vector<std::pair<std::string, long long>> deps;

		auto addFile = [&](const fs::path& p){
			deps.emplace_back(p.generic_string(), getStamp(p));
			};

		addFile(shaderRoot / "commonDefine.h");
		addFile(shaderRoot / "common.hlsl");
		addFile(shaderRoot / "Material/MaterialDefine.hlsli");
		addFile(shaderRoot / "Material/MaterialFunc.hlsli");
		addFile(shaderRoot / "Material" / (forward ? "FowardFunc.hlsli" : "DeferredFunc.hlsli"));

		if(!materialPath.empty()){
			addFile(shaderRoot / "Material" / materialPath);
		}

		if(includeMaterialCount){
			deps.emplace_back("__materialCount__", static_cast<long long>(ShaderMaterials.size()));
		}

		return deps;
		};

	auto depsMatch = [](const YAML::Node& node,
						const std::vector<std::pair<std::string, long long>>& deps) -> bool{
							if(!node || !node.IsSequence() || node.size() != deps.size()){
								return false;
							}

							for(size_t i = 0; i < deps.size(); ++i){
								const auto& d = deps[i];
								const YAML::Node item = node[i];

								if(!item || !item["path"] || !item["stamp"]){
									return false;
								}
								if(item["path"].as<std::string>() != d.first){
									return false;
								}
								if(item["stamp"].as<long long>() != d.second){
									return false;
								}
							}
							return true;
		};

	auto cacheMatches = [&](const YAML::Node& entry,
							const fs::path& hlslPath,
							const fs::path& csoPath,
							const std::string& entryPoint,
							const std::string& materialPath,
							const std::string& shaderModel,
							const std::vector<std::pair<std::string, long long>>& deps) -> bool{
								if(!entry || !entry.IsMap()){
									return false;
								}
								if(!fs::exists(csoPath)){
									return false;
								}
								if(!entry["hlsl"] || entry["hlsl"].as<std::string>() != hlslPath.generic_string()){
									return false;
								}
								if(!entry["cso"] || entry["cso"].as<std::string>() != csoPath.generic_string()){
									return false;
								}
								if(!entry["entryPoint"] || entry["entryPoint"].as<std::string>() != entryPoint){
									return false;
								}
								if(!entry["material"] || entry["material"].as<std::string>() != materialPath){
									return false;
								}
								if(!entry["shaderModel"] || entry["shaderModel"].as<std::string>() != shaderModel){
									return false;
								}
								if(!depsMatch(entry["deps"], deps)){
									return false;
								}
								return true;
		};

	auto makeEntry = [&](const fs::path& hlslPath,
						 const fs::path& csoPath,
						 const std::string& entryPoint,
						 const std::string& materialPath,
						 const std::string& shaderModel,
						 const std::vector<std::pair<std::string, long long>>& deps) -> YAML::Node{
							 YAML::Node entry;
							 entry["hlsl"] = hlslPath.generic_string();
							 entry["cso"] = csoPath.generic_string();
							 entry["entryPoint"] = entryPoint;
							 entry["material"] = materialPath;
							 entry["shaderModel"] = shaderModel;

							 YAML::Node depSeq(YAML::NodeType::Sequence);
							 for(const auto& d : deps){
								 YAML::Node item;
								 item["path"] = d.first;
								 item["stamp"] = d.second;
								 depSeq.push_back(item);
							 }
							 entry["deps"] = depSeq;
							 return entry;
		};

	auto writeDeferredSource = [&](std::ofstream& ofs,
								   const std::string& materialPath,
								   const std::string& entryPoint,
								   size_t index){
									   ofs
										   << "#include \"../commonDefine.h\"\n"
										   << "#include \"../common.hlsl\"\n"
										   << "#include \"../Material/MaterialDefine.hlsli\"\n"
										   << "#include \"../Material/DeferredFunc.hlsli\"\n"
										   << "#include \"../Material/MaterialFunc.hlsli\"\n"
										   << "#include \"../Material/" << materialPath << "\"\n"
										   << "\n"
										   << "float4 main(PS_IN In) : SV_Target\n"
										   << "{\n"
										   << "    if (GetMaterialID(In) != " << index << "u)\n"
										   << "    {\n"
										   << "        discard;\n"
										   << "    }\n"
										   << "\n"
										   << "    MaterialInput input = GetMaterialInput(In);\n"
										   << "    return " << entryPoint << "(input);\n"
										   << "}\n";
		};

	auto writeForwardSource = [&](std::ofstream& ofs,
								  const std::string& materialPath,
								  const std::string& entryPoint){
									  ofs
										  << "#include \"../commonDefine.h\"\n"
										  << "#include \"../common.hlsl\"\n"
										  << "#include \"../Material/MaterialDefine.hlsli\"\n"
										  << "#include \"../Material/FowardFunc.hlsli\"\n"
										  << "#include \"../Material/MaterialFunc.hlsli\"\n"
										  << "#include \"../Material/" << materialPath << "\"\n"
										  << "\n"
										  << "float4 main(PS_IN In) : SV_Target\n"
										  << "{\n"
										  << "    MaterialInput input = GetMaterialInput(In);\n"
										  << "    float4 Result = " << entryPoint << "(input);\n"
										  << "\n"
										  << "    if (Result.a <= ALPHA_CLIP_THRESHOLD)\n"
										  << "    {\n"
										  << "        discard;\n"
										  << "    }\n"
										  << "\n"
										  << "    return Result;\n"
										  << "}\n";
		};

	auto writeDeferredDebugSource = [&](std::ofstream& ofs){
		ofs
			<< "#include \"../commonDefine.h\"\n"
			<< "#include \"../common.hlsl\"\n"
			<< "#include \"../Material/MaterialDefine.hlsli\"\n"
			<< "#include \"../Material/DeferredFunc.hlsli\"\n"
			<< "#include \"../Material/MaterialFunc.hlsli\"\n"
			<< "\n"
			<< "float4 main(PS_IN In) : SV_Target\n"
			<< "{\n"
			<< "    if (GetMaterialID(In) < " << ShaderMaterials.size() << "u)\n"
			<< "    {\n"
			<< "        discard;\n"
			<< "    }\n"
			<< "    return float4(1, 0, 1, 1);\n"
			<< "}\n";
		};

	auto writeForwardDebugSource = [&](std::ofstream& ofs){
		ofs
			<< "#include \"../commonDefine.h\"\n"
			<< "#include \"../common.hlsl\"\n"
			<< "\n"
			<< "float4 main(PS_IN In) : SV_Target\n"
			<< "{\n"
			<< "    return float4(1, 0, 1, 1);\n"
			<< "}\n";
		};

	auto compileHlslToCso = [&](const fs::path& hlslPath,
								const fs::path& csoPath,
								const std::string& entryPoint,
								std::string& errorOut) -> bool{
									UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
								#if defined(_DEBUG)
									flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
								#endif

									ID3DBlob* code = nullptr;
									ID3DBlob* errorBlob = nullptr;

									const HRESULT hr = D3DCompileFromFile(
										hlslPath.c_str(),
										nullptr,
										D3D_COMPILE_STANDARD_FILE_INCLUDE,
										entryPoint.c_str(),
										"ps_5_0",
										flags,
										0,
										&code,
										&errorBlob
									);

									if(FAILED(hr)){
										if(errorBlob){
											errorOut = static_cast<const char*>(errorBlob->GetBufferPointer());
											errorBlob->Release();
										}
										if(code){
											code->Release();
										}
										return false;
									}

									std::ofstream ofs(csoPath, std::ios::binary | std::ios::trunc);
									if(!ofs){
										errorOut = "Failed to create cso file.";
										if(code){
											code->Release();
										}
										if(errorBlob){
											errorBlob->Release();
										}
										return false;
									}

									ofs.write(
										reinterpret_cast<const char*>(code->GetBufferPointer()),
										static_cast<std::streamsize>(code->GetBufferSize())
									);

									if(!ofs){
										errorOut = "Failed to write cso file.";
										code->Release();
										if(errorBlob){
											errorBlob->Release();
										}
										return false;
									}

									code->Release();
									if(errorBlob){
										errorBlob->Release();
									}
									return true;
		};

	YAML::Node newCache;
	YAML::Node deferredSeq(YAML::NodeType::Sequence);
	YAML::Node forwardSeq(YAML::NodeType::Sequence);

	const std::string shaderModel = "ps_5_0";

	for(size_t i = 0; i < ShaderMaterials.size(); ++i){
		const auto& mat = ShaderMaterials[i];
		const fs::path materialPath = fs::path(mat.filePath);
		const std::string materialPathStr = materialPath.generic_string();

		// ============================================================
		// Deferred
		// ============================================================
		{
			const fs::path hlslPath = shaderDir / ("DeferredRenderingPS_" + std::to_string(i) + ".hlsl");
			const fs::path csoPath = shaderDir / ("DeferredRenderingPS_" + std::to_string(i) + ".cso");
			const std::string csoKey = csoPath.string();

			const auto deps = makeDeps(materialPath, false, false);
			const YAML::Node cachedEntry = getSeqEntry(cacheRoot["Deferred"], i);

			const bool canReuse = cacheMatches(
				cachedEntry,
				hlslPath,
				csoPath,
				mat.entryPoint,
				materialPathStr,
				shaderModel,
				deps
			);

			if(canReuse && i < oldDeferred.size() && oldDeferred[i]){
				DeferredPSList[i] = oldDeferred[i];
			} else{
				bool compiled = false;

				if(!canReuse){
					{
						std::ofstream ofs(hlslPath, std::ios::trunc);
						if(!ofs){
							OutputDebugStringA(("Failed to create " + hlslPath.string() + "\n").c_str());
							goto deferred_fallback;
						}
						writeDeferredSource(ofs, materialPathStr, mat.entryPoint, i);
					}

					std::string errorText;
					compiled = compileHlslToCso(hlslPath, csoPath, "main", errorText);
					if(!compiled){
						OutputDebugStringA(
							(std::string("DeferredRenderingPS_") + std::to_string(i) + " compile failed\n" + errorText + "\n").c_str()
						);
						goto deferred_fallback;
					}

					m_context->resource->Unload<PixelShaderData>(csoKey.c_str());
				}

				{
					auto newPS = m_context->resource->Load<PixelShaderData>(csoKey.c_str());
					if(newPS){
						DeferredPSList[i] = newPS;
					} else{
						OutputDebugStringA(("DeferredRenderingPS_" + std::to_string(i) + " load failed\n").c_str());
					}
				}

			deferred_fallback:
				if(!DeferredPSList[i] && i < oldDeferred.size() && oldDeferred[i]){
					DeferredPSList[i] = oldDeferred[i];
				}
			}

			if(DeferredPSList[i]){
				deferredSeq.push_back(makeEntry(hlslPath, csoPath, mat.entryPoint, materialPathStr, shaderModel, deps));
			}
		}

		// ============================================================
		// Forward
		// ============================================================
		{
			const fs::path hlslPath = shaderDir / ("ForwardRenderingPS_" + std::to_string(i) + ".hlsl");
			const fs::path csoPath = shaderDir / ("ForwardRenderingPS_" + std::to_string(i) + ".cso");
			const std::string csoKey = csoPath.string();

			const auto deps = makeDeps(materialPath, true, false);
			const YAML::Node cachedEntry = getSeqEntry(cacheRoot["Forward"], i);

			const bool canReuse = cacheMatches(
				cachedEntry,
				hlslPath,
				csoPath,
				mat.entryPoint,
				materialPathStr,
				shaderModel,
				deps
			);

			if(canReuse && i < oldForward.size() && oldForward[i]){
				ForwardPSList[i] = oldForward[i];
			} else{
				bool compiled = false;

				if(!canReuse){
					{
						std::ofstream ofs(hlslPath, std::ios::trunc);
						if(!ofs){
							OutputDebugStringA(("Failed to create " + hlslPath.string() + "\n").c_str());
							goto forward_fallback;
						}
						writeForwardSource(ofs, materialPathStr, mat.entryPoint);
					}

					std::string errorText;
					compiled = compileHlslToCso(hlslPath, csoPath, "main", errorText);
					if(!compiled){
						OutputDebugStringA(
							(std::string("ForwardRenderingPS_") + std::to_string(i) + " compile failed\n" + errorText + "\n").c_str()
						);
						goto forward_fallback;
					}

					m_context->resource->Unload<PixelShaderData>(csoKey.c_str());
				}

				{
					auto newPS = m_context->resource->Load<PixelShaderData>(csoKey.c_str());
					if(newPS){
						ForwardPSList[i] = newPS;
					} else{
						OutputDebugStringA(("ForwardRenderingPS_" + std::to_string(i) + " load failed\n").c_str());
					}
				}

			forward_fallback:
				if(!ForwardPSList[i] && i < oldForward.size() && oldForward[i]){
					ForwardPSList[i] = oldForward[i];
				}
			}

			if(ForwardPSList[i]){
				forwardSeq.push_back(makeEntry(hlslPath, csoPath, mat.entryPoint, materialPathStr, shaderModel, deps));
			}
		}
	}

	// ============================================================
	// Deferred Debug
	// ============================================================
	{
		const fs::path hlslPath = shaderDir / "DeferredRenderingPS_Debug.hlsl";
		const fs::path csoPath = shaderDir / "DeferredRenderingPS_Debug.cso";
		const std::string csoKey = csoPath.string();
		const auto deps = makeDeps(fs::path(), false, true);
		const YAML::Node cachedEntry = cacheRoot["DeferredDebug"];

		const bool canReuse = cacheMatches(
			cachedEntry,
			hlslPath,
			csoPath,
			"",
			"",
			shaderModel,
			deps
		);

		ShaderPtr debugPS;
		if(canReuse && oldDeferredDebug){
			debugPS = oldDeferredDebug;
		} else{
			if(!canReuse){
				{
					std::ofstream ofs(hlslPath, std::ios::trunc);
					if(ofs){
						writeDeferredDebugSource(ofs);
					} else{
						OutputDebugStringA(("Failed to create " + hlslPath.string() + "\n").c_str());
					}
				}

				std::string errorText;
				if(!compileHlslToCso(hlslPath, csoPath, "main", errorText)){
					OutputDebugStringA(("DeferredRenderingPS_Debug compile failed\n" + errorText + "\n").c_str());
				} else{
					m_context->resource->Unload<PixelShaderData>(csoKey.c_str());
					debugPS = m_context->resource->Load<PixelShaderData>(csoKey.c_str());
				}
			} else{
				debugPS = m_context->resource->Load<PixelShaderData>(csoKey.c_str());
			}

			if(!debugPS && oldDeferredDebug){
				debugPS = oldDeferredDebug;
			}
		}

		if(debugPS){
			DeferredPSList.push_back(debugPS);
			newCache["DeferredDebug"] = makeEntry(hlslPath, csoPath, "", "", shaderModel, deps);
		}
	}

	// ============================================================
	// Forward Debug
	// ============================================================
	{
		const fs::path hlslPath = shaderDir / "ForwardRenderingPS_Debug.hlsl";
		const fs::path csoPath = shaderDir / "ForwardRenderingPS_Debug.cso";
		const std::string csoKey = csoPath.string();
		const auto deps = makeDeps(fs::path(), true, false);
		const YAML::Node cachedEntry = cacheRoot["ForwardDebug"];

		const bool canReuse = cacheMatches(
			cachedEntry,
			hlslPath,
			csoPath,
			"",
			"",
			shaderModel,
			deps
		);

		ShaderPtr debugPS;
		if(canReuse && oldForwardDebug){
			debugPS = oldForwardDebug;
		} else{
			if(!canReuse){
				{
					std::ofstream ofs(hlslPath, std::ios::trunc);
					if(ofs){
						writeForwardDebugSource(ofs);
					} else{
						OutputDebugStringA(("Failed to create " + hlslPath.string() + "\n").c_str());
					}
				}

				std::string errorText;
				if(!compileHlslToCso(hlslPath, csoPath, "main", errorText)){
					OutputDebugStringA(("ForwardRenderingPS_Debug compile failed\n" + errorText + "\n").c_str());
				} else{
					m_context->resource->Unload<PixelShaderData>(csoKey.c_str());
					debugPS = m_context->resource->Load<PixelShaderData>(csoKey.c_str());
				}
			} else{
				debugPS = m_context->resource->Load<PixelShaderData>(csoKey.c_str());
			}

			if(!debugPS && oldForwardDebug){
				debugPS = oldForwardDebug;
			}
		}

		if(debugPS){
			ForwardPSDebug = debugPS;
			newCache["ForwardDebug"] = makeEntry(hlslPath, csoPath, "", "", shaderModel, deps);
		}
	}

	newCache["Version"] = 2;
	newCache["Deferred"] = deferredSeq;
	newCache["Forward"] = forwardSeq;

	try{
		YAML::Emitter emitter;
		emitter << newCache;

		std::ofstream ofs(cachePath, std::ios::trunc);
		if(ofs){
			ofs << emitter.c_str();
		}
	} catch(const std::exception& e){
		OutputDebugStringA((std::string("ShaderCompileCache.yaml write failed: ") + e.what() + "\n").c_str());
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
