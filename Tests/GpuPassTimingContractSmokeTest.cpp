#include <cassert>
#include <cstdint>

#include "Service/Graphics/GpuPassTimingProfiler.h"

int main(){
	static_assert(GpuPassTimingScopeCount > 0);
	static_assert(GpuPassTimingScopeCount <= 64);

	GpuFrameTimingResult result;
	result.frameSerial = 7;
	result.status = GpuFrameTimingStatus::Resolved;
	result.passSeconds[static_cast<std::size_t>(
		GpuPassTimingScope::PlayerGBuffer
	)] = 0.002;
	result.resolvedPassMask = GpuPassTimingBit(
		GpuPassTimingScope::PlayerGBuffer
	);
	result.invalidPassMask = GpuPassTimingBit(
		GpuPassTimingScope::EditorShadow
	);

	assert(result.IsPassResolved(GpuPassTimingScope::PlayerGBuffer));
	assert(!result.IsPassResolved(GpuPassTimingScope::PlayerLighting));
	assert(result.IsPassInvalid(GpuPassTimingScope::EditorShadow));
	assert(result.GetPassSeconds(GpuPassTimingScope::PlayerGBuffer) == 0.002);
	assert(GpuPassTimingScopeName(GpuPassTimingScope::ImGui) != nullptr);

	GpuPassTimingProfiler profiler;
	profiler.BeginFrame(nullptr, nullptr, 42);
	auto dropped = profiler.ConsumeResolved(nullptr);
	assert(dropped.size() == 1);
	assert(dropped.front().frameSerial == 42);
	assert(dropped.front().status == GpuFrameTimingStatus::Dropped);
	assert(!profiler.BeginPass(nullptr, GpuPassTimingScope::PlayerGBuffer));

	profiler.Reset();
	assert(profiler.ConsumeResolved(nullptr).empty());
	return 0;
}
