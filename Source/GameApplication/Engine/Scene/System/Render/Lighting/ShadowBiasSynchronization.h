#pragma once

#include "ShadowBiasPolicy.h"

namespace ShadowBiasSynchronization {

inline void Apply(LIGHT& light) noexcept {
	ShadowBiasPolicy::Sanitize(light.ShadowBias);
	light.Param.w =
		ShadowBiasPolicy::GetMode(light.ShadowBias) ==
			ShadowBiasPolicy::Mode::LegacyNdc
		? light.ShadowBias.x
		: 0.0f;
}

} // namespace ShadowBiasSynchronization
