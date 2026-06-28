#include "GBufferPass.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "System/Render/RenderSystem/renderSystem.h"
#include "sceneManager.h"
#include "../RenderPassContext.h"
#include "System/Render/RenderSystem/Renderable/IRenderable.h"
#include "System/Render/RenderSystem/RenderTarget/renderTarget.h"
#include "Graphics/graphicsContext.h"
#include "Graphics/mainRenderer.h"
#include "Graphics/RHI/RHIService.h"
#include "Registry/systemRegistry.h"
#include "Resources/resourceService.h"
#include "Resources/Data/shaderData.h"
#include "Component/materialComponent.h"
#include "System/Render/RenderSystem/Renderable/Model/RenderableModel.h"
#include "System/Render/RenderSystem/Renderable/Terrain/RenderableTerrain.h"
#include "System/Render/RenderSystem/Renderable/Wave/RenderableWave.h"
#include "System/Render/RenderSystem/Renderable/BillBoard/RenderableBillBoard.h"
#include "System/Render/StaticBatch/StaticBatchD3D11GBufferTargetBinding.h"
#include "System/Render/StaticBatch/StaticBatchDrawSubmission.h"
#include "System/Render/StaticBatch/StaticBatchGroupVisibility.h"
#include "System/Render/StaticBatch/StaticBatchModelMaterialResolver.h"
#include "System/Render/StaticBatch/StaticBatchPacketReplacementSet.h"
#include "System/Render/StaticBatch/StaticBatchUploadSystem.h"

namespace {

bool IsPacketRangeValid(
	const StaticBatchPacketCacheEntry& group,
	std::span<const std::size_t> packetIndices,
	std::size_t packetCount
) noexcept {
	if(group.instanceCount == 0 ||
		group.firstInstance > packetIndices.size() ||
		group.instanceCount > packetIndices.size() - group.firstInstance){
		return false;
	}
	for(std::size_t offset = 0; offset < group.instanceCount; ++offset){
		if(packetIndices[group.firstInstance + offset] >= packetCount){
			return false;
		}
	}
	return true;
}

} // namespace

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

	auto bindRegularPipeline = [&](){
		deviceContext->VSSetShader(m_GBufferVertexShader->m_VertexShader.Get(), nullptr, 0);
		deviceContext->IASetInputLayout(m_GBufferVertexShader->m_VertexLayout.Get());
		deviceContext->PSSetShader(m_GBufferPixelShader->m_PixelShader.Get(), nullptr, 0);
		if(sampler){
			deviceContext->PSSetSamplers(0, 1, &sampler);
		}
	};
	bindRegularPipeline();
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

	StaticBatchPacketReplacementSet replacements;
	replacements.Begin(packetBuffer.Packets().size());

	auto submitStaticGroups = [&](){
		if(!m_context->sceneManager || !m_context->graphics ||
			pRenderTargets.size() !=
				StaticBatchD3D11GBufferTargetBinding::ColorTargetCount ||
			!pDepthTarget){
			return;
		}

		SystemRegistry* registry =
			m_context->sceneManager->GetSystemRegistry();
		StaticBatchUploadSystem* uploadSystem = registry
			? registry->GetSystem<StaticBatchUploadSystem>()
			: nullptr;
		if(!uploadSystem || !uploadSystem->IsPipelineReady() ||
			!uploadSystem->LastUploadSucceeded()){
			return;
		}

		RHI::RenderHardwareInterfaceService* rhiService =
			m_context->graphics->GetRHIService();
		RHI::IRHIDevice* rhiDevice = rhiService
			? rhiService->GetDevice()
			: nullptr;
		if(!rhiDevice ||
			rhiDevice->GetBackendType() != RHI::BackendType::Direct3D11){
			return;
		}

		const StaticBatchInstanceDataBuffer& source =
			packetBuffer.StaticBatchInstances();
		const StaticBatchPacketCache& packetCache =
			packetBuffer.StaticBatchCache();
		const StaticBatchGpuInstanceBuffer& gpuInstances =
			uploadSystem->GetGpuInstanceBuffer();
		if(!source.IsValid() || source.IsOverflowed() ||
			!packetCache.IsValid() || packetCache.IsOverflowed() ||
			!gpuInstances.CanSubmit() ||
			gpuInstances.UploadedSourceRevision() != source.SourceRevision()){
			return;
		}

		std::array<
			ID3D11RenderTargetView*,
			StaticBatchD3D11GBufferTargetBinding::ColorTargetCount
		> staticTargets{};
		for(std::size_t index = 0; index < staticTargets.size(); ++index){
			staticTargets[index] = pRenderTargets[index]->rtv.Get();
		}

		StaticBatchD3D11GBufferTargetBinding targetBinding;
		if(!targetBinding.Resolve(
			m_context->graphics->GetDevice(),
			staticTargets,
			pDepthTarget->dsv.Get()
		) || !targetBinding.Bind(deviceContext)){
			return;
		}

		RHI::CommandListCreateDesc commandDesc;
		commandDesc.queueType = RHI::CommandQueueType::Graphics;
		std::unique_ptr<RHI::IRHICommandList> commandList =
			rhiDevice->CreateCommandList(commandDesc);
		if(!commandList) return;

		RenderPacketCullingView cullingView;
		cullingView.camera = passContext.cameraData.ref.GetEntityID();
		cullingView.kind = passContext.cullingViewKind;
		cullingView.instanceID = passContext.cullingViewInstanceID;
		cullingView.viewProjection =
			passContext.viewMatrix * passContext.projectionMatrix;

		const std::span<const StaticBatchInstanceGroup> groups =
			source.Groups();
		const std::span<const std::size_t> packetIndices =
			packetCache.PacketIndices();
		std::vector<std::size_t> submittedGroups;
		submittedGroups.reserve(groups.size());

		commandList->Begin();
		for(std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex){
			const StaticBatchInstanceGroup& group = groups[groupIndex];
			if(group.instanceCount < 2 ||
				group.instanceCount >
					(static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)())) ||
				group.firstInstance >
					(static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)())) ||
				!IsPacketRangeValid(
					group,
					packetIndices,
					packetBuffer.Packets().size()
				)){
				continue;
			}

			const int layerIndex = static_cast<int>(group.key.layer);
			if(static_cast<unsigned>(layerIndex) >=
				static_cast<unsigned>(RenderLayer::MaxRenderLayer) ||
				!passContext.renderLayerVisibility[layerIndex]){
				continue;
			}

			const StaticBatchGroupVisibilityResult visibility =
				StaticBatchGroupVisibility::Evaluate(
					group,
					source.Instances(),
					packetBuffer.Packets(),
					m_renderSystem->GetCullingVisibility(),
					cullingView
				);
			if(!visibility.CanSubmitInstanced()) continue;

			const StaticBatchD3D11GeometryBinding* geometry =
				uploadSystem->GetGeometryBindingCache().FindByGroupIndex(
					groupIndex
				);
			if(!geometry || !geometry->IsReady() ||
				geometry->GeometryResourceKey() != group.key.geometryKey){
				continue;
			}

			const StaticBatchModelMaterialResolveResult material =
				StaticBatchModelMaterialResolver::Resolve(
					group,
					packetBuffer.Packets()
				);
			if(!material.IsEligible()) continue;

			graphics->SetMaterial(material.state.material);
			graphics->SetUVMatrixBuffer(material.state.uv);
			ObjectInfo objectInfo{};
			objectInfo.SceneID = group.sceneContextID;
			objectInfo.ObjectID = 0;
			objectInfo.ShaderID = material.state.shaderID;
			graphics->SetObjectInfo(objectInfo);

			ID3D11ShaderResourceView* diffuseTexture =
				material.state.diffuseTexture;
			deviceContext->PSSetShaderResources(
				TextureSlot_Albedo,
				1,
				&diffuseTexture
			);

			StaticBatchDrawSubmissionDesc draw;
			draw.pipeline =
				uploadSystem->GetPipelineResources().PipelineState();
			draw.vertexBuffer = geometry->VertexBuffer();
			draw.indexBuffer = geometry->IndexBuffer();
			draw.instanceBuffer = gpuInstances.Buffer();
			draw.vertexStride = geometry->VertexStride();
			draw.indexFormat = geometry->IndexFormat();
			draw.indexCount = geometry->IndexCount();
			draw.instanceCount =
				static_cast<std::uint32_t>(group.instanceCount);
			draw.firstInstance =
				static_cast<std::uint32_t>(group.firstInstance);
			if(!SubmitStaticBatchDraw(*commandList, draw)) continue;

			submittedGroups.push_back(groupIndex);
		}
		commandList->End();

		if(submittedGroups.empty()) return;
		RHI::IRHICommandQueue* queue =
			rhiDevice->GetQueue(RHI::CommandQueueType::Graphics);
		if(!queue) return;

		std::array<RHI::IRHICommandList*, 1> commandLists{
			commandList.get()
		};
		RHI::QueueSubmitDesc submitDesc;
		submitDesc.commandLists = commandLists;
		if(!queue->Submit(submitDesc)) return;

		for(const std::size_t groupIndex : submittedGroups){
			replacements.AddGroup(groups[groupIndex], packetIndices);
		}
	};

	submitStaticGroups();
	bindRegularPipeline();

	for(std::size_t packetIndex = 0;
		packetIndex < packetBuffer.Packets().size();
		++packetIndex){
		if(replacements.Contains(packetIndex)) continue;
		const RenderPacket& packet = packetBuffer.Packets()[packetIndex];
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
