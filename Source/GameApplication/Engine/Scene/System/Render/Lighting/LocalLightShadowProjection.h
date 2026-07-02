#pragma once

#include <algorithm>
#include <cmath>

#include "Shader/commonDefine.h"

namespace LocalLightShadowProjection {

inline constexpr float NearPlane = LOCAL_LIGHT_SHADOW_NEAR_PLANE;
inline constexpr float MinimumDepthSpan = LOCAL_LIGHT_SHADOW_MIN_DEPTH_SPAN;
inline constexpr float MaximumNdcBias = LOCAL_LIGHT_SHADOW_MAX_NDC_BIAS;
inline constexpr float BiasReferenceDepth = LOCAL_LIGHT_SHADOW_BIAS_REFERENCE_DEPTH;
inline constexpr float MaximumSlopeScale = LOCAL_LIGHT_SHADOW_MAX_SLOPE_SCALE;

inline float ResolveFarPlane(float lightRange) noexcept {
	if(!std::isfinite(lightRange)){
		return NearPlane + MinimumDepthSpan;
	}
	return (std::max)(lightRange, NearPlane + MinimumDepthSpan);
}

inline float ResolvePointShadowFarPlane(float lightRange) noexcept {
	return ResolveFarPlane(lightRange);
}

inline bool IsRangeValid(float lightRange) noexcept {
	return std::isfinite(lightRange) &&
		lightRange >= NearPlane + MinimumDepthSpan;
}

inline float DepthRatio(float lightRange) noexcept {
	return ResolveFarPlane(lightRange) / NearPlane;
}

inline float ResolvePerspectiveDepthDerivative(
	float viewDepth,
	float lightRange
) noexcept {
	const float farPlane = ResolveFarPlane(lightRange);
	const float safeDepth = (std::clamp)(
		std::abs(viewDepth),
		NearPlane,
		farPlane
	);
	const float projectionScale =
		(NearPlane * farPlane) /
		(std::max)(farPlane - NearPlane, 0.000001f);
	return projectionScale /
		(std::max)(safeDepth * safeDepth, 0.000001f);
}

inline float ResolvePerspectiveDepthBias(
	float viewDepth,
	float lightRange,
	float projectedDepthBiasAtReferenceDepth,
	float receiverNdotL = 1.0f
) noexcept {
	const float farPlane = ResolveFarPlane(lightRange);
	const float safeDepth = (std::clamp)(
		std::abs(viewDepth),
		NearPlane,
		farPlane
	);
	const float referenceDepth = (std::clamp)(
		BiasReferenceDepth,
		NearPlane,
		farPlane
	);

	const float baseNdcBias = std::isfinite(projectedDepthBiasAtReferenceDepth)
		? (std::clamp)(
			projectedDepthBiasAtReferenceDepth,
			0.0f,
			MaximumNdcBias
		)
		: 0.0f;
	if(baseNdcBias <= 0.0f){
		return 0.0f;
	}

	const float projectionScale =
		(NearPlane * farPlane) /
		(std::max)(farPlane - NearPlane, 0.000001f);
	const float referenceDerivative =
		projectionScale /
		(std::max)(referenceDepth * referenceDepth, 0.000001f);
	const float currentDerivative =
		projectionScale /
		(std::max)(safeDepth * safeDepth, 0.000001f);
	const float worldEquivalentBias =
		baseNdcBias /
		(std::max)(referenceDerivative, 0.000001f);

	const float safeNdotL = std::isfinite(receiverNdotL)
		? (std::clamp)(receiverNdotL, 0.0f, 1.0f)
		: 1.0f;
	const float grazing = 1.0f - safeNdotL;
	const float slopeScale = (std::min)(
		1.0f + grazing * grazing * 8.0f,
		MaximumSlopeScale
	);
	const float distanceAdjustedBias =
		worldEquivalentBias * slopeScale * currentDerivative;

	return (std::min)(baseNdcBias, distanceAdjustedBias);
}

} // namespace LocalLightShadowProjection
