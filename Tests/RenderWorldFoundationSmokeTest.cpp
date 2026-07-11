#include <array>
#include <cassert>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <type_traits>

#include "Engine/Scene/System/Render/RenderSystem/RenderWorld/RenderWorld.h"

namespace {

std::string ReadTextFile(const char* path){
	std::ifstream stream(path, std::ios::binary);
	assert(stream && "contract source file must exist");
	return std::string(
		std::istreambuf_iterator<char>(stream),
		std::istreambuf_iterator<char>()
	);
}

void ValidateRenderSystemOwnershipContract(){
	const std::string header = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/RenderSystem/renderSystem.h"
	);

	assert(header.find(
		"#include \"System/Render/RenderSystem/RenderWorld/RenderWorld.h\""
	) != std::string::npos);
	assert(header.find("RenderWorld m_renderWorld;") != std::string::npos);
	assert(header.find("m_renderWorld.Reset();") != std::string::npos);
	assert(header.find("return m_renderWorld.Packets();") != std::string::npos);
	assert(header.find("m_renderWorld.PrepareView(view);") != std::string::npos);
	assert(header.find("m_renderWorld.ShouldRender(view, packet)") !=
		std::string::npos);
	assert(header.find("return m_renderWorld.Visibility();") !=
		std::string::npos);
	assert(header.find("return m_renderWorld.LastSubmittedGeneration();") !=
		std::string::npos);

	// Step 18-A移行中のcpp互換名は参照であり、所有Storageではない。
	assert(header.find("RenderPacketFrameBuffer& m_renderPacketBuffer;") !=
		std::string::npos);
	assert(header.find("CullingVisibilitySet& m_cullingVisibility;") !=
		std::string::npos);
	assert(header.find("uint64_t& m_lastSubmittedPacketGeneration;") !=
		std::string::npos);
	assert(header.find("RenderPacketFrameBuffer m_renderPacketBuffer;") ==
		std::string::npos);
	assert(header.find("CullingVisibilitySet m_cullingVisibility;") ==
		std::string::npos);
	assert(header.find("uint64_t m_lastSubmittedPacketGeneration = 0;") ==
		std::string::npos);
}

} // namespace

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

	std::uint64_t& compatibilitySubmission =
		world.SubmissionGenerationStorage();
	compatibilitySubmission = 43;
	assert(world.LastSubmittedGeneration() == 43);

	RenderPacketCullingView unstableView;
	world.PrepareView(unstableView);
	assert(world.Visibility().FrameSerial() == 43);

	world.Reset();
	assert(world.Generation() == 0);
	assert(world.Visibility().FrameSerial() == 0);
	assert(world.LastSubmittedGeneration() == 0);
	assert(!world.IsReady());
	assert(!world.IsCurrentFrameSubmitted());

	ValidateRenderSystemOwnershipContract();
	return 0;
}
