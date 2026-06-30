#include <cassert>
#include <cstddef>

#include "Shader/common.hlsl"

int main(){
	static_assert(sizeof(CbLightingDebug) == 16);
	static_assert(alignof(CbLightingDebug) == 16);
	static_assert(LIGHTING_DEBUG_PCF_DEFAULT == 0);
	static_assert(LIGHTING_DEBUG_PCF_1X1 == 1);
	static_assert(LIGHTING_DEBUG_PCF_3X3 == 2);
	static_assert(LIGHTING_DEBUG_PCF_5X5 == 3);
	static_assert(
		(LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS &
		 LIGHTING_DEBUG_FLAG_DISABLE_ENVIRONMENT) == 0u
	);

	CbLightingDebug defaults{};
	assert(defaults.LightingDebugFlags == 0u);
	assert(defaults.LightingDebugPcfMode == LIGHTING_DEBUG_PCF_DEFAULT);
	assert(defaults.LightingDebugMaxActiveLights == 0);
	assert(defaults._LightingDebugPad == 0);

	CbLightingDebug diagnostic{};
	diagnostic.LightingDebugFlags =
		LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS |
		LIGHTING_DEBUG_FLAG_DISABLE_ENVIRONMENT;
	diagnostic.LightingDebugPcfMode = LIGHTING_DEBUG_PCF_3X3;
	diagnostic.LightingDebugMaxActiveLights = 1;

	assert((diagnostic.LightingDebugFlags &
		LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS) != 0u);
	assert((diagnostic.LightingDebugFlags &
		LIGHTING_DEBUG_FLAG_DISABLE_ENVIRONMENT) != 0u);
	assert(diagnostic.LightingDebugPcfMode == LIGHTING_DEBUG_PCF_3X3);
	assert(diagnostic.LightingDebugMaxActiveLights == 1);
	return 0;
}
