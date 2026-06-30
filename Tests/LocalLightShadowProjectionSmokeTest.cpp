#include <cassert>
#include <cmath>
#include <limits>

#include "Engine/Scene/System/Render/Lighting/LocalLightShadowProjection.h"

namespace {

bool NearlyEqualRelative(float left, float right, float epsilon = 0.0001f){
	const float scale = (std::max)(1.0f, (std::max)(std::abs(left), std::abs(right)));
	return std::abs(left - right) <= epsilon * scale;
}

float PerspectiveDepthDerivative(float viewDepth, float farPlane){
	using namespace LocalLightShadowProjection;
	return (NearPlane * farPlane) /
		((farPlane - NearPlane) * viewDepth * viewDepth);
}

} // namespace

int main(){
	using namespace LocalLightShadowProjection;

	assert(NearPlane > 0.0f);
	assert(MinimumDepthSpan > 0.0f);
	assert(MaximumNdcBias > 0.0f);
	assert(IsRangeValid(NearPlane + MinimumDepthSpan));
	assert(!IsRangeValid(0.0f));
	assert(!IsRangeValid(-10.0f));
	assert(!IsRangeValid(std::numeric_limits<float>::infinity()));
	assert(!IsRangeValid(std::numeric_limits<float>::quiet_NaN()));

	assert(ResolveFarPlane(10.0f) == 10.0f);
	assert(ResolveFarPlane(0.0f) == NearPlane + MinimumDepthSpan);
	assert(ResolveFarPlane(-10.0f) == NearPlane + MinimumDepthSpan);
	assert(
		ResolveFarPlane(std::numeric_limits<float>::infinity()) ==
		NearPlane + MinimumDepthSpan
	);
	assert(
		ResolveFarPlane(std::numeric_limits<float>::quiet_NaN()) ==
		NearPlane + MinimumDepthSpan
	);

	assert(std::abs(DepthRatio(10.0f) - 100.0f) < 0.001f);
	assert(std::abs(DepthRatio(1000.0f) - 10000.0f) < 0.001f);

	constexpr float worldBias = 0.005f;
	constexpr float range = 10.0f;
	const float nearDepthBias = ResolvePerspectiveDepthBias(1.0f, range, worldBias);
	const float middleDepthBias = ResolvePerspectiveDepthBias(5.0f, range, worldBias);
	const float farDepthBias = ResolvePerspectiveDepthBias(10.0f, range, worldBias);

	assert(nearDepthBias > middleDepthBias);
	assert(middleDepthBias > farDepthBias);
	assert(nearDepthBias <= MaximumNdcBias);

	const float reconstructedNearWorldBias =
		nearDepthBias / PerspectiveDepthDerivative(1.0f, range);
	const float reconstructedMiddleWorldBias =
		middleDepthBias / PerspectiveDepthDerivative(5.0f, range);
	const float reconstructedFarWorldBias =
		farDepthBias / PerspectiveDepthDerivative(10.0f, range);
	assert(NearlyEqualRelative(reconstructedNearWorldBias, worldBias));
	assert(NearlyEqualRelative(reconstructedMiddleWorldBias, worldBias));
	assert(NearlyEqualRelative(reconstructedFarWorldBias, worldBias));

	// 同じWorld BiasでもFarが変われば必要なNDC Biasは変わる。
	const float longRangeFarBias = ResolvePerspectiveDepthBias(
		1000.0f,
		1000.0f,
		worldBias
	);
	assert(longRangeFarBias > 0.0f);
	assert(longRangeFarBias < farDepthBias);

	assert(ResolvePerspectiveDepthBias(1.0f, range, -1.0f) == 0.0f);
	assert(
		ResolvePerspectiveDepthBias(
			1.0f,
			range,
			std::numeric_limits<float>::quiet_NaN()
		) == 0.0f
	);

	return 0;
}
