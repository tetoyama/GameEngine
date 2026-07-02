#pragma once

namespace PlayerViewRefreshPolicy {

constexpr float IdleRefreshIntervalSeconds = 1.0f;

constexpr bool ShouldRender(
	bool throttledState,
	float elapsedSeconds,
	bool hasOutput
) noexcept {
	return !throttledState ||
		!hasOutput ||
		elapsedSeconds >= IdleRefreshIntervalSeconds;
}

} // namespace PlayerViewRefreshPolicy
