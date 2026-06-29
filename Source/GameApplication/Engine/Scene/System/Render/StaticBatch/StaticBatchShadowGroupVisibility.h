#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "System/Render/RenderSystem/RenderPacket/StaticBatchPacketCache.h"

enum class StaticBatchShadowGroupVisibilityKind : std::uint8_t {
	Invalid,
	AllVisible,
	AllCulled,
	Mixed
};

struct StaticBatchShadowGroupVisibilityResult {
	StaticBatchShadowGroupVisibilityKind kind =
		StaticBatchShadowGroupVisibilityKind::Invalid;
	std::size_t visibleInstanceCount = 0;
	std::size_t culledInstanceCount = 0;
};

namespace StaticBatchShadowGroupVisibility {

template<typename Predicate>
StaticBatchShadowGroupVisibilityResult Resolve(
	const StaticBatchPacketCacheEntry& group,
	std::span<const std::size_t> packetIndices,
	std::size_t packetCount,
	Predicate&& isVisible
){
	StaticBatchShadowGroupVisibilityResult result;
	if(group.instanceCount == 0 ||
		group.firstInstance > packetIndices.size() ||
		group.instanceCount > packetIndices.size() - group.firstInstance){
		return result;
	}

	for(std::size_t offset = 0; offset < group.instanceCount; ++offset){
		const std::size_t packetIndex =
			packetIndices[group.firstInstance + offset];
		if(packetIndex >= packetCount){
			result.kind = StaticBatchShadowGroupVisibilityKind::Invalid;
			result.visibleInstanceCount = 0;
			result.culledInstanceCount = 0;
			return result;
		}
		if(isVisible(packetIndex)){
			++result.visibleInstanceCount;
		}else{
			++result.culledInstanceCount;
		}
	}

	if(result.visibleInstanceCount == group.instanceCount){
		result.kind = StaticBatchShadowGroupVisibilityKind::AllVisible;
	}else if(result.culledInstanceCount == group.instanceCount){
		result.kind = StaticBatchShadowGroupVisibilityKind::AllCulled;
	}else{
		result.kind = StaticBatchShadowGroupVisibilityKind::Mixed;
	}
	return result;
}

} // namespace StaticBatchShadowGroupVisibility
