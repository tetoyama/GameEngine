#include <cassert>

#include "Engine/Scene/Component/LightComponentDefaults.h"
#include "Engine/Scene/System/Render/Lighting/LightGpuSubmissionPolicy.h"
#include "Engine/Scene/System/Render/Lighting/PackedLightEntryTraversal.h"

int main(){
	const LIGHT defaults = LightComponentDefaults::Create();
	assert(defaults.Enable != 0);
	assert(defaults.LightType == LIGHT_TYPE_POINT);
	assert(defaults.CastShadow == 0);
	assert(LightGpuSubmissionPolicy::ShouldSubmitLighting(defaults));
	assert(!LightGpuSubmissionPolicy::ShouldExpandShadowEntries(defaults));

	LIGHT shadowPoint = defaults;
	shadowPoint.CastShadow = 1;
	shadowPoint.Dummy = -1;
	shadowPoint.Position.w = 6.0f;
	shadowPoint.Param.w = DEPTH_BIAS_CONSTANT;
	assert(LightGpuSubmissionPolicy::ShouldExpandShadowEntries(shadowPoint));
	assert(shadowPoint.Param.w == 0.0f);

	shadowPoint.ShadowBias = ShadowBiasPolicy::MakeLegacy(0.002f);
	assert(LightGpuSubmissionPolicy::ShouldSubmitLighting(shadowPoint));
	assert(shadowPoint.Param.w == 0.002f);

	const LIGHT unshadowed =
		LightGpuSubmissionPolicy::MakeUnshadowedLogicalEntry(shadowPoint);
	assert(unshadowed.Enable != 0);
	assert(unshadowed.CastShadow == 0);
	assert(unshadowed.Dummy == 0);
	assert(unshadowed.Position.w == 0.0f);
	assert(unshadowed.Param.w == 0.002f);

	LightBuffer packed{};
	packed.ActiveLightCount = 5;
	packed.Lights[0].Enable = 1;
	packed.Lights[0].CastShadow = 1;
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
