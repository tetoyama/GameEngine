#pragma once

#include "Shader/common.hlsl"
#include "Shader/commonDefine.h"

namespace LightComponentDefaults {

inline LIGHT Create() noexcept {
	LIGHT light{};
	light.Enable = TRUE;
	light.LightType = LIGHT_TYPE_POINT;
	light.CastShadow = FALSE;
	light.Ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
	light.Diffuse = float4(1.0f, 1.0f, 1.0f, 1.0f);
	light.Param = float4(10.0f, 0.0f, 0.0f, DEPTH_BIAS_CONSTANT);
	return light;
}

} // namespace LightComponentDefaults
