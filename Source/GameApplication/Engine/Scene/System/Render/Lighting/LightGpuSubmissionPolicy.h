#pragma once

#include "Shader/common.hlsl"

namespace LightGpuSubmissionPolicy {

inline bool ShouldSubmitLighting(const LIGHT& light) noexcept {
	return light.Enable != 0;
}

inline bool ShouldExpandShadowEntries(const LIGHT& light) noexcept {
	return ShouldSubmitLighting(light) && light.CastShadow != 0;
}

inline LIGHT MakeUnshadowedLogicalEntry(const LIGHT& source) noexcept {
	LIGHT entry = source;
	entry.CastShadow = 0;
	entry.Dummy = 0;
	entry.Position.w = 0.0f;
	return entry;
}

} // namespace LightGpuSubmissionPolicy
