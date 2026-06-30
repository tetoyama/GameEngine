#pragma once

#include <algorithm>
#include <cmath>

#include "Shader/commonDefine.h"

namespace LocalLightShadowProjection {

inline constexpr float NearPlane = LOCAL_LIGHT_SHADOW_NEAR_PLANE;
inline constexpr float MinimumDepthSpan =
	LOCAL_LIGHT_SHADOW_MIN_DEPTH_SPAN;

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
