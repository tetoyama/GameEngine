#include <cassert>

#include "Engine/Scene/Component/LightComponentDefaults.h"
#include "Engine/Scene/System/Render/Lighting/LightGpuSubmissionPolicy.h"
#include "Engine/Scene/System/Render/Lighting/PackedLightEntryTraversal.h"

int main(){
	const LIGHT defaults = LightComponentDefaults::Create();
	assert(defaults.Enable != FALSE);
	assert(defaults.LightType == LIGHT_TYPE_POINT);
	assert(defaults.CastShadow == FALSE);
	assert(LightGpuSubmissionPolicy::ShouldSubmitLighting(defaults));
	assert(!LightGpuSubmissionPolicy::ShouldExpandShadowEntries(defaults));

	LIGHT shadowPoint = defaults;
	shadowPoint.CastShadow = TRUE;
	shadowPoint.Dummy = -1;
	shadowPoint.Position.w = 6.0f;
	assert(LightGpuSubmissionPolicy::ShouldExpandShadowEntries(shadowPoint));

	const LIGHT unshadowed =
		LightGpuSubmissionPolicy::MakeUnshadowedLogicalEntry(shadowPoint);
	assert(unshadowed.Enable != FALSE);
	assert(unshadowed.CastShadow == FALSE);
	assert(unshadowed.Dummy == 0);
	assert(unshadowed.Position.w == 0.0f);

	LightBuffer packed{};
	packed.ActiveLightCount = 5;
	packed.Lights[0].Enable = TRUE;
	packed.Lights[0].CastShadow = TRUE;
	packed.Lights[0].LightType = LIGHT_TYPE_DIRECTIONAL_CSM;
	packed.Lights[0].Dummy = 1;
	packed.Lights[0].Position.w = 4.0f;
	for(int index = 1; index < 4; ++index){
		packed.Lights[index] = packed.Lights[0];
		packed.Lights[index].Dummy = index + 1;
	}
	packed.Lights[4] = defaults;

	assert(PackedLightEntryTraversal::CountLogicalLights(packed) == 2);
	assert(
		PackedLightEntryTraversal::CountShadowCastingLogicalLights(packed) == 1
	);
	assert(
		PackedLightEntryTraversal::ResolveEntryCountForLogicalLimit(packed, 1) == 4
	);
	assert(
		PackedLightEntryTraversal::ResolveEntryCountForLogicalLimit(packed, 2) == 5
	);

	return 0;
}
