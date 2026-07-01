#include "GBufferPass.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
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
#include "System/Render/Culling/CullingVisibilityBuilder.h"
#include "System/Render/Culling/RenderPacketViewCulling.h"
#include "System/Render/RenderSystem/Renderable/Model/RenderableModel.h"
#include "System/Render/RenderSystem/Renderable/Terrain/RenderableTerrain.h"
#include "System/Render/RenderSystem/Renderable/Wave/RenderableWave.h"
#include "System/Render/RenderSystem/Renderable/BillBoard/RenderableBillBoard.h"
#include "System/Render/StaticBatch/StaticBatchD3D11GBufferTargetBinding.h"
#include "System/Render/StaticBatch/StaticBatchDrawSubmission.h"
#include "System/Render/StaticBatch/StaticBatchModelMaterialResolver.h"
#include "System/Render/StaticBatch/StaticBatchPacketReplacementSet.h"
#include "System/Render/StaticBatch/StaticBatchUploadSystem.h"

namespace {

constexpr UINT MaterialStencilReadWriteMask = 0xffu;

UINT ResolveMaterialStencilRef(std::uint64_t materialKey) noexcept {
	return static_cast<UINT>(materialKey & MaterialStencilReadWriteMask);
}

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

void HashViewRevision(std::uint64_t& hash, std::uint64_t value) noexcept {
	hash ^= value + 0x9e3779b97f4a7c15ull +
		(hash << 6) + (hash >> 2);
}

std::uint64_t MakeStaticBatchViewRevision(
	const CullingVisibilitySet& visibility,
	const RenderPacketCullingView& view
) noexcept {
	std::uint64_t hash = visibility.FrameSerial();
	HashViewRevision(hash, view.camera.GetPackedValue());
	HashViewRevision(hash, static_cast<std::uint64_t>(view.kind));
	HashViewRevision(hash, view.instanceID);
	return hash == 0 ? 1 : hash;
}

struct StaticBatchVisibleStoragePolicy {
	std::size_t reserveCount = 0;
	bool allowRuntimeGrowth = true;
	bool valid = true;
};

StaticBatchVisibleStoragePolicy ResolveVisibleStoragePolicy(
	std::span<const StaticBatchInstanceGroup> groups,
	std::span<const RenderPacket> packets
) noexcept {
	StaticBatchVisibleStoragePolicy policy;
	std::vector<SceneContext*> scenes;
	scenes.reserve(groups.size());

	for(const StaticBatchInstanceGroup& group : groups){
		if(group.representativePacketIndex >= packets.size()){
			policy.valid = false;
			return policy;
		}

		SceneContext* context =
			packets[group.representativePacketIndex].bindings.sceneContext;
		if(!context){
			policy.valid = false;
			return policy;
		}
		if(std::find(scenes.begin(), scenes.end(), context) != scenes.end()){
			continue;
		}
		scenes.push_back(context);

		const std::size_t reserve =
			static_cast<std::size_t>(context->storageConfig.staticBatchReserve);
		if(reserve >
			(std::numeric_limits<std::size_t>::max)() - policy.reserveCount){
			policy.valid = false;
			return policy;
		}
		policy.reserveCount += reserve;
		policy.allowRuntimeGrowth =
			policy.allowRuntimeGrowth &&
			context->storageConfig.allowRuntimeGrowth;
	}
	return policy;
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

	D3D11_DEPTH_STENCIL_DESC stencilDesc{};
	stencilDesc.DepthEnable = TRUE;
	stencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	stencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	stencilDesc.StencilEnable = TRUE;
	stencilDesc.StencilReadMask = MaterialStencilReadWriteMask;
	stencilDesc.StencilWriteMask = MaterialStencilReadWriteMask;
	stencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	stencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
	stencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc.BackFace = stencilDesc.FrontFace;
	context->graphics->GetDevice()->CreateDepthStencilState(
		&stencilDesc,
		m_materialStencilWriteState.ReleaseAndGetAddressOf()
	);

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
	if(m_context && m_context->graphics){
		RHI::RenderHardwareInterfaceService* rhiService =
			m_context->graphics->GetRHIService();
		RHI::IRHIDevice* rhiDevice = rhiService
			? rhiService->GetDevice()
			: nullptr;
		if(rhiDevice){
			m_staticBatchVisibleGpuInstances.Release(*rhiDevice);
		}
	}
	m_materialStencilWriteState.Reset();
	m_staticBatchVisibleInstances.Reset();
	m_staticBatchTelemetry.Reset();
	m_GBufferPixelShader.reset();
	m_GBufferVertexShader.reset();
	if(sampler){ sampler->Release(); sampler = nullptr; }
	for(RenderTarget* target : pRenderTargets){ delete target; }
	pRenderTargets.clear();
	delete pDepthTarget;
	pDepthTarget = nullptr;
}

void GBufferPass::Execute(const RenderPassContext& context){
	m_staticBatchTelemetry.Reset();

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
	deviceContext->OMSetDepthStencilState(m_materialStencilWriteState.Get(), 0);

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

	const StaticBatchInstanceDataBuffer& staticSource =
		packetBuffer.StaticBatchInstances();
	m_staticBatchTelemetry.sourceRevision = staticSource.SourceRevision();
	m_staticBatchTelemetry.candidateGroupCount = staticSource.Groups().size();

	StaticBatchPacketReplacementSet replacements;
	replacements.Begin(packetBuffer.Packets().size());

	auto submitStaticGroups = [&](){
		if(!m_context->sceneManager || !m_context->graphics ||
			pRenderTargets.size() !=
				StaticBatchD3D11GBufferTargetBinding::ColorTargetCount ||
			!pDepthTarget){
			++m_staticBatchTelemetry.targetBindingFailureCount;
			return;
		}

		SystemRegistry* registry =
			m_context->sceneManager->GetSystemRegistry();
		StaticBatchUploadSystem* uploadSystem = registry
			? registry->GetSystem<StaticBatchUploadSystem>()
			: nullptr;
		if(!uploadSystem || !uploadSystem->IsPipelineReady()){
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

		const StaticBatchPacketCache& packetCache =
			packetBuffer.StaticBatchCache();
		if(!staticSource.IsValid() || staticSource.IsOverflowed() ||
			!packetCache.IsValid() || packetCache.IsOverflowed()){
			return;
		}

		const StaticBatchVisibleStoragePolicy storagePolicy =
			ResolveVisibleStoragePolicy(
				staticSource.Groups(),
				packetBuffer.Packets()
			);
		if(!storagePolicy.valid){
			++m_staticBatchTelemetry.visibilitySelectionFailureCount;
			return;
		}

		RenderPacketCullingView cullingView;
		cullingView.camera = passContext.cameraData.ref.GetEntityID();
		cullingView.kind = passContext.cullingViewKind;
		cullingView.instanceID = passContext.cullingViewInstanceID;
		cullingView.viewProjection =
			passContext.viewMatrix * passContext.projectionMatrix;

		const CullingVisibilitySet& visibility =
			m_renderSystem->GetCullingVisibility();
		const std::uint64_t viewRevision =
			MakeStaticBatchViewRevision(visibility, cullingView);
		m_staticBatchVisibleInstances.Reserve(
			storagePolicy.reserveCount,
			storagePolicy.reserveCount
		);
		if(!m_staticBatchVisibleInstances.Build(
			staticSource.Groups(),
			staticSource.Instances(),
			packetCache.PacketIndices(),
			staticSource.SourceRevision(),
			viewRevision,
			storagePolicy.allowRuntimeGrowth,
			[&](
				const StaticBatchInstanceData& instance,
				std::size_t
			){
				SceneContext* sceneContext =
					m_context->sceneManager->GetContextFromID(
						instance.sceneContextID
					);
				if(!sceneContext || !sceneContext->component ||
					instance.entityIndex == 0){
					return false;
				}
				const Entity entity(
					instance.entityIndex,
					instance.entityGeneration
				);
				const CullingViewKey viewKey =
					RenderPacketViewCulling::MakeViewKey(
						instance.sceneContextID,
						cullingView
					);
				return CullingVisibilityBuilder::ShouldRender(
					visibility,
					viewKey,
					entity,
					*sceneContext->component
				);
			}
		)){
			++m_staticBatchTelemetry.visibilitySelectionFailureCount;
			return;
		}

		const StaticBatchVisibleInstanceBufferTelemetry visibilityTelemetry =
			m_staticBatchVisibleInstances.Telemetry();
		m_staticBatchTelemetry.visibleInstanceCount =
			visibilityTelemetry.visibleInstanceCount;
		m_staticBatchTelemetry.culledInstanceCount =
			visibilityTelemetry.culledInstanceCount;
		m_staticBatchTelemetry.culledGroupCount =
			visibilityTelemetry.allCulledGroupCount;
		m_staticBatchTelemetry.compactedMixedGroupCount =
			visibilityTelemetry.mixedGroupCount;
		if(m_staticBatchVisibleInstances.Instances().empty()) return;
		if(!m_staticBatchVisibleGpuInstances.Reserve(
			*rhiDevice,
			storagePolicy.reserveCount
		)){
			++m_staticBatchTelemetry.drawFailureCount;
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
			++m_staticBatchTelemetry.targetBindingFailureCount;
			return;
		}
		deviceContext->OMSetDepthStencilState(m_materialStencilWriteState.Get(), 0);

		RHI::CommandListCreateDesc commandDesc;
		commandDesc.queueType = RHI::CommandQueueType::Graphics;
		std::unique_ptr<RHI::IRHICommandList> commandList =
			rhiDevice->CreateCommandList(commandDesc);
		if(!commandList){
			++m_staticBatchTelemetry.drawFailureCount;
			return;
		}

		const std::span<const StaticBatchInstanceGroup> groups =
			m_staticBatchVisibleInstances.Groups();
		const std::span<const std::size_t> sourceGroupIndices =
			m_staticBatchVisibleInstances.SourceGroupIndices();
		const std::span<const std::size_t> packetIndices =
			m_staticBatchVisibleInstances.PacketIndices();
		if(sourceGroupIndices.size() != groups.size()){
			++m_staticBatchTelemetry.visibilitySelectionFailureCount;
			return;
		}

		std::vector<std::size_t> submittedGroups;
		submittedGroups.reserve(groups.size());

		commandList->Begin();
		const bool uploadSucceeded =
			m_staticBatchVisibleGpuInstances.Upload(
				*rhiDevice,
				*commandList,
				m_staticBatchVisibleInstances.Instances(),
				m_staticBatchVisibleInstances.SelectionRevision(),
				storagePolicy.allowRuntimeGrowth
			);
		if(!uploadSucceeded){
			commandList->End();
			++m_staticBatchTelemetry.drawFailureCount;
			return;
		}
		m_staticBatchTelemetry.instanceUploadReady = true;

		for(std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex){
			const StaticBatchInstanceGroup& group = groups[groupIndex];
			if(group.instanceCount < 2){
				++m_staticBatchTelemetry.singleInstanceFallbackCount;
				continue;
			}
			if(group.instanceCount >
					static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)()) ||
				group.firstInstance >
					static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)()) ||
				!IsPacketRangeValid(
					group,
					packetIndices,
					packetBuffer.Packets().size()
				)){
				++m_staticBatchTelemetry.packetRangeFallbackCount;
				continue;
			}

			const int layerIndex = static_cast<int>(group.key.layer);
			if(static_cast<unsigned>(layerIndex) >=
				static_cast<unsigned>(RenderLayer::MaxRenderLayer) ||
				!passContext.renderLayerVisibility[layerIndex]){
				++m_staticBatchTelemetry.layerFallbackCount;
				continue;
			}

			const std::size_t sourceGroupIndex = sourceGroupIndices[groupIndex];
			const StaticBatchD3D11GeometryBinding* geometry =
				uploadSystem->GetGeometryBindingCache().FindByGroupIndex(
					sourceGroupIndex
				);
			if(!geometry || !geometry->IsReady() ||
				geometry->GeometryResourceKey() != group.key.geometryKey){
				++m_staticBatchTelemetry.geometryFallbackCount;
				continue;
			}

			const StaticBatchModelMaterialResolveResult material =
				StaticBatchModelMaterialResolver::Resolve(
					group,
					packetBuffer.Packets()
				);
			if(!material.IsEligible()){
				++m_staticBatchTelemetry.materialFallbackCount;
				continue;
			}

			graphics->SetMaterial(material.state.material);
			graphics->SetUVMatrixBuffer(material.state.uv);
			ObjectInfo objectInfo{};
			objectInfo.SceneID = group.sceneContextID;
			objectInfo.ObjectID = 0;
			objectInfo.ShaderID = material.state.shaderID;
			graphics->SetObjectInfo(objectInfo);
			deviceContext->OMSetDepthStencilState(
				m_materialStencilWriteState.Get(),
				ResolveMaterialStencilRef(material.state.shaderID)
			);

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
			draw.instanceBuffer = m_staticBatchVisibleGpuInstances.Buffer();
			draw.vertexStride = geometry->VertexStride();
			draw.indexFormat = geometry->IndexFormat();
			draw.indexCount = geometry->IndexCount();
			draw.instanceCount =
				static_cast<std::uint32_t>(group.instanceCount);
			draw.firstInstance =
				static_cast<std::uint32_t>(group.firstInstance);
			if(!SubmitStaticBatchDraw(*commandList, draw)){
				++m_staticBatchTelemetry.drawFailureCount;
				continue;
			}

			submittedGroups.push_back(groupIndex);
		}
		commandList->End();

		if(submittedGroups.empty()) return;
		RHI::IRHICommandQueue* queue =
			rhiDevice->GetQueue(RHI::CommandQueueType::Graphics);
		if(!queue){
			++m_staticBatchTelemetry.queueFailureCount;
			return;
		}

		std::array<RHI::IRHICommandList*, 1> commandLists{
			commandList.get()
		};
		RHI::QueueSubmitDesc submitDesc;
		submitDesc.commandLists = commandLists;
		if(!queue->Submit(submitDesc)){
			++m_staticBatchTelemetry.queueFailureCount;
			return;
		}

		for(const std::size_t groupIndex : submittedGroups){
			const StaticBatchInstanceGroup& group = groups[groupIndex];
			if(replacements.AddGroup(group, packetIndices)){
				++m_staticBatchTelemetry.submittedGroupCount;
				m_staticBatchTelemetry.submittedInstanceCount +=
					group.instanceCount;
			}
		}
	};

	submitStaticGroups();
	bindRegularPipeline();
	deviceContext->OMSetRenderTargets(GBufferSlot_Max, targets, pDepthTarget->dsv.Get());
	deviceContext->OMSetDepthStencilState(m_materialStencilWriteState.Get(), 0);

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
		deviceContext->OMSetDepthStencilState(
			m_materialStencilWriteState.Get(),
			ResolveMaterialStencilRef(packet.materialKey)
		);
		renderable->Execute(passContext, packet);
	}

	graphics->SetDepthMode(DepthMode::Write);
}
