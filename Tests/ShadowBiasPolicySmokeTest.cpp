#include <cassert>
#include <cmath>
#include <limits>

#include "Engine/Scene/System/Render/Lighting/ShadowBiasPolicy.h"
#include "Engine/Scene/System/Render/Lighting/ShadowBiasSynchronization.h"

int main(){
	using namespace ShadowBiasPolicy;

	float4 legacy = MakeLegacy(0.005f);
	assert(GetMode(legacy) == Mode::LegacyNdc);
	assert(legacy.x == 0.005f);
	assert(legacy.y == 0.0f);

	LIGHT legacyLight{};
	legacyLight.Param.w = 0.25f;
	legacyLight.ShadowBias = legacy;
	ShadowBiasSynchronization::Apply(legacyLight);
	assert(legacyLight.Param.w == legacyLight.ShadowBias.x);

	float4 world = MakeWorldDefaults();
	assert(GetMode(world) == Mode::WorldSpace);
	assert(world.x == SHADOW_BIAS_DEFAULT_WORLD_DEPTH);
	assert(world.y == SHADOW_BIAS_DEFAULT_WORLD_NORMAL);

	LIGHT worldLight{};
	worldLight.Param.w = DEPTH_BIAS_CONSTANT;
	worldLight.ShadowBias = world;
	ShadowBiasSynchronization::Apply(worldLight);
	assert(worldLight.Param.w == 0.0f);

	float4 invalid(
		std::numeric_limits<float>::quiet_NaN(),
		std::numeric_limits<float>::infinity(),
		static_cast<float>(Mode::WorldSpace),
		5.0f
	);
	Sanitize(invalid);
	assert(invalid.x == 0.0f);
	assert(invalid.y == 0.0f);
	assert(invalid.w == 0.0f);

	float4 excessiveLegacy = MakeLegacy(100.0f);
	assert(excessiveLegacy.x == MaximumLegacyNdcBias);

	float4 modeSwitch = MakeLegacy(0.002f);
	SetMode(modeSwitch, Mode::WorldSpace, 0.002f);
	assert(GetMode(modeSwitch) == Mode::WorldSpace);
	SetMode(modeSwitch, Mode::LegacyNdc, 0.002f);
	assert(GetMode(modeSwitch) == Mode::LegacyNdc);
	assert(std::abs(modeSwitch.x - 0.002f) < 0.000001f);
	return 0;
}
