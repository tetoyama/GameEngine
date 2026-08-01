#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "System/Render/RenderSystem/RenderPacket/StaticBatchPacketCache.h"

enum class StaticBatchShadowGroupRejectReason : std::uint8_t {
	None,
	SingleInstance,
	InvalidPacketRange,
	InvalidRepresentativePacket,
	UnsupportedPacketKind,
	UnsupportedRenderLayer,
	MissingShadowPass,
	GroupPacketMismatch,
	MaterialUnavailable,
	DiffuseTextureUnavailable
};

// Pure packet/cache validation used before any material or texture resolution.
// Keeping this stage independent allows scheduler and cache contract tests to
// run without linking Assimp, texture loading, or editor serialization code.
namespace StaticBatchShadowGroupPacketEligibility {

inline StaticBatchShadowGroupRejectReason Validate(
	const StaticBatchPacketCacheEntry& group,
	std::span<const std::size_t> packetIndices,
	std::span<const RenderPacket> packets
) noexcept {
	if(group.instanceCount < 2){
		return StaticBatchShadowGroupRejectReason::SingleInstance;
	}
	if(group.firstInstance > packetIndices.size() ||
		group.instanceCount > packetIndices.size() - group.firstInstance ||
		group.firstInstance >
			static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)()) ||
		group.instanceCount >
			static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)())){
		return StaticBatchShadowGroupRejectReason::InvalidPacketRange;
	}
	if(group.representativePacketIndex >= packets.size()){
		return StaticBatchShadowGroupRejectReason::InvalidRepresentativePacket;
	}

	const RenderPacket& representative =
		packets[group.representativePacketIndex];
	if(representative.kind != RenderPacketKind::Model){
		return StaticBatchShadowGroupRejectReason::UnsupportedPacketKind;
	}
	if(representative.layer != RenderLayer::Opaque3D){
		return StaticBatchShadowGroupRejectReason::UnsupportedRenderLayer;
	}
	if(!HasRenderPacketPass(
		representative.passMask,
		RenderPacketPassMask::Shadow
	)){
		return StaticBatchShadowGroupRejectReason::MissingShadowPass;
	}

	for(std::size_t offset = 0; offset < group.instanceCount; ++offset){
		const std::size_t packetIndex =
			packetIndices[group.firstInstance + offset];
		if(packetIndex >= packets.size()){
			return StaticBatchShadowGroupRejectReason::InvalidPacketRange;
		}

		const RenderPacket& packet = packets[packetIndex];
		if(packet.sceneContextID != group.sceneContextID ||
			packet.kind != group.key.kind ||
			packet.layer != group.key.layer ||
			packet.materialKey != group.key.materialKey){
			return StaticBatchShadowGroupRejectReason::GroupPacketMismatch;
		}
		if(!HasRenderPacketPass(packet.passMask, RenderPacketPassMask::Shadow)){
			return StaticBatchShadowGroupRejectReason::MissingShadowPass;
		}
	}

	return StaticBatchShadowGroupRejectReason::None;
}

} // namespace StaticBatchShadowGroupPacketEligibility
