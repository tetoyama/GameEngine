#pragma once

#include "Shader/common.hlsl"
#include "ShadowBiasSynchronization.h"

namespace LightGpuSubmissionPolicy {

inline void PrepareForSubmission(LIGHT& light) noexcept {
	ShadowBiasSynchronization::Apply(light);
}

inline bool ShouldSubmitLighting(LIGHT& light) noexcept {
	PrepareForSubmission(light);
	return light.Enable != 0;
}

inline bool ShouldSubmitLighting(const LIGHT& light) noexcept {
	return light.Enable != 0;
}

inline bool ShouldExpandShadowEntries(LIGHT& light) noexcept {
	PrepareForSubmission(light);
	return ShouldSubmitLighting(static_cast<const LIGHT&>(light)) &&
		light.CastShadow != 0;
}

inline bool ShouldExpandShadowEntries(const LIGHT& light) noexcept {
	return ShouldSubmitLighting(light) && light.CastShadow != 0;
}

inline LIGHT MakeUnshadowedLogicalEntry(const LIGHT& source) noexcept {
	LIGHT entry = source;
	PrepareForSubmission(entry);
	entry.CastShadow = 0;
	entry.Dummy = 0;
	entry.Position.w = 0.0f;
	return entry;
}

} // namespace LightGpuSubmissionPolicy
