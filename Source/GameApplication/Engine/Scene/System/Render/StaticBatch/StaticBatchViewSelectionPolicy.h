#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

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
	StaticBatchViewSelectionStoragePolicy result;
	for(const StaticBatchInstanceGroup& group : groups){
		if(group.representativePacketIndex >= packets.size() ||
			group.instanceCount >
				(std::numeric_limits<std::size_t>::max)() - result.reserveCount){
			result.valid = false;
			return result;
		}

		const RenderPacket& packet = packets[group.representativePacketIndex];
		SceneContext* scene = packet.bindings.sceneContext;
		if(!scene || packet.sceneContextID != group.sceneContextID ||
			scene->contextID != group.sceneContextID){
			result.valid = false;
			return result;
		}

		result.reserveCount += group.instanceCount;
		result.allowRuntimeGrowth =
			result.allowRuntimeGrowth &&
			scene->storageConfig.allowRuntimeGrowth;
	}
	return result;
}

inline std::uint64_t MakeViewRevision(
	const CullingVisibilitySet& visibility,
	const RenderPacketCullingView& view
) noexcept {
	std::uint64_t revision = visibility.FrameSerial();
	revision = revision * 1099511628211ull + view.camera.GetPackedValue();
	revision = revision * 1099511628211ull +
		static_cast<std::uint64_t>(view.kind);
	revision = revision * 1099511628211ull + view.instanceID;
	return revision == 0 ? 1 : revision;
}

} // namespace StaticBatchViewSelectionPolicy
