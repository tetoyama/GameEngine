#pragma once

#include "Shader/common.hlsl"
#include "Shader/commonDefine.h"
#include "System/Render/Lighting/ShadowBiasPolicy.h"

namespace LightComponentDefaults {

inline LIGHT Create() noexcept {
	LIGHT light{};
	light.Enable = 1;
	light.LightType = LIGHT_TYPE_POINT;
	light.CastShadow = 0;
	light.Ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
	light.Diffuse = float4(1.0f, 1.0f, 1.0f, 1.0f);
	light.Param = float4(10.0f, 0.0f, 0.0f, 0.0f);
	light.ShadowBias = ShadowBiasPolicy::MakeWorldDefaults();
	return light;
}

} // namespace LightComponentDefaults
