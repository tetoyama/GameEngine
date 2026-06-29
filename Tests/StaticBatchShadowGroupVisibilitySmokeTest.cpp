#include <array>
#include <cassert>
#include <cstddef>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchShadowGroupVisibility.h"

int main(){
	StaticBatchPacketCacheEntry group;
	group.firstInstance = 1;
	group.instanceCount = 3;
	constexpr std::array<std::size_t, 5> packetIndices{4, 0, 2, 3, 1};

	const auto allVisible = StaticBatchShadowGroupVisibility::Resolve(
		group,
		packetIndices,
		5,
		[](std::size_t){ return true; }
	);
	assert(allVisible.kind == StaticBatchShadowGroupVisibilityKind::AllVisible);
	assert(allVisible.visibleInstanceCount == 3);
	assert(allVisible.culledInstanceCount == 0);

	const auto allCulled = StaticBatchShadowGroupVisibility::Resolve(
		group,
		packetIndices,
		5,
		[](std::size_t){ return false; }
	);
	assert(allCulled.kind == StaticBatchShadowGroupVisibilityKind::AllCulled);
	assert(allCulled.visibleInstanceCount == 0);
	assert(allCulled.culledInstanceCount == 3);

	const auto mixed = StaticBatchShadowGroupVisibility::Resolve(
		group,
		packetIndices,
		5,
		[](std::size_t packetIndex){ return packetIndex != 2; }
	);
	assert(mixed.kind == StaticBatchShadowGroupVisibilityKind::Mixed);
	assert(mixed.visibleInstanceCount == 2);
	assert(mixed.culledInstanceCount == 1);

	group.firstInstance = 4;
	const auto invalidRange = StaticBatchShadowGroupVisibility::Resolve(
		group,
		packetIndices,
		5,
		[](std::size_t){ return true; }
	);
	assert(invalidRange.kind == StaticBatchShadowGroupVisibilityKind::Invalid);

	group.firstInstance = 0;
	group.instanceCount = 1;
	constexpr std::array<std::size_t, 1> invalidPacketIndex{9};
	const auto invalidPacket = StaticBatchShadowGroupVisibility::Resolve(
		group,
		invalidPacketIndex,
		5,
		[](std::size_t){ return true; }
	);
	assert(invalidPacket.kind == StaticBatchShadowGroupVisibilityKind::Invalid);
	assert(invalidPacket.visibleInstanceCount == 0);
	assert(invalidPacket.culledInstanceCount == 0);
	return 0;
}
