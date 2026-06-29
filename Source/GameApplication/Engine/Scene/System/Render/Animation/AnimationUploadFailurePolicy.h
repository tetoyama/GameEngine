#pragma once

#include <cstdint>
#include <limits>

namespace AnimationUploadFailurePolicy {

inline constexpr std::uint32_t RecreateBufferThreshold = 60;
inline constexpr std::uint32_t PeriodicLogInterval = 300;

inline std::uint32_t Increment(std::uint32_t count) noexcept {
	return count < (std::numeric_limits<std::uint32_t>::max)()
		? count + 1
		: count;
}

inline bool ShouldLog(std::uint32_t count) noexcept {
	return count == 1 ||
		(count != 0 && (count % PeriodicLogInterval) == 0);
}

inline bool ShouldRecreateBuffers(std::uint32_t count) noexcept {
	return count >= RecreateBufferThreshold &&
		(count % RecreateBufferThreshold) == 0;
}

} // namespace AnimationUploadFailurePolicy
