// =======================================================================
// 
// ForwardPass.cpp
// 
// =======================================================================
#include "ForwardPass.h"
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
#include "Registry/entityRegistry.h"

#include "Graphics/graphicsContext.h"
#include "Registry/systemRegistry.h"

#include "Component/CameraComponent.h"
#include "Component/LightComponent.h"

#include "Resources/Data/textureData.h"

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

void ForwardPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context) {

	m_renderSystem = renderSystem;
	m_context = context;

	m_VertexShader = m_context->resource->Load<VertexShaderData>("Asset\\Shader\\commonVS.cso");

	renderables.clear();
	renderables.push_back(renderSystem->GetRenderable<RenderableModel>());
	renderables.push_back(renderSystem->GetRenderable<RenderableBillBoard>());
	renderables.push_back(renderSystem->GetRenderable<RenderableMesh>());
	renderables.push_back(renderSystem->GetRenderable<RenderableParticle>());
	renderables.push_back(renderSystem->GetRenderable<RenderableSprite>());
	renderables.push_back(renderSystem->GetRenderable<RenderableTerrain>());
	renderables.push_back(renderSystem->GetRenderable<RenderableWave>());
	renderables.push_back(renderSystem->GetRenderable<RenderableEffect>());
}

void ForwardPass::Finalize() {
	m_VertexShader.reset();
}

void ForwardPass::SetInputs(LightingPass* lightingPass, GBufferPass* gBufferPass, ShadowMapPass* shadowMapPass) {
	m_lightingPass  = lightingPass;
	m_gBufferPass   = gBufferPass;
	m_shadowMapPass = shadowMapPass;
}

void ForwardPass::Execute(const RenderPassContext& ctx){

	GraphicsContext* graphics = m_context->graphics;
	ID3D11DeviceContext* deviceContext = graphics->GetDeviceContext();

	deviceContext->OMSetRenderTargets(
		1,
		m_lightingPass->pRenderTarget->rtv.GetAddressOf(),
		m_gBufferPass->pDepthTarget->dsv.Get()
	);

	deviceContext->PSSetShaderResources(TextureSlot_ShadowMap, 1, m_shadowMapPass->shadowRenderTarget->srv.GetAddressOf());
	deviceContext->PSSetSamplers(1, 1, &m_shadowMapPass->shadowSampler);

	if(m_lightingPass->m_EnvironmentMap && m_lightingPass->m_EnvironmentMap->pTexture){
		ID3D11ShaderResourceView* environmentSRV =
			m_lightingPass->m_EnvironmentMap->pTexture.Get();
		deviceContext->PSSetShaderResources(TextureSlot_EnvironmentMap, 1, &environmentSRV);
	}
	if(m_lightingPass->m_EnvMapSampler){
		deviceContext->PSSetSamplers(3, 1, &m_lightingPass->m_EnvMapSampler);
	}

	deviceContext->VSSetShader(m_VertexShader->m_VertexShader.Get(), nullptr, 0);
	deviceContext->IASetInputLayout(m_VertexShader->m_VertexLayout.Get());

	D3D11_VIEWPORT vp = {};
	vp.Width = ctx.screenSize.x;
	vp.Height = ctx.screenSize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	deviceContext->RSSetViewports(1, &vp);

	graphics->SetBlendMode(BlendMode::Alpha);
	graphics->SetDepthMode(DepthMode::ReadOnly);

	const auto& forwardPSList = m_renderSystem->GetForwardPSList();

	auto bindForwardPS = [&](int materialID){
		PixelShaderData* ps = nullptr;
		if(materialID >= 0 && materialID < (int)forwardPSList.size()){
			ps = forwardPSList[materialID].get();
		}
		if(!ps){
			ps = m_renderSystem->GetForwardPSDebug();
		}
		deviceContext->PSSetShader(ps ? ps->m_PixelShader.Get() : nullptr, nullptr, 0);
	};

	auto packetIsCurrent = [&](const RenderPacket& packet){
		if(packet.sceneContextID == 0 || !m_context->sceneManager){
			return false;
		}

		SceneContext* current =
			m_context->sceneManager->GetContextFromID(packet.sceneContextID);
		if(!current || current != packet.bindings.sceneContext){
			return false;
		}
		if(!current->entity || !current->entity->IsAlive(packet.entity)){
			return false;
		}
		return true;
	};

	auto drawPacket = [&](const RenderPacket& packet){
		if(!packetIsCurrent(packet)) return;

		IRenderable* renderable =
			m_renderSystem->GetRenderableForPacketKind(packet.kind);
		if(!renderable) return;

		const int materialID = static_cast<int>(packet.materialKey);
		bindForwardPS(materialID);

		ObjectInfo info;
		info.SceneID = packet.sceneContextID;
		info.ObjectID = packet.entity;
		info.ShaderID = materialID;
		m_context->graphics->SetObjectInfo(info);
		renderable->Execute(ctx, packet);
	};

	const RenderPacketFrameBuffer& packetBuffer =
		m_renderSystem->GetRenderPacketBuffer();
	if(packetBuffer.IsReady()){
		std::vector<RenderPacketViewItem> sortedPackets;

		for(const RenderPacket& packet : packetBuffer.Packets()){
			if(!packetIsCurrent(packet)) continue;

			const float alpha = packet.bindings.material
				? packet.bindings.material->Material.BaseColor.w
				: 1.0f;
			const bool materialNeedsAlphaBlend = alpha < 0.999f;

			RenderLayer effectiveLayer = packet.layer;
			bool useForward =
				HasRenderPacketPass(packet.passMask, RenderPacketPassMask::Forward);

			// Opaque指定でもMaterial Alphaが1未満なら、Deferredではなく
			// Forwardの距離Sort付き半透明経路として扱う。
			if(materialNeedsAlphaBlend &&
			   packet.kind != RenderPacketKind::Sprite &&
			   (packet.layer == RenderLayer::Opaque3D ||
			    packet.layer == RenderLayer::Background2D)){
				effectiveLayer = RenderLayer::SortTransparent3D;
				useForward = HasRenderPacketPass(
					RenderPacketPassesForKind(packet.kind),
					RenderPacketPassMask::Forward
				);
			}

			if(!useForward) continue;

			const int layerIndex = static_cast<int>(effectiveLayer);
			if(static_cast<unsigned>(layerIndex) >=
			   static_cast<unsigned>(RenderLayer::MaxRenderLayer)){
				continue;
			}
			if(!ctx.renderLayerVisibility[layerIndex]) continue;

			if(effectiveLayer != RenderLayer::SortTransparent3D){
				drawPacket(packet);
				continue;
			}

			const float dx = packet.transform.worldPosition[0] - ctx.CameraPosition.x;
			const float dy = packet.transform.worldPosition[1] - ctx.CameraPosition.y;
			const float dz = packet.transform.worldPosition[2] - ctx.CameraPosition.z;
			sortedPackets.push_back({&packet, dx * dx + dy * dy + dz * dz});
		}

		std::sort(
			sortedPackets.begin(),
			sortedPackets.end(),
			RenderPacketBackToFront
		);
		for(const RenderPacketViewItem& item : sortedPackets){
			if(item.packet) drawPacket(*item.packet);
		}
	}

	graphics->SetDepthMode(DepthMode::Write);
	ID3D11RenderTargetView* nullRTV[1] = {nullptr};
	deviceContext->OMSetRenderTargets(1, nullRTV, nullptr);
}
