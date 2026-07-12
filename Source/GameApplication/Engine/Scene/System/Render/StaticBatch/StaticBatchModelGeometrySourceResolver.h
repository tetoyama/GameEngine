#pragma once

#include <cstdint>
#include <span>

#include "Scene/Component/modelRendererComponent.h"
#include "System/Render/RenderSystem/RenderPacket/RenderPacket.h"
#include "System/Render/RenderSystem/RenderPacket/StaticBatchPacketCache.h"
#include "System/Render/StaticBatch/StaticBatchD3D11GeometrySource.h"
#include "System/Render/StaticBatch/StaticBatchModelGeometrySourceProvider.h"

enum class StaticBatchModelGeometryRejectReason : std::uint8_t {
	None,
	InvalidRepresentativePacket,
	GroupPacketMismatch,
	UnsupportedPacketKind,
	UnsupportedRenderLayer,
	MissingGBufferPass,
	MissingModelRenderer,
	AnimatedModel,
	MissingModelResource,
	UnsupportedSubMeshCount,
	InvalidSubMeshIndex,
	MissingSubMesh,
	SkinnedSubMesh,
	MissingNativeBuffer,
	InvalidGeometryCount,
	GeometryResourceKeyMismatch
};

struct StaticBatchModelGeometryResolveResult {
	StaticBatchD3D11GeometrySource source;
	StaticBatchModelGeometryRejectReason rejectReason =
		StaticBatchModelGeometryRejectReason::InvalidRepresentativePacket;

	bool IsEligible() const noexcept {
		return rejectReason == StaticBatchModelGeometryRejectReason::None &&
			source.IsValid();
	}
};

namespace StaticBatchModelGeometrySourceResolver {

inline StaticBatchModelGeometryRejectReason MapSourceStatus(
	StaticBatchModelGeometrySourceStatus status
) noexcept {
	switch(status){
		case StaticBatchModelGeometrySourceStatus::None:
			return StaticBatchModelGeometryRejectReason::None;
		case StaticBatchModelGeometrySourceStatus::MissingModelResource:
			return StaticBatchModelGeometryRejectReason::MissingModelResource;
		case StaticBatchModelGeometrySourceStatus::UnsupportedSubMeshCount:
			return StaticBatchModelGeometryRejectReason::UnsupportedSubMeshCount;
		case StaticBatchModelGeometrySourceStatus::InvalidSubMeshIndex:
			return StaticBatchModelGeometryRejectReason::InvalidSubMeshIndex;
		case StaticBatchModelGeometrySourceStatus::MissingSubMesh:
			return StaticBatchModelGeometryRejectReason::MissingSubMesh;
		case StaticBatchModelGeometrySourceStatus::SkinnedSubMesh:
			return StaticBatchModelGeometryRejectReason::SkinnedSubMesh;
		case StaticBatchModelGeometrySourceStatus::MissingNativeBuffer:
			return StaticBatchModelGeometryRejectReason::MissingNativeBuffer;
		case StaticBatchModelGeometrySourceStatus::InvalidGeometryCount:
			return StaticBatchModelGeometryRejectReason::InvalidGeometryCount;
		default:
			return StaticBatchModelGeometryRejectReason::InvalidGeometryCount;
	}
}

inline StaticBatchModelGeometryResolveResult Resolve(
	const StaticBatchPacketCacheEntry& group,
	std::span<const RenderPacket> packets,
	const IStaticBatchModelGeometrySourceProvider& sourceProvider
) noexcept {
	StaticBatchModelGeometryResolveResult result;
	if(group.representativePacketIndex >= packets.size()){
		return result;
	}

	const RenderPacket& packet = packets[group.representativePacketIndex];
	if(packet.sceneContextID != group.sceneContextID ||
		packet.kind != group.key.kind ||
		packet.layer != group.key.layer){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::GroupPacketMismatch;
		return result;
	}
	if(packet.kind != RenderPacketKind::Model){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::UnsupportedPacketKind;
		return result;
	}
	if(packet.layer != RenderLayer::Opaque3D){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::UnsupportedRenderLayer;
		return result;
	}
	if(!HasRenderPacketPass(packet.passMask, RenderPacketPassMask::GBuffer)){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::MissingGBufferPass;
		return result;
	}
	if(group.key.geometryKey == 0){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::InvalidGeometryCount;
		return result;
	}

	const ModelRendererComponent* renderer = packet.bindings.modelRenderer;
	if(!renderer){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::MissingModelRenderer;
		return result;
	}
	if(!renderer->blendedAnimations.empty()){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::AnimatedModel;
		return result;
	}

	const StaticBatchModelGeometrySourceResult sourceResult =
		sourceProvider.Resolve(*renderer, packet, group.key.geometryKey);
	if(!sourceResult.IsEligible()){
		result.rejectReason = MapSourceStatus(sourceResult.status);
		return result;
	}
	if(sourceResult.source.geometryResourceKey != group.key.geometryKey){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::GeometryResourceKeyMismatch;
		return result;
	}

	result.source = sourceResult.source;
	result.rejectReason = StaticBatchModelGeometryRejectReason::None;
	return result;
}

inline StaticBatchModelGeometryResolveResult Resolve(
	const StaticBatchPacketCacheEntry& group,
	std::span<const RenderPacket> packets
) noexcept {
	return Resolve(
		group,
		packets,
		StaticBatchModelGeometrySourceProviders::LegacyModelData()
	);
}

} // namespace StaticBatchModelGeometrySourceResolver
