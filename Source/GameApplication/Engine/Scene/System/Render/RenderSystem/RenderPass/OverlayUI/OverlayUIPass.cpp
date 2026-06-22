// =======================================================================
// 
// OverlayUIPass.cpp
// 
// =======================================================================
#include "OverlayUIPass.h"
#include "Shader/commonDefine.h"

#include "System/Render/RenderSystem/renderSystem.h"
#include "sceneManager.h"
#include "../RenderPassContext.h"
#include "../../renderPhase.h"
#include "../../renderLayer.h"

#include "../GBuffer/GBufferPass.h"
#include "../LightingPass/LightingPass.h"
#include "../ShadowMap/ShadowMapPass.h"

#include "scene.h"
#include "System/Render/RenderSystem/Renderable/IRenderable.h"
#include "System/Render/RenderSystem/RenderTarget/renderTarget.h"
#include "Component/transformComponent.h"
#include "Registry/componentRegistry.h"

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
#include "System/Render/RenderSystem/Renderable/Effect/RenderableEffect.h"

#include <algorithm>
#include <Component/outlineComponent.h>
#include <Component/materialComponent.h>
#include <Component/RenderLayerComponent.h>

void OverlayUIPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context) {

	m_renderSystem = renderSystem;
	m_context = context;
	m_VertexShader = m_context->resource->Load<VertexShaderData>("Asset\\Shader\\commonVS.cso");

	renderables.clear();
	renderables.push_back(renderSystem->GetRenderable<RenderableSprite>());
}

void OverlayUIPass::Finalize() {
	m_VertexShader.reset();

}

void OverlayUIPass::Execute(const RenderPassContext& ctx) {

	GraphicsContext* graphics = m_context->graphics;
	ID3D11DeviceContext* deviceContext = graphics->GetDeviceContext();

	// レンダーターゲット設定 (ライティング結果テクスチャに書き込む)
	deviceContext->OMSetRenderTargets(
		1,
		m_rtv,
		m_dsv->dsv.Get()
	);

	// シェーダーセット
	deviceContext->VSSetShader(m_VertexShader->m_VertexShader.Get(), nullptr, 0);
	deviceContext->IASetInputLayout(m_VertexShader->m_VertexLayout.Get());
	PixelShaderData* ps = m_renderSystem->GetForwardPSList()[0].get();
	deviceContext->PSSetShader(ps ? ps->m_PixelShader.Get() : nullptr, nullptr, 0);

	// ビューポート設定
	D3D11_VIEWPORT vp = {};
	vp.Width = ctx.screenSize.x;
	vp.Height = ctx.screenSize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	deviceContext->RSSetViewports(1, &vp);

	graphics->SetBlendMode(BlendMode::Alpha);

	if(ctx.renderLayerVisibility[RenderLayer::OverlayUI]){
		std::vector<const RenderPacket*> overlayPackets;
		const RenderPacketFrameBuffer& packetBuffer =
			m_renderSystem->GetRenderPacketBuffer();

		if(packetBuffer.IsReady()){
			for(const RenderPacket& packet : packetBuffer.Packets()){
				if(packet.layer != RenderLayer::OverlayUI) continue;
				if(!HasRenderPacketPass(packet.passMask, RenderPacketPassMask::Overlay)){
					continue;
				}
				overlayPackets.push_back(&packet);
			}
		}

		std::sort(
			overlayPackets.begin(),
			overlayPackets.end(),
			RenderPacketOverlayOrder
		);

		for(const RenderPacket* packet : overlayPackets){
			if(!packet) continue;
			SceneContext* sceneContext =
				m_context->sceneManager->GetContextFromID(packet->sceneContextID);
			if(!sceneContext) continue;

			IRenderable* renderable =
				m_renderSystem->GetRenderableForPacketKind(packet->kind);
			if(!renderable) continue;

			ObjectInfo info;
			info.SceneID = packet->sceneContextID;
			info.ObjectID = packet->entity;
			info.ShaderID = static_cast<int>(packet->materialKey);
			m_context->graphics->SetObjectInfo(info);
			renderable->Execute(ctx, sceneContext, packet->entity);
		}
	}

	// RTV を解除
	ID3D11RenderTargetView* nullRTV[1] = { nullptr };
	deviceContext->OMSetRenderTargets(1, nullRTV, nullptr);
}

void OverlayUIPass::SetInputs(ID3D11RenderTargetView** setRTV, RenderTarget* setDSV) {
	m_rtv = setRTV;
	m_dsv = setDSV;
}
