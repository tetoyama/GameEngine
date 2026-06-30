#pragma once

#include <algorithm>
#include <cmath>

#include "Shader/common.hlsl"
#include "Shader/commonDefine.h"

namespace ShadowBiasPolicy {

enum class Mode : int {
	LegacyNdc = SHADOW_BIAS_MODE_LEGACY_NDC,
	WorldSpace = SHADOW_BIAS_MODE_WORLD_SPACE
};

inline constexpr float MaximumLegacyNdcBias = 1.0f;
inline constexpr float MaximumWorldBias = 10.0f;

inline float4 MakeLegacy(float legacyNdcBias) noexcept {
	const float value = std::isfinite(legacyNdcBias)
		? (std::clamp)(legacyNdcBias, 0.0f, MaximumLegacyNdcBias)
		: 0.0f;
	return float4(
		value,
		0.0f,
		static_cast<float>(Mode::LegacyNdc),
		0.0f
	);
}

inline float4 MakeWorldDefaults() noexcept {
	return float4(
		SHADOW_BIAS_DEFAULT_WORLD_DEPTH,
		SHADOW_BIAS_DEFAULT_WORLD_NORMAL,
		static_cast<float>(Mode::WorldSpace),
		0.0f
	);
}

inline Mode ResolveMode(float modeValue) noexcept {
	if(std::isfinite(modeValue) &&
	   static_cast<int>(std::lround(modeValue)) ==
		static_cast<int>(Mode::WorldSpace)){
		return Mode::WorldSpace;
	}
	return Mode::LegacyNdc;
}

inline Mode GetMode(const float4& bias) noexcept {
	return ResolveMode(bias.z);
}

inline bool IsWorldSpace(const float4& bias) noexcept {
	return GetMode(bias) == Mode::WorldSpace;
}

inline void Sanitize(float4& bias) noexcept {
	const Mode mode = GetMode(bias);
	const float maximum = mode == Mode::WorldSpace
		? MaximumWorldBias
		: MaximumLegacyNdcBias;

	bias.x = std::isfinite(bias.x)
		? (std::clamp)(bias.x, 0.0f, maximum)
		: 0.0f;
	bias.y = mode == Mode::WorldSpace && std::isfinite(bias.y)
		? (std::clamp)(bias.y, 0.0f, MaximumWorldBias)
		: 0.0f;
	bias.z = static_cast<float>(mode);
	bias.w = 0.0f;
}

inline void SetMode(float4& bias, Mode mode, float legacyNdcFallback) noexcept {
	const Mode previousMode = GetMode(bias);
	if(mode == Mode::WorldSpace){
		bias = MakeWorldDefaults();
		return;
	}

	float fallback = legacyNdcFallback;
	if(previousMode == Mode::WorldSpace &&
	   (!std::isfinite(fallback) || fallback <= 0.0f)){
		fallback = DEPTH_BIAS_CONSTANT;
	}
	bias = MakeLegacy(fallback);
}

} // namespace ShadowBiasPolicy
