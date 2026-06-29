#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "System/Render/StaticBatch/StaticBatchModelMaterialResolver.h"

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
	DiffuseTextureUnsupported
};

struct StaticBatchShadowGroupEligibilityResult {
	StaticBatchModelMaterialState material;
	StaticBatchShadowGroupRejectReason rejectReason =
		StaticBatchShadowGroupRejectReason::InvalidRepresentativePacket;

	bool IsEligible() const noexcept {
		return rejectReason == StaticBatchShadowGroupRejectReason::None;
	}
};

namespace StaticBatchShadowGroupEligibility {

inline StaticBatchShadowGroupEligibilityResult Resolve(
	const StaticBatchInstanceGroup& group,
	std::span<const std::size_t> packetIndices,
	std::span<const RenderPacket> packets
){
	StaticBatchShadowGroupEligibilityResult result;
	if(group.instanceCount < 2){
		result.rejectReason =
			StaticBatchShadowGroupRejectReason::SingleInstance;
		return result;
	}
	if(group.firstInstance > packetIndices.size() ||
		group.instanceCount > packetIndices.size() - group.firstInstance){
		result.rejectReason =
			StaticBatchShadowGroupRejectReason::InvalidPacketRange;
		return result;
	}
	if(group.representativePacketIndex >= packets.size()){
		return result;
	}

	const RenderPacket& representative =
		packets[group.representativePacketIndex];
	if(representative.kind != RenderPacketKind::Model){
		result.rejectReason =
			StaticBatchShadowGroupRejectReason::UnsupportedPacketKind;
		return result;
	}
	if(representative.layer != RenderLayer::Opaque3D){
		result.rejectReason =
			StaticBatchShadowGroupRejectReason::UnsupportedRenderLayer;
		return result;
	}
	if(!HasRenderPacketPass(
		representative.passMask,
		RenderPacketPassMask::Shadow
	)){
		result.rejectReason =
			StaticBatchShadowGroupRejectReason::MissingShadowPass;
		return result;
	}

	for(std::size_t offset = 0; offset < group.instanceCount; ++offset){
		const std::size_t packetIndex =
			packetIndices[group.firstInstance + offset];
		if(packetIndex >= packets.size()){
			result.rejectReason =
				StaticBatchShadowGroupRejectReason::InvalidPacketRange;
			return result;
		}

		const RenderPacket& packet = packets[packetIndex];
		if(packet.sceneContextID != group.sceneContextID ||
			packet.kind != group.key.kind ||
			packet.layer != group.key.layer ||
			packet.materialKey != group.key.materialKey){
			result.rejectReason =
				StaticBatchShadowGroupRejectReason::GroupPacketMismatch;
			return result;
		}
		if(!HasRenderPacketPass(packet.passMask, RenderPacketPassMask::Shadow)){
			result.rejectReason =
				StaticBatchShadowGroupRejectReason::MissingShadowPass;
			return result;
		}
	}

	const StaticBatchModelMaterialResolveResult material =
		StaticBatchModelMaterialResolver::Resolve(group, packets);
	if(!material.IsEligible()){
		result.rejectReason =
			StaticBatchShadowGroupRejectReason::MaterialUnavailable;
		return result;
	}
	if(material.state.UsesDiffuseTexture()){
		result.rejectReason =
			StaticBatchShadowGroupRejectReason::DiffuseTextureUnsupported;
		return result;
	}

	result.material = material.state;
	result.rejectReason = StaticBatchShadowGroupRejectReason::None;
	return result;
}

} // namespace StaticBatchShadowGroupEligibility
