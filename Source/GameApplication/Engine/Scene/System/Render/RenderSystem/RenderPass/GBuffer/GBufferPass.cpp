#include "GBufferPass.h"
#include "System/Render/RenderSystem/renderSystem.h"
#include "sceneManager.h"
#include "../RenderPassContext.h"
#include "../../renderPhase.h"

#include "scene.h"
#include "System/Render/RenderSystem/Renderable/IRenderable.h"
#include "System/Render/RenderSystem/RenderTarget/renderTarget.h"
#include "Component/transformComponent.h"
#include "Registry/componentRegistry.h"

#include "Graphics/graphicsContext.h"

#include <System/Render/RenderSystem/Renderable/Model/RenderableModel.h>
#include <System/Render/RenderSystem/Renderable/Terrain/RenderableTerrain.h>
#include <System/Render/RenderSystem/Renderable/Wave/RenderableWave.h>
#include <System/Render/RenderSystem/Renderable/BillBoard/RenderableBillBoard.h>

#include "Resources/resourceService.h"
#include "Resources/Data/shaderData.h"
#include <ImGui/imgui_impl_dx11.h>
#include <Component/materialComponent.h>

using namespace DirectX;

void GBufferPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context) {

	m_renderSystem = renderSystem;
	m_context = context;

	m_GBufferVertexShader = m_context->resource->Load<VertexShaderData>("Asset\\Shader\\commonVS.cso");
	m_GBufferPixelShader = m_context->resource->Load<PixelShaderData>("Asset\\Shader\\GBufferPS.cso");

	// ----- Sampler -----
	D3D11_SAMPLER_DESC samp = {};
	samp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samp.AddressU = samp.AddressV = samp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samp.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	context->graphics->GetDevice()->CreateSamplerState(&samp, &sampler);

	Vector2 size = Vector2((float)context->graphics->m_width, (float)context->graphics->m_height);

	// ----- GBuffer RenderTargets -----
	pRenderTargets.clear();
	pRenderTargets.resize(GBufferSlot_Max);

	pRenderTargets[GBufferSlot_Albedo] =
		new RenderTarget(size, context->graphics, RENDERTARGET_TYPE_COLOR_NO_DSV);
	pRenderTargets[GBufferSlot_Normal] =
		new RenderTarget(size, context->graphics, RENDERTARGET_TYPE_COLOR_NO_DSV);
	pRenderTargets[GBufferSlot_Position] =
		new RenderTarget(size, context->graphics, RENDERTARGET_TYPE_COLOR_NO_DSV);
	pRenderTargets[GBufferSlot_Material] =
		new RenderTarget(size, context->graphics, RENDERTARGET_TYPE_COLOR_NO_DSV);
	pRenderTargets[GBufferSlot_Emissive] =
		new RenderTarget(size, context->graphics, RENDERTARGET_TYPE_COLOR_NO_DSV);
	pRenderTargets[GBufferSlot_Param] =
		new RenderTarget(size, context->graphics, RENDERTARGET_TYPE_UINT4);

	// Depth は別管理
	pDepthTarget =
		new RenderTarget(size, context->graphics, RENDERTARGET_TYPE_DEPTH);

	// ----- Renderables -----
	renderables.clear();
	renderables.push_back(renderSystem->GetRenderable<RenderableModel>());
	renderables.push_back(renderSystem->GetRenderable<RenderableTerrain>());
	renderables.push_back(renderSystem->GetRenderable<RenderableWave>());
	renderables.push_back(renderSystem->GetRenderable<RenderableBillBoard>());
}

void GBufferPass::Finalize() {

	m_GBufferPixelShader.reset();
	m_GBufferVertexShader.reset();

	if (sampler) {
		sampler->Release();
		sampler = nullptr;
	}

	for (auto rt : pRenderTargets) {
		delete rt;
		rt = nullptr;
	}

	delete pDepthTarget;
	pDepthTarget = nullptr;
}

void GBufferPass::Execute(const RenderPassContext& ctx) {

	ID3D11DeviceContext* dc = m_context->graphics->GetDeviceContext();
	GraphicsContext* gc = m_context->renderer->GetGraphicsContext();

	dc->VSSetShader(m_GBufferVertexShader->m_VertexShader.Get(), nullptr, 0);
	dc->IASetInputLayout(m_GBufferVertexShader->m_VertexLayout.Get());
	dc->PSSetShader(m_GBufferPixelShader->m_PixelShader.Get(), nullptr, 0);

	gc->SetBlendMode(BlendMode::None);

	// ----- Clear -----
	float clearColor[4] = { 0,0,0,0 };

	// ----- Resize & Clear -----
	for (int i = 0; i < GBufferSlot_Max; i++) {
		pRenderTargets[i]->Resize(ctx.screenSize, m_context->graphics);

		// UINT RT は Clear しない
		if (pRenderTargets[i]->type != RENDERTARGET_TYPE_UINT4) {
			dc->ClearRenderTargetView(
				pRenderTargets[i]->rtv.Get(),
				clearColor
			);
		}
	}

	// Depth
	pDepthTarget->Resize(ctx.screenSize, m_context->graphics);
	dc->ClearDepthStencilView(
		pDepthTarget->dsv.Get(),
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
		1.0f, 0
	);

	// ----- OMSetRenderTargets -----
	ID3D11RenderTargetView* rtvs[GBufferSlot_Max];
	for (int i = 0; i < GBufferSlot_Max; i++) {
		rtvs[i] = pRenderTargets[i]->rtv.Get();
	}

	dc->OMSetRenderTargets(
		GBufferSlot_Max,
		rtvs,
		pDepthTarget->dsv.Get()
	);

	// ----- Context -----
	RenderPassContext newCtx = ctx;
	newCtx.passPhase = RenderPhase::PHASE_GBUFFER;
	newCtx.renderLayerVisibility[RenderLayer::SortTransparent3D] = false;
	newCtx.renderLayerVisibility[RenderLayer::Transparent3D] = false;
	newCtx.renderLayerVisibility[RenderLayer::OverlayUI] = false;

	// ----- CameraBuffer -----
	gc->SetCameraPosition(ctx.CameraPosition);
	gc->SetViewMatrix(ctx.viewMatrix);
	gc->SetProjectionMatrix(ctx.projectionMatrix);

	// ----- Viewport -----
	D3D11_VIEWPORT vp{};
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	vp.Width = ctx.screenSize.x;
	vp.Height = ctx.screenSize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	dc->RSSetViewports(1, &vp);

	// ----- Draw -----
	for (int layer = 0; layer < (int)RenderLayer::MaxRenderLayer; layer++) {

		if (!newCtx.renderLayerVisibility[layer]) continue;

		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {

			auto sctx = scene->GetSceneContext();
			auto entities = sctx->component->FindEntitiesWithComponent<TransformComponent>();
			if (entities.empty()) continue;

			for (Entity ent : entities) {

				if ((int)scene->GetRenderLayerFromEntity(ent) != layer) continue;

				for (auto r : renderables) {

					int materialID = 0;
					MaterialComponent* material =
						sctx->component->GetComponent<MaterialComponent>(ent);
					if(material){
						materialID = material->ShaderID;
					}

					ObjectInfo info;
					info.SceneID = (unsigned int)sctx;
					info.ObjectID = ent;
					info.ShaderID = materialID;
					m_context->graphics->SetObjectInfo(info);

					r->Execute(newCtx, sctx, ent);
				}
			}
		}
	}
	{
		return;
		// ================================
		// ImGui Debug Draw（暫定）
		// ================================

		// 1. バックバッファに戻す
		dc->OMSetRenderTargets(0, nullptr, nullptr);

		// 5. ImGui 描画
		ImGui::Begin("GBuffer Debug");

		ImGui::Text("GBuffer Debug View");

		// Albedo
		ImGui::Image(
			pRenderTargets[GBufferSlot_Albedo]->srv.Get(),
			ImVec2(256, ctx.screenSize.y / ctx.screenSize.x * 256.0f)
		);

		// Normal
		ImGui::Image(
			pRenderTargets[GBufferSlot_Normal]->srv.Get(),
			ImVec2(256, ctx.screenSize.y / ctx.screenSize.x * 256.0f)
		);

		// Position
		ImGui::Image(
			pRenderTargets[GBufferSlot_Position]->srv.Get(),
			ImVec2(256, ctx.screenSize.y / ctx.screenSize.x * 256.0f)
		);

		ImGui::End();
	}
}
