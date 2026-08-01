#pragma once

#include <cstddef>
#include <span>

#include "System/Render/StaticBatch/StaticBatchModelMaterialResolver.h"
#include "System/Render/StaticBatch/StaticBatchShadowGroupPacketEligibility.h"

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
	const StaticBatchPacketCacheEntry& group,
	std::span<const std::size_t> packetIndices,
	std::span<const RenderPacket> packets
){
	StaticBatchShadowGroupEligibilityResult result;
	result.rejectReason = StaticBatchShadowGroupPacketEligibility::Validate(
		group,
		packetIndices,
		packets
	);
	if(result.rejectReason != StaticBatchShadowGroupRejectReason::None){
		return result;
	}

	const StaticBatchModelMaterialResolveResult material =
		StaticBatchModelMaterialResolver::Resolve(
			group,
			packets,
			StaticBatchModelMaterialResolvePolicy::Shadow()
		);
	if(!material.IsEligible()){
		result.rejectReason =
			StaticBatchShadowGroupRejectReason::MaterialUnavailable;
		return result;
	}
	if(material.state.UsesDiffuseTexture() &&
		!material.state.diffuseTexture){
		result.rejectReason =
			StaticBatchShadowGroupRejectReason::DiffuseTextureUnavailable;
		return result;
	}

	result.material = material.state;
	result.rejectReason = StaticBatchShadowGroupRejectReason::None;
	return result;
}

} // namespace StaticBatchShadowGroupEligibility
