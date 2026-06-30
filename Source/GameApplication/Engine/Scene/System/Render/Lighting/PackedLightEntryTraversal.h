#pragma once

#include <algorithm>
#include <cmath>

#include "Shader/common.hlsl"

namespace PackedLightEntryTraversal {

inline const char* LightTypeName(int lightType) noexcept {
	switch(lightType){
		case LIGHT_TYPE_DIRECTIONAL: return "Directional";
		case LIGHT_TYPE_DIRECTIONAL_CSM: return "Directional CSM";
		case LIGHT_TYPE_POINT: return "Point";
		case LIGHT_TYPE_SPOT: return "Spot";
		case LIGHT_TYPE_NONE: return "None";
		default: return "Unknown";
	}
}

inline int ResolveEntrySpan(
	const LIGHT& light,
	int remainingEntries
) noexcept {
	remainingEntries = (std::max)(remainingEntries, 1);
	int span = 1;
	if(light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM && light.Dummy == 1){
		span = (std::max)(1, static_cast<int>(std::lround(light.Position.w)));
	}else if(light.LightType == LIGHT_TYPE_POINT && light.Dummy == -1){
		span = (std::max)(1, static_cast<int>(std::lround(light.Position.w)));
	}
	return (std::clamp)(span, 1, remainingEntries);
}

inline int CountLogicalLights(const LightBuffer& lights) noexcept {
	const int activeEntries = (std::clamp)(
		lights.ActiveLightCount,
		0,
		LIGHT_MAX_COUNT
	);
	int entryIndex = 0;
	int logicalCount = 0;
	while(entryIndex < activeEntries){
		const LIGHT& light = lights.Lights[entryIndex];
		entryIndex += ResolveEntrySpan(light, activeEntries - entryIndex);
		++logicalCount;
	}
	return logicalCount;
}

inline int CountShadowCastingLogicalLights(const LightBuffer& lights) noexcept {
	const int activeEntries = (std::clamp)(
		lights.ActiveLightCount,
		0,
		LIGHT_MAX_COUNT
	);
	int entryIndex = 0;
	int logicalShadowCount = 0;
	while(entryIndex < activeEntries){
		const LIGHT& light = lights.Lights[entryIndex];
		const int span = ResolveEntrySpan(
			light,
			activeEntries - entryIndex
		);
		if(light.Enable != 0 && light.CastShadow != 0){
			++logicalShadowCount;
		}
		entryIndex += span;
	}
	return logicalShadowCount;
}

inline int ResolveEntryCountForLogicalLimit(
	const LightBuffer& lights,
	int maxLogicalLights
) noexcept {
	const int activeEntries = (std::clamp)(
		lights.ActiveLightCount,
		0,
		LIGHT_MAX_COUNT
	);
	if(maxLogicalLights <= 0){
		return activeEntries;
	}

	int entryIndex = 0;
	int logicalCount = 0;
	while(entryIndex < activeEntries && logicalCount < maxLogicalLights){
		const LIGHT& light = lights.Lights[entryIndex];
		entryIndex += ResolveEntrySpan(light, activeEntries - entryIndex);
		++logicalCount;
	}
	return entryIndex;
}

} // namespace PackedLightEntryTraversal
