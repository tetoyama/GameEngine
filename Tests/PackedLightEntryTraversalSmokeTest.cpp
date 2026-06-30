#include <cassert>
#include <string_view>

#include "Engine/Scene/System/Render/Lighting/PackedLightEntryTraversal.h"

int main(){
	using PackedLightEntryTraversal::LightTypeName;
	assert(std::string_view(LightTypeName(LIGHT_TYPE_DIRECTIONAL_CSM)) ==
		"Directional CSM");
	assert(std::string_view(LightTypeName(LIGHT_TYPE_POINT)) == "Point");

	LightBuffer lights{};
	lights.ActiveLightCount = 10;

	lights.Lights[0].Enable = 1;
	lights.Lights[0].CastShadow = 1;
	lights.Lights[0].LightType = LIGHT_TYPE_DIRECTIONAL_CSM;
	lights.Lights[0].Dummy = 1;
	lights.Lights[0].Position.w = 4.0f;
	for(int index = 1; index < 4; ++index){
		lights.Lights[index].Enable = 1;
		lights.Lights[index].CastShadow = 1;
		lights.Lights[index].LightType = LIGHT_TYPE_DIRECTIONAL_CSM;
		lights.Lights[index].Dummy = index + 1;
	}

	lights.Lights[4].Enable = 1;
	lights.Lights[4].CastShadow = 1;
	lights.Lights[4].LightType = LIGHT_TYPE_POINT;
	lights.Lights[4].Dummy = -1;
	lights.Lights[4].Position.w = 6.0f;
	for(int index = 5; index < 10; ++index){
		lights.Lights[index].Enable = 1;
		lights.Lights[index].CastShadow = 1;
		lights.Lights[index].LightType = LIGHT_TYPE_POINT;
		lights.Lights[index].Dummy = -(index - 3);
	}

	assert(PackedLightEntryTraversal::CountLogicalLights(lights) == 2);
	assert(
		PackedLightEntryTraversal::CountShadowCastingLogicalLights(lights) == 2
	);
	assert(
		PackedLightEntryTraversal::ResolveEntryCountForLogicalLimit(lights, 0) == 10
	);
	assert(
		PackedLightEntryTraversal::ResolveEntryCountForLogicalLimit(lights, 1) == 4
	);
	assert(
		PackedLightEntryTraversal::ResolveEntryCountForLogicalLimit(lights, 2) == 10
	);
	assert(
		PackedLightEntryTraversal::ResolveEntryCountForLogicalLimit(lights, 8) == 10
	);

	LightBuffer truncated{};
	truncated.ActiveLightCount = 3;
	truncated.Lights[0].Enable = 1;
	truncated.Lights[0].CastShadow = 1;
	truncated.Lights[0].LightType = LIGHT_TYPE_DIRECTIONAL_CSM;
	truncated.Lights[0].Dummy = 1;
	truncated.Lights[0].Position.w = 4.0f;
	assert(PackedLightEntryTraversal::CountLogicalLights(truncated) == 1);
	assert(
		PackedLightEntryTraversal::ResolveEntryCountForLogicalLimit(truncated, 1) == 3
	);

	return 0;
}
