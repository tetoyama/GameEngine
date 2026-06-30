#pragma once

#include "Shader/common.hlsl"

namespace LightGpuSubmissionPolicy {

inline bool ShouldSubmitLighting(const LIGHT& light) noexcept {
	return light.Enable != FALSE;
}

inline bool ShouldExpandShadowEntries(const LIGHT& light) noexcept {
	return ShouldSubmitLighting(light) && light.CastShadow != FALSE;
}

inline LIGHT MakeUnshadowedLogicalEntry(const LIGHT& source) noexcept {
	LIGHT entry = source;
	entry.CastShadow = FALSE;
	entry.Dummy = 0;
	entry.Position.w = 0.0f;
	return entry;
}

} // namespace LightGpuSubmissionPolicy
