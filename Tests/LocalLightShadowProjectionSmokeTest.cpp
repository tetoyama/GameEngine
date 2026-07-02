#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

#include "Engine/Scene/System/Render/Lighting/LocalLightShadowProjection.h"

namespace {

bool NearlyEqual(
	float left,
	float right,
	float relativeEpsilon = 0.0001f,
	float absoluteEpsilon = 0.0000001f
){
	const float difference = std::abs(left - right);
	const float scale = (std::max)(std::abs(left), std::abs(right));
	return difference <= (std::max)(absoluteEpsilon, relativeEpsilon * scale);
}

} // namespace

int main(){
	using namespace LocalLightShadowProjection;

	assert(NearPlane > 0.0f);
	assert(MinimumDepthSpan > 0.0f);
	assert(MaximumNdcBias > 0.0f);
	assert(BiasReferenceDepth >= NearPlane);
	assert(MaximumSlopeScale >= 1.0f);
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

	constexpr float projectedBiasAtReferenceDepth = 0.005f;
	constexpr float range = 10.0f;
	const float referenceDepth = (std::clamp)(
		BiasReferenceDepth,
		NearPlane,
		ResolveFarPlane(range)
	);
	const float referenceBias = ResolvePerspectiveDepthBias(
		referenceDepth,
		range,
		projectedBiasAtReferenceDepth
	);
	const float middleBias = ResolvePerspectiveDepthBias(
		5.0f,
		range,
		projectedBiasAtReferenceDepth
	);
	const float farBias = ResolvePerspectiveDepthBias(
		10.0f,
		range,
		projectedBiasAtReferenceDepth
	);

	assert(NearlyEqual(referenceBias, projectedBiasAtReferenceDepth));
	assert(referenceBias > middleBias);
	assert(middleBias > farBias);
	assert(referenceBias <= MaximumNdcBias);

	const float referenceDerivative = ResolvePerspectiveDepthDerivative(
		referenceDepth,
		range
	);
	const float expectedWorldEquivalent =
		projectedBiasAtReferenceDepth / referenceDerivative;
	const float reconstructedMiddleWorldEquivalent =
		middleBias / ResolvePerspectiveDepthDerivative(5.0f, range);
	const float reconstructedFarWorldEquivalent =
		farBias / ResolvePerspectiveDepthDerivative(10.0f, range);
	assert(NearlyEqual(reconstructedMiddleWorldEquivalent, expectedWorldEquivalent));
	assert(NearlyEqual(reconstructedFarWorldEquivalent, expectedWorldEquivalent));

	// Farだけを変更しても、同じ受光点深度なら補正後Biasは変化しない。
	const float shortRangeBias = ResolvePerspectiveDepthBias(
		5.0f,
		10.0f,
		projectedBiasAtReferenceDepth
	);
	const float longRangeBias = ResolvePerspectiveDepthBias(
		5.0f,
		1000.0f,
		projectedBiasAtReferenceDepth
	);
	assert(NearlyEqual(shortRangeBias, longRangeBias));

	// 基準深度より近い受光点では、既存Projected Biasを上限として維持する。
	const float closeBias = ResolvePerspectiveDepthBias(
		0.2f,
		range,
		projectedBiasAtReferenceDepth
	);
	assert(NearlyEqual(closeBias, projectedBiasAtReferenceDepth));

	const float frontFacingBias = ResolvePerspectiveDepthBias(
		5.0f,
		range,
		projectedBiasAtReferenceDepth,
		1.0f
	);
	const float grazingBias = ResolvePerspectiveDepthBias(
		5.0f,
		range,
		projectedBiasAtReferenceDepth,
		0.0f
	);
	assert(grazingBias > frontFacingBias);
	assert(grazingBias <= projectedBiasAtReferenceDepth);

	assert(
		NearlyEqual(
			ResolvePerspectiveDepthBias(
				referenceDepth,
				range,
				MaximumNdcBias * 2.0f
			),
			MaximumNdcBias
		)
	);
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
