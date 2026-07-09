#include <cassert>
#include <cmath>
#include <cstddef>
#include <vector>

#include "Shader/common.hlsl"
#include "Engine/Editor/UI/LightingDiagnosticCapture.h"

int main(){
	static_assert(sizeof(CbLightingDebug) == 32);
	static_assert(alignof(CbLightingDebug) == 16);
	static_assert(LIGHTING_DEBUG_PCF_DEFAULT == 0);
	static_assert(LIGHTING_DEBUG_PCF_1X1 == 1);
	static_assert(LIGHTING_DEBUG_PCF_3X3 == 2);
	static_assert(LIGHTING_DEBUG_PCF_5X5 == 3);

	const unsigned int allFlags =
		LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS |
		LIGHTING_DEBUG_FLAG_DISABLE_ENVIRONMENT |
		LIGHTING_DEBUG_FLAG_DISABLE_CSM_SHADOWS |
		LIGHTING_DEBUG_FLAG_DISABLE_POINT_SHADOWS |
		LIGHTING_DEBUG_FLAG_SHOW_CSM_CASCADES |
		LIGHTING_DEBUG_FLAG_DISABLE_CSM_TEXEL_BIAS;
	static_assert(
		(LIGHTING_DEBUG_FLAG_DISABLE_CSM_SHADOWS &
		 LIGHTING_DEBUG_FLAG_DISABLE_POINT_SHADOWS) == 0u
	);
	static_assert(
		(LIGHTING_DEBUG_FLAG_SHOW_CSM_CASCADES &
		 LIGHTING_DEBUG_FLAG_DISABLE_CSM_TEXEL_BIAS) == 0u
	);
	static_assert(LIGHTING_DEBUG_FLAG_SHOW_CSM_CASCADES == (1u << 4));
	// 全フィールド0=通常描画: Texel Biasは既定ONなので無効化側をフラグにする。
	static_assert(LIGHTING_DEBUG_FLAG_DISABLE_CSM_TEXEL_BIAS == (1u << 5));
	assert(allFlags == 0x3fu);

	CbLightingDebug defaults{};
	assert(defaults.LightingDebugFlags == 0u);
	assert(defaults.LightingDebugPcfMode == LIGHTING_DEBUG_PCF_DEFAULT);
	assert(defaults.LightingDebugMaxActiveLights == 0);
	// 全フィールド0=通常描画契約: 各Scale 0は既定倍率x1として解釈される。
	assert(defaults.LightingDebugCsmBiasScale == 0.0f);
	assert(defaults.LightingDebugCsmTexelBiasScale == 0.0f);

	CbLightingDebug diagnostic{};
	diagnostic.LightingDebugFlags = allFlags;
	diagnostic.LightingDebugPcfMode = LIGHTING_DEBUG_PCF_3X3;
	diagnostic.LightingDebugMaxActiveLights = 1;

	assert((diagnostic.LightingDebugFlags &
		LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS) != 0u);
	assert((diagnostic.LightingDebugFlags &
		LIGHTING_DEBUG_FLAG_DISABLE_ENVIRONMENT) != 0u);
	assert((diagnostic.LightingDebugFlags &
		LIGHTING_DEBUG_FLAG_DISABLE_CSM_SHADOWS) != 0u);
	assert((diagnostic.LightingDebugFlags &
		LIGHTING_DEBUG_FLAG_DISABLE_POINT_SHADOWS) != 0u);
	assert(diagnostic.LightingDebugPcfMode == LIGHTING_DEBUG_PCF_3X3);
	assert(diagnostic.LightingDebugMaxActiveLights == 1);

	LightingDiagnosticCapture capture;
	capture.Start("capture", 0, 2);
	std::vector<GpuFrameTimingResult> samples(2);
	for(std::size_t index = 0; index < samples.size(); ++index){
		GpuFrameTimingResult& sample = samples[index];
		sample.frameSerial = index + 1;
		sample.status = GpuFrameTimingStatus::Resolved;
		sample.seconds = static_cast<double>(index + 1) * 0.010;
		sample.resolvedPassMask =
			GpuPassTimingBit(GpuPassTimingScope::PlayerLighting) |
			GpuPassTimingBit(GpuPassTimingScope::PlayerShadow) |
			GpuPassTimingBit(GpuPassTimingScope::PlayerPostEffect);
		sample.passSeconds[static_cast<std::size_t>(
			GpuPassTimingScope::PlayerLighting)] =
			static_cast<double>(index + 1) * 0.004;
		sample.passSeconds[static_cast<std::size_t>(
			GpuPassTimingScope::PlayerShadow)] =
			static_cast<double>(index + 1) * 0.001;
		sample.passSeconds[static_cast<std::size_t>(
			GpuPassTimingScope::PlayerPostEffect)] =
			static_cast<double>(index + 1) * 0.003;
	}
	capture.Consume(samples);
	const auto& summary = capture.GetSummary();
	const std::size_t lightingIndex = static_cast<std::size_t>(
		GpuPassTimingScope::PlayerLighting
	);
	assert(summary.valid);
	assert(summary.sampleCount == 2);
	assert(summary.passSampleCounts[lightingIndex] == 2);
	assert(std::abs(summary.gpuFrame.averageMilliseconds - 15.0) < 0.000001);
	assert(std::abs(summary.gpuFrame.p95Milliseconds - 20.0) < 0.000001);
	assert(std::abs(summary.passes[lightingIndex].averageMilliseconds - 6.0) < 0.000001);
	return 0;
}
