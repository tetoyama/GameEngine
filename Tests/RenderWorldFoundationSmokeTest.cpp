#include <array>
#include <cassert>
#include <span>
#include <type_traits>

#include "Engine/Scene/System/Render/RenderSystem/RenderWorld/RenderWorld.h"

int main(){
	static_assert(!std::is_copy_constructible_v<RenderWorld>);
	static_assert(!std::is_copy_assignable_v<RenderWorld>);
	static_assert(!std::is_move_constructible_v<RenderWorld>);
	static_assert(!std::is_move_assignable_v<RenderWorld>);

	RenderWorld world;
	assert(world.Generation() == 0);
	assert(world.LastSubmittedGeneration() == 0);
	assert(!world.IsReady());
	assert(!world.IsCurrentFrameSubmitted());
	assert(!world.MarkSubmitted());

	world.BeginFrame(42);
	assert(world.Generation() == 42);
	assert(world.Visibility().FrameSerial() == 42);
	assert(!world.IsReady());

	const std::array<RenderPacketWorkerBuffer, 0> noWorkers{};
	world.Publish(std::span<const RenderPacketWorkerBuffer>(noWorkers));
	assert(world.IsReady());
	assert(world.Packets().Size() == 0);
	assert(world.StaticBatchCandidates().Candidates().empty());
	assert(world.StaticBatchCache().Entries().empty());
	assert(world.StaticBatchInstances().Instances().empty());
	assert(world.MarkSubmitted());
	assert(world.LastSubmittedGeneration() == 42);
	assert(world.IsCurrentFrameSubmitted());

	world.BeginFrame(43);
	assert(world.Generation() == 43);
	assert(world.Visibility().FrameSerial() == 43);
	assert(!world.IsReady());
	assert(!world.IsCurrentFrameSubmitted());
	assert(world.LastSubmittedGeneration() == 42);

	RenderPacketCullingView unstableView;
	world.PrepareView(unstableView);
	assert(world.Visibility().FrameSerial() == 43);

	world.Reset();
	assert(world.Generation() == 0);
	assert(world.Visibility().FrameSerial() == 0);
	assert(world.LastSubmittedGeneration() == 0);
	assert(!world.IsReady());
	assert(!world.IsCurrentFrameSubmitted());
	return 0;
}
