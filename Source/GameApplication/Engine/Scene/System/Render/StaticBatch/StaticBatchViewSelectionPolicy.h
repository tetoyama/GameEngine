#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include "System/Render/Culling/RenderPacketViewCulling.h"
#include "System/Render/RenderSystem/RenderPacket/StaticBatchInstanceDataBuffer.h"

struct StaticBatchViewSelectionStoragePolicy {
	std::size_t reserveCount = 0;
	bool allowRuntimeGrowth = true;
	bool valid = true;
};

namespace StaticBatchViewSelectionPolicy {

inline StaticBatchViewSelectionStoragePolicy ResolveStorage(
	std::span<const StaticBatchInstanceGroup> groups,
	std::span<const RenderPacket> packets
) noexcept {
	StaticBatchViewSelectionStoragePolicy policy;
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

inline std::uint64_t MakeViewRevision(
	const CullingVisibilitySet& visibility,
	const RenderPacketCullingView& view
) noexcept {
	std::uint64_t hash = visibility.FrameSerial();
	const auto combine = [&hash](std::uint64_t value){
		hash ^= value + 0x9e3779b97f4a7c15ull +
			(hash << 6) + (hash >> 2);
	};
	combine(view.camera.GetPackedValue());
	combine(static_cast<std::uint64_t>(view.kind));
	combine(view.instanceID);
	return hash == 0 ? 1 : hash;
}

} // namespace StaticBatchViewSelectionPolicy
