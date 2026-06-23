#pragma once

#include <cstdint>

enum class GpuFrameTimingStatus : std::uint8_t {
	Pending = 0,
	Resolved,
	Invalid,
	Dropped
};

struct GpuFrameTimingResult {
	std::uint64_t frameSerial = 0;
	double seconds = 0.0;
	GpuFrameTimingStatus status = GpuFrameTimingStatus::Invalid;
};
