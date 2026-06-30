#include <cassert>
#include <cmath>
#include <cstddef>
#include <vector>

#include "Shader/common.hlsl"
#include "Engine/Editor/UI/LightingDiagnosticCapture.h"

int main(){
	static_assert(sizeof(CbLightingDebug) == 16);
	static_assert(alignof(CbLightingDebug) == 16);
	static_assert(LIGHTING_DEBUG_PCF_DEFAULT == 0);
	static_assert(LIGHTING_DEBUG_PCF_1X1 == 1);
	static_assert(LIGHTING_DEBUG_PCF_3X3 == 2);
	static_assert(LIGHTING_DEBUG_PCF_5X5 == 3);
	static_assert(
		(LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS &
		 LIGHTING_DEBUG_FLAG_DISABLE_ENVIRONMENT) == 0u
	);

	CbLightingDebug defaults{};
	assert(defaults.LightingDebugFlags == 0u);
	assert(defaults.LightingDebugPcfMode == LIGHTING_DEBUG_PCF_DEFAULT);
	assert(defaults.LightingDebugMaxActiveLights == 0);
	assert(defaults._LightingDebugPad == 0);

	CbLightingDebug diagnostic{};
	diagnostic.LightingDebugFlags =
		LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS |
		LIGHTING_DEBUG_FLAG_DISABLE_ENVIRONMENT;
	diagnostic.LightingDebugPcfMode = LIGHTING_DEBUG_PCF_3X3;
	diagnostic.LightingDebugMaxActiveLights = 1;

	assert((diagnostic.LightingDebugFlags &
		LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS) != 0u);
	assert((diagnostic.LightingDebugFlags &
		LIGHTING_DEBUG_FLAG_DISABLE_ENVIRONMENT) != 0u);
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
	assert(summary.valid);
	assert(summary.sampleCount == 2);
	assert(std::abs(summary.gpuFrame.averageMilliseconds - 15.0) < 0.000001);
	assert(std::abs(summary.gpuFrame.p95Milliseconds - 20.0) < 0.000001);
	assert(std::abs(summary.playerLighting.averageMilliseconds - 6.0) < 0.000001);
	return 0;
}
