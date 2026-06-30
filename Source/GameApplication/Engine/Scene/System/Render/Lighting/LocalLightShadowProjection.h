#pragma once

#include <algorithm>
#include <cmath>

namespace LocalLightShadowProjection {

inline constexpr float NearPlane = 0.1f;
inline constexpr float MinimumDepthSpan = 0.1f;

inline float ResolveFarPlane(float lightRange) noexcept {
	if(!std::isfinite(lightRange)){
		return NearPlane + MinimumDepthSpan;
	}
	return (std::max)(lightRange, NearPlane + MinimumDepthSpan);
}

inline bool IsRangeValid(float lightRange) noexcept {
	return std::isfinite(lightRange) &&
		lightRange >= NearPlane + MinimumDepthSpan;
}

inline float DepthRatio(float lightRange) noexcept {
	return ResolveFarPlane(lightRange) / NearPlane;
}

} // namespace LocalLightShadowProjection
