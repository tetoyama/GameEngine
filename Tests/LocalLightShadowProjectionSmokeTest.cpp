#include <cassert>
#include <cmath>
#include <limits>

#include "Engine/Scene/System/Render/Lighting/LocalLightShadowProjection.h"

int main(){
	using namespace LocalLightShadowProjection;

	assert(NearPlane > 0.0f);
	assert(MinimumDepthSpan > 0.0f);
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

	return 0;
}
