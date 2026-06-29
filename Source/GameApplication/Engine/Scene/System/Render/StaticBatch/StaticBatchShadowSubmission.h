#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "Graphics/graphicsContext.h"
#include "System/Render/RenderSystem/RenderPacket/RenderPacketBuffer.h"
#include "System/Render/StaticBatch/StaticBatchDrawSubmission.h"
#include "System/Render/StaticBatch/StaticBatchPacketReplacementSet.h"
#include "System/Render/StaticBatch/StaticBatchShadowGroupEligibility.h"
#include "System/Render/StaticBatch/StaticBatchUploadSystem.h"

struct StaticBatchShadowSubmissionTelemetry {
	std::size_t candidateGroupCount = 0;
	std::size_t submittedGroupCount = 0;
	std::size_t submittedInstanceCount = 0;
	std::size_t mappingFallbackCount = 0;
	std::size_t eligibilityFallbackCount = 0;
	std::size_t layerFallbackCount = 0;
	std::size_t geometryFallbackCount = 0;
	std::size_t drawFailureCount = 0;
	std::size_t queueFailureCount = 0;

	std::size_t EstimatedDrawCallReduction() const noexcept {
		return submittedInstanceCount > submittedGroupCount
			? submittedInstanceCount - submittedGroupCount
			: 0;
	}
};

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

inline StaticBatchShadowSubmissionTelemetry Submit(
	RHI::IRHIDevice& device,
	GraphicsContext& graphics,
	const RenderPacketFrameBuffer& frameBuffer,
	const StaticBatchUploadSystem& uploadSystem,
	const bool* renderLayerVisibility,
	std::size_t renderLayerCount,
	StaticBatchPacketReplacementSet& replacements
){
	StaticBatchShadowSubmissionTelemetry telemetry;
	replacements.Begin(frameBuffer.Packets().size());
	if(device.GetBackendType() != RHI::BackendType::Direct3D11 ||
		!renderLayerVisibility || !frameBuffer.IsReady() ||
		!uploadSystem.IsShadowPipelineReady()){
		return telemetry;
	}

	const StaticBatchInstanceDataBuffer& source =
		frameBuffer.StaticBatchInstances();
	const StaticBatchPacketCache& cache = frameBuffer.StaticBatchCache();
	const StaticBatchGpuInstanceBuffer& gpuInstances =
		uploadSystem.GetGpuInstanceBuffer();
	telemetry.candidateGroupCount = source.Groups().size();
	if(!source.IsValid() || source.IsOverflowed() ||
		!cache.IsValid() || cache.IsOverflowed() ||
		!gpuInstances.CanSubmit() ||
		gpuInstances.UploadedSourceRevision() != source.SourceRevision() ||
		cache.SourceRevision() != source.SourceRevision() ||
		cache.Entries().size() != source.Groups().size()){
		return telemetry;
	}

	RHI::CommandListCreateDesc commandDesc;
	commandDesc.queueType = RHI::CommandQueueType::Graphics;
	std::unique_ptr<RHI::IRHICommandList> commandList =
		device.CreateCommandList(commandDesc);
	if(!commandList){
		++telemetry.drawFailureCount;
		return telemetry;
	}

	const std::span<const RenderPacket> packets = frameBuffer.Packets();
	const std::span<const std::size_t> packetIndices = cache.PacketIndices();
	const std::span<const StaticBatchPacketCacheEntry> cacheEntries =
		cache.Entries();
	const std::span<const StaticBatchInstanceGroup> groups = source.Groups();
	std::vector<std::size_t> submittedGroups;
	submittedGroups.reserve(groups.size());

	commandList->Begin();
	for(std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex){
		const StaticBatchInstanceGroup& group = groups[groupIndex];
		if(!IsGroupMappingEquivalent(group, cacheEntries[groupIndex])){
			++telemetry.mappingFallbackCount;
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
			uploadSystem.GetGeometryBindingCache().FindByGroupIndex(groupIndex);
		if(!geometry || !geometry->IsReady() ||
			geometry->GeometryResourceKey() != group.key.geometryKey){
			++telemetry.geometryFallbackCount;
			continue;
		}

		graphics.SetMaterial(eligibility.material.material);
		graphics.SetUVMatrixBuffer(eligibility.material.uv);

		StaticBatchDrawSubmissionDesc draw;
		draw.pipeline =
			uploadSystem.GetShadowPipelineResources().PipelineState();
		draw.vertexBuffer = geometry->VertexBuffer();
		draw.indexBuffer = geometry->IndexBuffer();
		draw.instanceBuffer = gpuInstances.Buffer();
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
