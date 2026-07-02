#include <cassert>
#include <span>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchViewSelectionPolicy.h"

int main(){
	const StaticBatchViewSelectionStoragePolicy emptyPolicy =
		StaticBatchViewSelectionPolicy::ResolveStorage(
			std::span<const StaticBatchInstanceGroup>{},
			std::span<const RenderPacket>{}
		);
	assert(emptyPolicy.valid);
	assert(emptyPolicy.reserveCount == 0);
	assert(emptyPolicy.allowRuntimeGrowth);

	CullingVisibilitySet visibility;
	visibility.BeginFrame(42);

	RenderPacketCullingView playerView;
	playerView.kind = CullingViewKind::Player;
	playerView.instanceID = 1;

	RenderPacketCullingView shadowView;
	shadowView.kind = CullingViewKind::Shadow;
	shadowView.instanceID = 1;

	const std::uint64_t playerRevision =
		StaticBatchViewSelectionPolicy::MakeViewRevision(
			visibility,
			playerView
		);
	const std::uint64_t shadowRevision =
		StaticBatchViewSelectionPolicy::MakeViewRevision(
			visibility,
			shadowView
		);
	assert(playerRevision != 0);
	assert(shadowRevision != 0);
	assert(playerRevision != shadowRevision);
	assert(
		playerRevision == StaticBatchViewSelectionPolicy::MakeViewRevision(
			visibility,
			playerView
		)
	);

	visibility.BeginFrame(43);
	assert(
		playerRevision != StaticBatchViewSelectionPolicy::MakeViewRevision(
			visibility,
			playerView
		)
	);
	return 0;
}
