#pragma once

#include <algorithm>
#include <cmath>

#include "Shader/common.hlsl"

namespace PackedLightEntryTraversal {

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
