#pragma once

#include <algorithm>
#include <cmath>

#include "Shader/commonDefine.h"

namespace LocalLightShadowProjection {

inline constexpr float NearPlane = LOCAL_LIGHT_SHADOW_NEAR_PLANE;
inline constexpr float MinimumDepthSpan =
	LOCAL_LIGHT_SHADOW_MIN_DEPTH_SPAN;
inline constexpr float MaximumNdcBias =
	LOCAL_LIGHT_SHADOW_MAX_NDC_BIAS;
inline constexpr float PointShadowFarMultiplier = 2.0f;

inline float ResolveFarPlane(float lightRange) noexcept {
	if(!std::isfinite(lightRange)){
		return NearPlane + MinimumDepthSpan;
	}
	return (std::max)(lightRange, NearPlane + MinimumDepthSpan);
}

inline float ResolvePointShadowFarPlane(float lightRange) noexcept {
	const float lightingRange = ResolveFarPlane(lightRange);
	return (std::max)(
		lightingRange * PointShadowFarMultiplier,
		NearPlane + MinimumDepthSpan
	);
}

inline bool IsRangeValid(float lightRange) noexcept {
	return std::isfinite(lightRange) &&
		lightRange >= NearPlane + MinimumDepthSpan;
}

inline float DepthRatio(float lightRange) noexcept {
	return ResolveFarPlane(lightRange) / NearPlane;
}

inline float ResolvePerspectiveDepthBias(
	float viewDepth,
	float lightRange,
	float worldBias
) noexcept {
	const float farPlane = ResolveFarPlane(lightRange);
	const float safeDepth = (std::clamp)(
		std::abs(viewDepth),
		NearPlane,
		farPlane
	);
	const float safeWorldBias = std::isfinite(worldBias)
		? (std::max)(worldBias, 0.0f)
		: 0.0f;

	// DirectX LH perspective depth:
	// z_ndc = far/(far-near) - near*far/((far-near)*viewZ)
	// その微分を使い、ワールド距離Biasを受光点深度に対応するNDC Biasへ変換する。
	const float depthDerivative =
		(NearPlane * farPlane) /
		((farPlane - NearPlane) * safeDepth * safeDepth);
	return (std::min)(
		safeWorldBias * depthDerivative,
		MaximumNdcBias
	);
}

} // namespace LocalLightShadowProjection
