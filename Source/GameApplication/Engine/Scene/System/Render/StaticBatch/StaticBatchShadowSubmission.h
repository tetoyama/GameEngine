#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "Graphics/graphicsContext.h"
#include "System/Render/Culling/RenderPacketViewCulling.h"
#include "System/Render/RenderSystem/RenderPacket/RenderPacketBuffer.h"
#include "System/Render/StaticBatch/StaticBatchDrawSubmission.h"
#include "System/Render/StaticBatch/StaticBatchPacketReplacementSet.h"
#include "System/Render/StaticBatch/StaticBatchShadowGroupEligibility.h"
#include "System/Render/StaticBatch/StaticBatchShadowPixelState.h"
#include "System/Render/StaticBatch/StaticBatchShadowSubmissionTelemetry.h"
#include "System/Render/StaticBatch/StaticBatchUploadSystem.h"
#include "System/Render/StaticBatch/StaticBatchViewSelectionPolicy.h"

namespace StaticBatchShadowSubmission {

inline bool IsGroupMappingEquivalent(
	const StaticBatchInstanceGroup& sourceGroup,
	const StaticBatchPacketCacheEntry& cacheEntry
) noexcept {
	return sourceGroup.key == cacheEntry.key &&
		sourceGroup.sceneContextID == cacheEntry.sceneContextID &&
		sourceGroup.representativePacketIndex ==
			cacheEntry.representativePacketIndex &&
		sourceGroup.firstInstance == cacheEntry.firstInstance &&
		sourceGroup.instanceCount == cacheEntry.instanceCount;
}

class SubmissionTelemetryRecorder final {
public:
	SubmissionTelemetryRecorder(
		const StaticBatchUploadSystem& uploadSystem,
		StaticBatchShadowSubmissionTelemetry& telemetry
	) noexcept
		: m_uploadSystem(uploadSystem),
		  m_telemetry(telemetry) {
	}

	~SubmissionTelemetryRecorder(){
		m_uploadSystem.RecordShadowSubmissionTelemetry(m_telemetry);
	}

	SubmissionTelemetryRecorder(const SubmissionTelemetryRecorder&) = delete;
	SubmissionTelemetryRecorder& operator=(
		const SubmissionTelemetryRecorder&
	) = delete;

private:
	const StaticBatchUploadSystem& m_uploadSystem;
	StaticBatchShadowSubmissionTelemetry& m_telemetry;
};

inline StaticBatchShadowSubmissionTelemetry Submit(
	RHI::IRHIDevice& device,
	GraphicsContext& graphics,
	const RenderPacketFrameBuffer& frameBuffer,
	const StaticBatchUploadSystem& uploadSystem,
	const CullingVisibilitySet& visibility,
	const RenderPacketCullingView& cullingView,
	const bool* renderLayerVisibility,
	std::size_t renderLayerCount,
	StaticBatchPacketReplacementSet& replacements
){
	StaticBatchShadowSubmissionTelemetry telemetry;
	[[maybe_unused]] SubmissionTelemetryRecorder telemetryRecorder(
		uploadSystem,
		telemetry
	);
	replacements.Begin(frameBuffer.Packets().size());
	if(device.GetBackendType() != RHI::BackendType::Direct3D11 ||
		!renderLayerVisibility || !frameBuffer.IsReady() ||
		!uploadSystem.IsShadowPipelineReady() ||
		!RenderPacketViewCulling::HasStableViewIdentity(cullingView)){
		return telemetry;
	}

	const StaticBatchInstanceDataBuffer& source =
		frameBuffer.StaticBatchInstances();
	const StaticBatchPacketCache& cache = frameBuffer.StaticBatchCache();
	telemetry.candidateGroupCount = source.Groups().size();
	if(!source.IsValid() || source.IsOverflowed() ||
		!cache.IsValid() || cache.IsOverflowed() ||
		cache.SourceRevision() != source.SourceRevision() ||
		cache.Entries().size() != source.Groups().size() ||
		cache.PacketIndices().size() != source.Instances().size()){
		return telemetry;
	}

	const std::span<const RenderPacket> packets = frameBuffer.Packets();
	const std::span<const StaticBatchInstanceGroup> sourceGroups =
		source.Groups();
	const std::span<const StaticBatchPacketCacheEntry> cacheEntries =
		cache.Entries();
	const std::span<const std::size_t> sourcePacketIndices =
		cache.PacketIndices();
	for(const std::size_t packetIndex : sourcePacketIndices){
		if(packetIndex >= packets.size()){
			++telemetry.visibilityFailureCount;
			return telemetry;
		}
	}

	const StaticBatchViewSelectionStoragePolicy storagePolicy =
		StaticBatchViewSelectionPolicy::ResolveStorage(
			sourceGroups,
			packets
		);
	if(!storagePolicy.valid){
		++telemetry.visibilityFailureCount;
		return telemetry;
	}

	StaticBatchVisibleInstanceBuffer& visibleInstances =
		uploadSystem.GetShadowVisibleInstanceBuffer();
	visibleInstances.Reserve(
		storagePolicy.reserveCount,
		storagePolicy.reserveCount
	);
	if(!visibleInstances.Build(
		sourceGroups,
		source.Instances(),
		sourcePacketIndices,
		source.SourceRevision(),
		StaticBatchViewSelectionPolicy::MakeViewRevision(
			visibility,
			cullingView
		),
		storagePolicy.allowRuntimeGrowth,
		[&](const StaticBatchInstanceData&, std::size_t sourceIndex){
			return RenderPacketViewCulling::ShouldRender(
				visibility,
				cullingView,
				packets[sourcePacketIndices[sourceIndex]]
			);
		}
	)){
		++telemetry.visibilityFailureCount;
		return telemetry;
	}

	const StaticBatchVisibleInstanceBufferTelemetry visibilityTelemetry =
		visibleInstances.Telemetry();
	telemetry.visibleInstanceCount = visibilityTelemetry.visibleInstanceCount;
	telemetry.culledInstanceCount = visibilityTelemetry.culledInstanceCount;
	telemetry.fullyCulledGroupCount =
		visibilityTelemetry.allCulledGroupCount;
	telemetry.compactedMixedGroupCount = visibilityTelemetry.mixedGroupCount;
	if(visibleInstances.Instances().empty()) return telemetry;

	StaticBatchGpuInstanceBuffer& visibleGpuInstances =
		uploadSystem.GetShadowVisibleGpuInstanceBuffer();
	if(!visibleGpuInstances.Reserve(device, storagePolicy.reserveCount)){
		++telemetry.instanceUploadFailureCount;
		return telemetry;
	}

	StaticBatchShadowPixelState pixelBindings(
		graphics.GetDeviceContext(),
		TextureSlot_Albedo
	);
	RHI::CommandListCreateDesc commandDesc;
	commandDesc.queueType = RHI::CommandQueueType::Graphics;
	std::unique_ptr<RHI::IRHICommandList> commandList =
		device.CreateCommandList(commandDesc);
	if(!commandList){
		++telemetry.drawFailureCount;
		return telemetry;
	}

	const std::span<const StaticBatchInstanceGroup> groups =
		visibleInstances.Groups();
	const std::span<const std::size_t> sourceGroupIndices =
		visibleInstances.SourceGroupIndices();
	const std::span<const std::size_t> packetIndices =
		visibleInstances.PacketIndices();
	if(sourceGroupIndices.size() != groups.size()){
		++telemetry.visibilityFailureCount;
		return telemetry;
	}

	std::vector<std::size_t> submittedGroups;
	submittedGroups.reserve(groups.size());

	commandList->Begin();
	if(!visibleGpuInstances.Upload(
		device,
		*commandList,
		visibleInstances.Instances(),
		visibleInstances.SelectionRevision(),
		storagePolicy.allowRuntimeGrowth
	)){
		commandList->End();
		++telemetry.instanceUploadFailureCount;
		return telemetry;
	}

	for(std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex){
		const StaticBatchInstanceGroup& group = groups[groupIndex];
		const std::size_t sourceGroupIndex = sourceGroupIndices[groupIndex];
		if(sourceGroupIndex >= sourceGroups.size() ||
			sourceGroupIndex >= cacheEntries.size() ||
			!IsGroupMappingEquivalent(
				sourceGroups[sourceGroupIndex],
				cacheEntries[sourceGroupIndex]
			)){
			++telemetry.mappingFallbackCount;
			continue;
		}
		if(group.instanceCount < 2){
			++telemetry.singleInstanceFallbackCount;
			continue;
		}

		const StaticBatchShadowGroupEligibilityResult eligibility =
			StaticBatchShadowGroupEligibility::Resolve(
				group,
				packetIndices,
				packets
			);
		if(!eligibility.IsEligible()){
			++telemetry.eligibilityFallbackCount;
			continue;
		}

		const int layerIndex = static_cast<int>(group.key.layer);
		if(layerIndex < 0 ||
			static_cast<std::size_t>(layerIndex) >= renderLayerCount ||
			!renderLayerVisibility[layerIndex]){
			++telemetry.layerFallbackCount;
			continue;
		}

		const StaticBatchD3D11GeometryBinding* geometry =
			uploadSystem.GetGeometryBindingCache().FindByGroupIndex(
				sourceGroupIndex
			);
		if(!geometry || !geometry->IsReady() ||
			geometry->GeometryResourceKey() != group.key.geometryKey){
			++telemetry.geometryFallbackCount;
			continue;
		}

		if(eligibility.material.UsesDiffuseTexture() &&
			!pixelBindings.CanSampleDiffuseTexture()){
			++telemetry.textureBindingFallbackCount;
			continue;
		}
		if(!pixelBindings.BindDiffuseTexture(
			eligibility.material.diffuseTexture
		)){
			++telemetry.textureBindingFallbackCount;
			continue;
		}

		graphics.SetMaterial(eligibility.material.material);
		graphics.SetUVMatrixBuffer(eligibility.material.uv);

		StaticBatchDrawSubmissionDesc draw;
		draw.pipeline =
			uploadSystem.GetShadowPipelineResources().PipelineState();
		draw.vertexBuffer = geometry->VertexBuffer();
		draw.indexBuffer = geometry->IndexBuffer();
		draw.instanceBuffer = visibleGpuInstances.Buffer();
		draw.vertexStride = geometry->VertexStride();
		draw.indexFormat = geometry->IndexFormat();
		draw.indexCount = geometry->IndexCount();
		draw.instanceCount = static_cast<std::uint32_t>(group.instanceCount);
		draw.firstInstance = static_cast<std::uint32_t>(group.firstInstance);
		if(!SubmitStaticBatchDraw(*commandList, draw)){
			++telemetry.drawFailureCount;
			continue;
		}
		submittedGroups.push_back(groupIndex);
	}
	commandList->End();

	if(submittedGroups.empty()) return telemetry;
	RHI::IRHICommandQueue* queue =
		device.GetQueue(RHI::CommandQueueType::Graphics);
	if(!queue){
		++telemetry.queueFailureCount;
		return telemetry;
	}

	std::array<RHI::IRHICommandList*, 1> commandLists{
		commandList.get()
	};
	RHI::QueueSubmitDesc submitDesc;
	submitDesc.commandLists = commandLists;
	if(!queue->Submit(submitDesc)){
		++telemetry.queueFailureCount;
		return telemetry;
	}

	for(const std::size_t groupIndex : submittedGroups){
		const StaticBatchInstanceGroup& group = groups[groupIndex];
		if(!replacements.AddGroup(group, packetIndices)) continue;
		++telemetry.submittedGroupCount;
		telemetry.submittedInstanceCount += group.instanceCount;
	}
	return telemetry;
}

} // namespace StaticBatchShadowSubmission
