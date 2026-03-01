#include "PlayerPass.h"
#include "System/Render/RenderSystem/renderSystem.h"
#include "sceneManager.h"
#include "../RenderPassContext.h"
#include "../../renderPhase.h"

#include "../ShadowMap/ShadowMapPass.h"
#include "../GBuffer/GBufferPass.h"
#include "../LightingPass/LightingPass.h"

#include "scene.h"
#include "System/Render/RenderSystem/Renderable/IRenderable.h"
#include "System/Render/RenderSystem/RenderTarget/renderTarget.h"
#include "Component/transformComponent.h"
#include "Registry/componentRegistry.h"

#include "Shader/commonDefine.h"

#include "Graphics/graphicsContext.h"
#include "Registry/systemRegistry.h"

#include "Component/CameraComponent.h"
#include "Component/LightComponent.h"

#include "System/Render/RenderSystem/Renderable/Model/RenderableModel.h"
#include "System/Render/RenderSystem/Renderable/BillBoard/RenderableBillBoard.h"
#include "System/Render/RenderSystem/Renderable/Mesh/RenderableMesh.h"
#include "System/Render/RenderSystem/Renderable/Particle/RenderableParticle.h"
#include "System/Render/RenderSystem/Renderable/Sprite/RenderableSprite.h"
#include "System/Render/RenderSystem/Renderable/Terrain/RenderableTerrain.h"
#include "System/Render/RenderSystem/Renderable/Wave/RenderableWave.h"
#include <queue>
#include <Component/outlineComponent.h>
#include <Component/bumpMapComponent.h>
#include <PhysX/PxScene.h>
#include <System/Physic/physicSystem.h>
#include <Component/textureComponent.h>
#include <Component/materialComponent.h>
#include <System/Render/RenderSystem/Renderable/Effect/RenderableEffect.h>
#include <Component/RenderLayerComponent.h>


void PlayerPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context) {

	m_renderSystem = renderSystem;
	m_context = context;

	m_RenderableVertexShader= m_context->resource->Load<VertexShaderData>("Asset\\Shader\\commonVS.cso");

	shadowMapPass = new ShadowMapPass();
	shadowMapPass->Initialize(
		renderSystem,
		context
	);

	gBufferPass = new GBufferPass();
	gBufferPass->Initialize(
		renderSystem,
		context
	);

	lightingPass = new LightingPass();
	lightingPass->Initialize(
		renderSystem,
		context
	);

	renderables.clear();
	renderables.push_back(renderSystem->GetRenderable<RenderableModel>());
	renderables.push_back(renderSystem->GetRenderable<RenderableBillBoard>());
	renderables.push_back(renderSystem->GetRenderable<RenderableMesh>());
	renderables.push_back(renderSystem->GetRenderable<RenderableParticle>());
	renderables.push_back(renderSystem->GetRenderable<RenderableSprite>());
	renderables.push_back(renderSystem->GetRenderable<RenderableTerrain>());
	renderables.push_back(renderSystem->GetRenderable<RenderableWave>());
	renderables.push_back(renderSystem->GetRenderable<RenderableEffect>());

	editorRenderTarget = new RenderTarget(
		context->PlayerScreenSize,
		context->graphics,
		RenderTargetType::RENDERTARGET_TYPE_COLOR
	);
}

void PlayerPass::Finalize() {

	m_RenderableVertexShader.reset();

	lightingPass->Finalize();
	delete lightingPass;
	lightingPass = nullptr;

	gBufferPass->Finalize();
	delete gBufferPass;
	gBufferPass = nullptr;

	shadowMapPass->Finalize();
	delete shadowMapPass;
	shadowMapPass = nullptr;

	delete editorRenderTarget;
	editorRenderTarget = nullptr;
}

void PlayerPass::Execute(const RenderPassContext& ctx) {

	//return;

	RenderPassContext passCtx = ctx;

	// コンテキストの取得
	GraphicsContext* graphics = m_context->graphics;
	GraphicsContext* graphicsContext = m_context->renderer->GetGraphicsContext();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();



	passCtx.passPhase = RenderPhase::PHASE_GBUFFER;
	gBufferPass->Execute(ctx);

	passCtx.passPhase = RenderPhase::PHASE_SHADOW;
	shadowMapPass->Execute(ctx);


	CameraBuffer CameraBuffer{};
	CameraBuffer.CameraPosition = ctx.CameraPosition;
	graphicsContext->SetCameraBuffer(CameraBuffer);
	graphicsContext->SetViewMatrix(ctx.viewMatrix);
	graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);

	passCtx.passPhase = RenderPhase::PHASE_LIGHTING;
	lightingPass->SetTextureSlot(gBufferPass, shadowMapPass, graphicsContext);
	lightingPass->Execute(ctx);

	float clearCol[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
	editorRenderTarget->Resize(ctx.screenSize, m_context->graphics);
	editorRenderTarget->Clear(m_context->graphics->GetDeviceContext(), clearCol);
	//m_context->graphics->GetDeviceContext()->OMSetRenderTargets(1, editorRenderTarget->rtv.GetAddressOf(), editorRenderTarget->dsv.Get());
	m_context->graphics->GetDeviceContext()->OMSetRenderTargets(1, lightingPass->pRenderTarget->rtv.GetAddressOf(), gBufferPass->pDepthTarget->dsv.Get());

	m_context->graphics->GetDeviceContext()->PSSetShaderResources(TextureSlot_ShadowMap, 1, shadowMapPass->shadowRenderTarget->srv.GetAddressOf());
	m_context->graphics->GetDeviceContext()->PSSetSamplers(1, 1, &shadowMapPass->shadowSampler);
	
	deviceContext->VSSetShader(m_RenderableVertexShader->m_VertexShader.Get(), nullptr, 0);
	deviceContext->IASetInputLayout(m_RenderableVertexShader->m_VertexLayout.Get());
	PixelShaderData* ps = m_renderSystem->GetForwardPS();
	deviceContext->PSSetShader(ps ? ps->m_PixelShader.Get() : nullptr, nullptr, 0);

	D3D11_VIEWPORT vp = {};
	vp.Width = ctx.screenSize.x;
	vp.Height = ctx.screenSize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	deviceContext->RSSetViewports(1, &vp);

	ID3D11ShaderResourceView* initialSRV = editorRenderTarget->srv.Get();
	initialSRV = lightingPass->pRenderTarget->srv.Get();

	//Effekseer
	//effectPass->Execute(ctx);

	graphicsContext->SetBlendMode(BlendMode::Alpha);

	// シェーダーセット

	passCtx.passPhase = RenderPhase::PHASE_FOWARD;

	for (int i = 0; i < (int)RenderLayer::MaxRenderLayer; i++) {
		if (ctx.renderLayerVisibility[i] == false) {
			continue;
		}

		if (i != (int)RenderLayer::OverlayUI && i != (int)RenderLayer::SortTransparent3D && i != (int)RenderLayer::Transparent3D) {
			continue;
		}
		// --- Transparent 用の一時バッファ ---
		std::vector<TransparentDrawItem> transparentList;
		std::vector<SpriteDrawItem> spriteList;

		for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){

			auto context = scene->GetSceneContext();
			std::vector<Entity> entities =
				context->component->FindEntitiesWithComponent<TransformComponent>();

			if(entities.empty()){
				continue;
			}

			for(Entity entity : entities){

				RenderLayer layer = scene->GetRenderLayerFromEntity(entity);
				if((int)layer != i){
					continue;
				}

				// SortTransparent3D の場合は一旦貯める
				if(layer == RenderLayer::SortTransparent3D){

					auto transform =
						context->component->GetComponent<TransformComponent>(entity);
					if(!transform){
						continue;
					}

					Vector3 worldPos = transform->GetWorldPosition(context->component);
					Vector3 diff = worldPos - Vector3(ctx.CameraPosition.x, ctx.CameraPosition.y, ctx.CameraPosition.z);

					TransparentDrawItem item;
					item.ref = EntityRef(entity, context);
					item.distanceSq = diff.dot(diff);
					transparentList.push_back(item);

				} else if (layer == RenderLayer::OverlayUI) {

					auto transform =
						context->component->GetComponent<TransformComponent>(entity);
					if (!transform) {
						continue;
					}

					SpriteDrawItem item;
					item.ref = EntityRef(entity, context);
					item.orderInLayer = 0;
					OrderInLayerComponent* layer = context->component->GetComponent<OrderInLayerComponent>(entity);
					if (layer) {
						item.orderInLayer = layer->order;
					}
					spriteList.push_back(item);

				} else {

					// 通常描画（不透明など）
					for(auto renderable : renderables){

						int materialID = 0;
						auto material =
							context->component->GetComponent<MaterialComponent>(entity);
						if(material){
							materialID = material->ShaderID;
						}

						ObjectInfo info;
						info.SceneID = (unsigned int)context;
						info.ObjectID = entity;
						info.ShaderID = materialID;
						m_context->graphics->SetObjectInfo(info);

						renderable->Execute(ctx, context, entity);
					}
				}
			}

		}

		// --- Z ソートして描画 ---
		if(!transparentList.empty()){

			m_context->graphics->SetDepthMode(DepthMode::ReadOnly);

			std::sort(
				transparentList.begin(),
				transparentList.end(),
				[](const TransparentDrawItem& a,
				   const TransparentDrawItem& b){
					   return a.distanceSq > b.distanceSq; // 遠い→近い
				});

			for(auto& item : transparentList){

				if(!item.ref.IsValid()) continue;
				Entity entity = item.ref.GetEntityID();
				SceneContext* itemCtx = item.ref.GetScene();

				for(auto renderable : renderables){

					int materialID = 0;
					auto material =
						itemCtx->component->GetComponent<MaterialComponent>(entity);
					if(material){
						materialID = material->ShaderID;
					}

					ObjectInfo info;
					info.SceneID = (unsigned int)itemCtx;
					info.ObjectID = entity;
					info.ShaderID = materialID;
					m_context->graphics->SetObjectInfo(info);

					renderable->Execute(ctx, itemCtx, entity);
				}
			}

			m_context->graphics->SetDepthMode(DepthMode::Write);
		}
		// --- Sprite 描画 ---
		if (!spriteList.empty()) {

			std::sort(
				spriteList.begin(),
				spriteList.end(),
				[](const SpriteDrawItem& a,
				   const SpriteDrawItem& b) {
				return a.orderInLayer > b.orderInLayer; // 遠い→近い
			});

			for (auto& item : spriteList) {

				if(!item.ref.IsValid()) continue;
				Entity entity = item.ref.GetEntityID();
				SceneContext* itemCtx = item.ref.GetScene();

				for (auto renderable : renderables) {

					int materialID = 0;
					auto material =
						itemCtx->component->GetComponent<MaterialComponent>(entity);
					if (material) {
						materialID = material->ShaderID;
					}

					ObjectInfo info;
					info.SceneID = (unsigned int)itemCtx;
					info.ObjectID = entity;
					info.ShaderID = materialID;
					m_context->graphics->SetObjectInfo(info);

					renderable->Execute(ctx, itemCtx, entity);
				}
			}
		}


	}

	ID3D11RenderTargetView* nullRTV[1] = {nullptr};
	deviceContext->OMSetRenderTargets(1, nullRTV, nullptr);

	std::vector<PostProcessNode> postNodes;
	std::unordered_map<int, int> effectIndexToPostNodeIndex; // CameraBuffer->postEffects idx → postNodes idx

	DirectX::XMFLOAT4 clearColor = { 0,0,0,1 };

	CameraComponent* CameraComponent = ctx.cameraData.CameraComponent;

	if (CameraComponent) {
		auto sortedIndices = CameraComponent->TopologicalSortPostEffects();

		for (int idx : sortedIndices) {
			if (idx < 0) continue; // -1/-2 は描画対象としてノード作らない

			auto& e = CameraComponent->postEffects[idx];
			if (!e.enabled || !e.ps || !e.vs ) continue;

			PostProcessNode node{};
			node.id = idx;
			node.shader.m_VS = e.vs->m_VertexShader;
			node.shader.m_PS = e.ps->m_PixelShader;
			node.shader.m_InputLayout = e.vs->m_VertexLayout;
			node.param = e.Param;

			e.ResizeTexture(graphics->GetDevice(), ctx.screenSize);
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
			for (auto& link : CameraComponent->postEffectLinks) {
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

		// SRV を全解除
		ID3D11ShaderResourceView* nullSRV[TextureSlot_Max] = {nullptr};
		deviceContext->PSSetShaderResources(0, TextureSlot_Max, nullSRV);

		// RTV を解除
		ID3D11RenderTargetView* nullRTV[1] = {nullptr};
		deviceContext->OMSetRenderTargets(1, nullRTV, nullptr);

		graphics->ApplyPostProcessChain(postNodes, initialSRV);

		result = graphics->m_CurrentSRV;
	} else {
		result = initialSRV;
	}
	{
		// SRV を全解除
		ID3D11ShaderResourceView* nullSRV[TextureSlot_Max] = {nullptr};
		deviceContext->PSSetShaderResources(0, TextureSlot_Max, nullSRV);

		// RTV を解除
		ID3D11RenderTargetView* nullRTV[1] = {nullptr};
		deviceContext->OMSetRenderTargets(1, nullRTV, nullptr);
	}
}

