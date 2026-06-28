#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "System/Render/Culling/CullingVisibilityBuilder.h"
#include "System/Render/Culling/RenderPacketViewCulling.h"
#include "System/Render/RenderSystem/RenderPacket/StaticBatchInstanceDataBuffer.h"

enum class StaticBatchGroupVisibilityState : std::uint8_t {
	Invalid,
	AllVisible,
	AllCulled,
	Mixed
};

struct StaticBatchGroupVisibilityResult {
	StaticBatchGroupVisibilityState state =
		StaticBatchGroupVisibilityState::Invalid;
	std::size_t visibleInstanceCount = 0;
	std::size_t culledInstanceCount = 0;

	bool CanSubmitInstanced() const noexcept {
		return state == StaticBatchGroupVisibilityState::AllVisible;
	}

	bool CanSkipEntireGroup() const noexcept {
		return state == StaticBatchGroupVisibilityState::AllCulled;
	}

	bool RequiresPacketFallback() const noexcept {
		return state == StaticBatchGroupVisibilityState::Invalid ||
			state == StaticBatchGroupVisibilityState::Mixed;
	}
};

namespace StaticBatchGroupVisibility {

inline bool IsInstanceRangeValid(
	const StaticBatchPacketCacheEntry& group,
	std::span<const StaticBatchInstanceData> instances
) noexcept {
	if(group.instanceCount == 0) return false;
	if(group.firstInstance > instances.size()) return false;
	return group.instanceCount <= instances.size() - group.firstInstance;
}

inline StaticBatchGroupVisibilityResult Evaluate(
	const StaticBatchPacketCacheEntry& group,
	std::span<const StaticBatchInstanceData> instances,
	std::span<const RenderPacket> packets,
	const CullingVisibilitySet& visibility,
	const RenderPacketCullingView& view
){
	StaticBatchGroupVisibilityResult result;
	if(!IsInstanceRangeValid(group, instances) ||
		group.representativePacketIndex >= packets.size()){
		return result;
	}

	const RenderPacket& representative =
		packets[group.representativePacketIndex];
	SceneContext* context = representative.bindings.sceneContext;
	if(!context || !context->component ||
		representative.sceneContextID != group.sceneContextID ||
		representative.kind != group.key.kind){
		return result;
	}

	const CullingViewKey viewKey = RenderPacketViewCulling::MakeViewKey(
		group.sceneContextID,
		view
	);
	for(std::size_t offset = 0; offset < group.instanceCount; ++offset){
		const StaticBatchInstanceData& instance =
			instances[group.firstInstance + offset];
		if(instance.sceneContextID != group.sceneContextID ||
			instance.entityIndex == 0){
			result.state = StaticBatchGroupVisibilityState::Invalid;
			result.visibleInstanceCount = 0;
			result.culledInstanceCount = 0;
			return result;
		}

		const Entity entity(
			instance.entityIndex,
			instance.entityGeneration
		);
		if(CullingVisibilityBuilder::ShouldRender(
			visibility,
			viewKey,
			entity,
			*context->component
		)){
			++result.visibleInstanceCount;
		}else{
			++result.culledInstanceCount;
		}
	}

	if(result.visibleInstanceCount == group.instanceCount){
		result.state = StaticBatchGroupVisibilityState::AllVisible;
	}else if(result.culledInstanceCount == group.instanceCount){
		result.state = StaticBatchGroupVisibilityState::AllCulled;
	}else{
		result.state = StaticBatchGroupVisibilityState::Mixed;
	}
	return result;
}

} // namespace StaticBatchGroupVisibility
