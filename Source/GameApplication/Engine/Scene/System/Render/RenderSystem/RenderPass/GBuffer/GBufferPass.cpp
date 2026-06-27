#include "GBufferPass.h"
#include "System/Render/RenderSystem/renderSystem.h"
#include "sceneManager.h"
#include "../RenderPassContext.h"
#include "System/Render/RenderSystem/Renderable/IRenderable.h"
#include "System/Render/RenderSystem/RenderTarget/renderTarget.h"
#include "Graphics/graphicsContext.h"
#include "Graphics/mainRenderer.h"
#include "Resources/resourceService.h"
#include "Resources/Data/shaderData.h"
#include "Component/materialComponent.h"
#include "System/Render/RenderSystem/Renderable/Model/RenderableModel.h"
#include "System/Render/RenderSystem/Renderable/Terrain/RenderableTerrain.h"
#include "System/Render/RenderSystem/Renderable/Wave/RenderableWave.h"
#include "System/Render/RenderSystem/Renderable/BillBoard/RenderableBillBoard.h"

void GBufferPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context){
	m_renderSystem = renderSystem;
	m_context = context;
	m_GBufferVertexShader = m_context->resource->Load<VertexShaderData>("Asset\\Shader\\commonVS.cso");
	m_GBufferPixelShader = m_context->resource->Load<PixelShaderData>("Asset\\Shader\\GBufferPS.cso");

	D3D11_SAMPLER_DESC samplerDesc{};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	context->graphics->GetDevice()->CreateSamplerState(&samplerDesc, &sampler);

	const Vector2 size(
		static_cast<float>(context->graphics->m_width),
		static_cast<float>(context->graphics->m_height)
	);
	pRenderTargets.clear();
	pRenderTargets.resize(GBufferSlot_Max);
	pRenderTargets[GBufferSlot_Albedo] = new RenderTarget(size, context->graphics, RENDERTARGET_TYPE_COLOR_NO_DSV);
	pRenderTargets[GBufferSlot_Normal] = new RenderTarget(size, context->graphics, RENDERTARGET_TYPE_COLOR_NO_DSV);
	pRenderTargets[GBufferSlot_Position] = new RenderTarget(size, context->graphics, RENDERTARGET_TYPE_COLOR_NO_DSV);
	pRenderTargets[GBufferSlot_Material] = new RenderTarget(size, context->graphics, RENDERTARGET_TYPE_COLOR_NO_DSV);
	pRenderTargets[GBufferSlot_Emissive] = new RenderTarget(size, context->graphics, RENDERTARGET_TYPE_COLOR_NO_DSV);
	pRenderTargets[GBufferSlot_Param] = new RenderTarget(size, context->graphics, RENDERTARGET_TYPE_UINT4);
	pDepthTarget = new RenderTarget(size, context->graphics, RENDERTARGET_TYPE_DEPTH);

	renderables.clear();
	renderables.push_back(renderSystem->GetRenderable<RenderableModel>());
	renderables.push_back(renderSystem->GetRenderable<RenderableTerrain>());
	renderables.push_back(renderSystem->GetRenderable<RenderableWave>());
	renderables.push_back(renderSystem->GetRenderable<RenderableBillBoard>());
}

void GBufferPass::Finalize(){
	m_GBufferPixelShader.reset();
	m_GBufferVertexShader.reset();
	if(sampler){ sampler->Release(); sampler = nullptr; }
	for(RenderTarget* target : pRenderTargets){ delete target; }
	pRenderTargets.clear();
	delete pDepthTarget;
	pDepthTarget = nullptr;
}

void GBufferPass::Execute(const RenderPassContext& context){
	ID3D11DeviceContext* deviceContext = m_context->graphics->GetDeviceContext();
	GraphicsContext* graphics = m_context->renderer->GetGraphicsContext();
	deviceContext->VSSetShader(m_GBufferVertexShader->m_VertexShader.Get(), nullptr, 0);
	deviceContext->IASetInputLayout(m_GBufferVertexShader->m_VertexLayout.Get());
	deviceContext->PSSetShader(m_GBufferPixelShader->m_PixelShader.Get(), nullptr, 0);
	if(sampler){
		deviceContext->PSSetSamplers(0, 1, &sampler);
	}
	graphics->SetBlendMode(BlendMode::None);

	float clearColor[4] = {0, 0, 0, 0};
	for(int index = 0; index < GBufferSlot_Max; ++index){
		pRenderTargets[index]->Resize(context.screenSize, m_context->graphics);
		if(pRenderTargets[index]->type != RENDERTARGET_TYPE_UINT4){
			deviceContext->ClearRenderTargetView(pRenderTargets[index]->rtv.Get(), clearColor);
		}
	}
	pDepthTarget->Resize(context.screenSize, m_context->graphics);
	deviceContext->ClearDepthStencilView(
		pDepthTarget->dsv.Get(),
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
		1.0f,
		0
	);

	ID3D11RenderTargetView* targets[GBufferSlot_Max];
	for(int index = 0; index < GBufferSlot_Max; ++index){
		targets[index] = pRenderTargets[index]->rtv.Get();
	}
	deviceContext->OMSetRenderTargets(GBufferSlot_Max, targets, pDepthTarget->dsv.Get());

	RenderPassContext passContext = context;
	passContext.passPhase = RenderPhase::PHASE_GBUFFER;
	passContext.renderLayerVisibility[RenderLayer::SortTransparent3D] = false;
	passContext.renderLayerVisibility[RenderLayer::Transparent3D] = false;
	passContext.renderLayerVisibility[RenderLayer::OverlayUI] = false;

	graphics->SetCameraPosition(context.CameraPosition);
	graphics->SetViewMatrix(context.viewMatrix);
	graphics->SetProjectionMatrix(context.projectionMatrix);

	D3D11_VIEWPORT viewport{};
	viewport.Width = context.screenSize.x;
	viewport.Height = context.screenSize.y;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	deviceContext->RSSetViewports(1, &viewport);

	const RenderPacketFrameBuffer& packetBuffer = m_renderSystem->GetRenderPacketBuffer();
	if(!packetBuffer.IsReady()) return;

	for(const RenderPacket& packet : packetBuffer.Packets()){
		if(!HasRenderPacketPass(packet.passMask, RenderPacketPassMask::GBuffer)) continue;
		if(!m_renderSystem->ShouldRenderPacket(passContext, packet)) continue;
		if(packet.bindings.material &&
			packet.bindings.material->Material.BaseColor.w < 0.999f){
			continue;
		}

		const int layerIndex = static_cast<int>(packet.layer);
		if(static_cast<unsigned>(layerIndex) >=
			static_cast<unsigned>(RenderLayer::MaxRenderLayer)){
			continue;
		}
		if(!passContext.renderLayerVisibility[layerIndex]) continue;

		IRenderable* renderable =
			m_renderSystem->GetRenderableForPacketKind(packet.kind);
		if(!renderable) continue;

		ObjectInfo objectInfo;
		objectInfo.SceneID = packet.sceneContextID;
		objectInfo.ObjectID = packet.entity;
		objectInfo.ShaderID = static_cast<int>(packet.materialKey);
		m_context->graphics->SetObjectInfo(objectInfo);
		renderable->Execute(passContext, packet);
	}
}
