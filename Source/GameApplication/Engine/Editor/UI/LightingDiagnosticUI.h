#pragma once

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>

#include "Backends/ImGui/imgui.h"
#include "Editor/UI/LightingDiagnosticCapture.h"
#include "Graphics/graphicsContext.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/systemRegistry.h"
#include "System/Render/Lighting/PackedLightEntryTraversal.h"
#include "System/Render/RenderSystem/renderSystem.h"

namespace LightingDiagnosticUI {

inline const char* PcfModeName(int mode){
	switch(mode){
		case LIGHTING_DEBUG_PCF_DEFAULT: return "Default";
		case LIGHTING_DEBUG_PCF_1X1: return "1x1";
		case LIGHTING_DEBUG_PCF_3X3: return "3x3";
		case LIGHTING_DEBUG_PCF_5X5: return "5x5";
	}
	return "Invalid";
}

inline bool HasFlag(const CbLightingDebug& settings, unsigned int flag){
	return (settings.LightingDebugFlags & flag) != 0u;
}

inline void SetFlag(CbLightingDebug& settings, unsigned int flag, bool enabled){
	if(enabled){
		settings.LightingDebugFlags |= flag;
	}else{
		settings.LightingDebugFlags &= ~flag;
	}
}

inline void Draw(
	SceneManager* sceneManager,
	const std::vector<GpuFrameTimingResult>* resolvedGpuTimings = nullptr
){
	SystemRegistry* registry =
		sceneManager ? sceneManager->GetSystemRegistry() : nullptr;
	RenderSystem* renderSystem = registry
		? registry->GetSystem<RenderSystem>()
		: nullptr;
	if(!renderSystem){
		ImGui::TextDisabled("RenderSystem is not available.");
		return;
	}

	static LightingDiagnosticCapture capture;
	if(resolvedGpuTimings){
		capture.Consume(std::span<const GpuFrameTimingResult>(
			resolvedGpuTimings->data(),
			resolvedGpuTimings->size()
		));
	}

	CbLightingDebug& settings = renderSystem->GetLightingDebugSettings();
	settings.LightingDebugMaxActiveLights = (std::clamp)(
		settings.LightingDebugMaxActiveLights,
		0,
		LIGHT_MAX_COUNT
	);
	settings.LightingDebugPcfMode = (std::clamp)(
		settings.LightingDebugPcfMode,
		LIGHTING_DEBUG_PCF_DEFAULT,
		LIGHTING_DEBUG_PCF_5X5
	);
	if(!std::isfinite(settings.LightingDebugCsmBiasScale) ||
		settings.LightingDebugCsmBiasScale < 0.0f){
		settings.LightingDebugCsmBiasScale = 0.0f;
	}
	settings.LightingDebugCsmBiasScale = (std::min)(
		settings.LightingDebugCsmBiasScale,
		8.0f
	);
	if(!std::isfinite(settings.LightingDebugCsmTexelBiasScale) ||
		settings.LightingDebugCsmTexelBiasScale < 0.0f){
		settings.LightingDebugCsmTexelBiasScale = 0.0f;
	}
	settings.LightingDebugCsmTexelBiasScale = (std::min)(
		settings.LightingDebugCsmTexelBiasScale,
		4.0f
	);
	settings._LightingDebugPad = float3(0.0f, 0.0f, 0.0f);

	ImGui::SeparatorText("Lighting GPU Diagnostics");
	ImGui::TextWrapped(
		"Runtime-only A/B controls. These settings affect Player and Editor "
		"Lighting Passes and are not saved to Scene or Project YAML."
	);
	ImGui::TextDisabled(
		"Material dispatch remains material-specific. No materialID switch is used."
	);

	SceneManagerContext* managerContext =
		sceneManager ? sceneManager->GetContext() : nullptr;
	if(managerContext){
		const int playerWidth = (std::max)(
			0,
			static_cast<int>(managerContext->PlayerScreenSize.x)
		);
		const int playerHeight = (std::max)(
			0,
			static_cast<int>(managerContext->PlayerScreenSize.y)
		);
		const int editorWidth = (std::max)(
			0,
			static_cast<int>(managerContext->EditorScreenSize.x)
		);
		const int editorHeight = (std::max)(
			0,
			static_cast<int>(managerContext->EditorScreenSize.y)
		);
		const std::uint64_t playerPixels =
			static_cast<std::uint64_t>(playerWidth) *
			static_cast<std::uint64_t>(playerHeight);
		const std::uint64_t editorPixels =
			static_cast<std::uint64_t>(editorWidth) *
			static_cast<std::uint64_t>(editorHeight);

		ImGui::Text(
			"Player Resolution: %d x %d / %llu pixels",
			playerWidth,
			playerHeight,
			static_cast<unsigned long long>(playerPixels)
		);
		ImGui::Text(
			"Editor Resolution: %d x %d / %llu pixels",
			editorWidth,
			editorHeight,
			static_cast<unsigned long long>(editorPixels)
		);
	}

	bool disableShadows = HasFlag(
		settings,
		LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS
	);
	if(ImGui::Checkbox("Disable All Shadow Evaluation", &disableShadows)){
		SetFlag(
			settings,
			LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS,
			disableShadows
		);
	}
	if(ImGui::IsItemHovered()){
		ImGui::SetTooltip(
			"Disables shadow sampling inside Lighting only. "
			"Shadow Map generation remains active."
		);
	}

	bool disableCsmShadows = HasFlag(
		settings,
		LIGHTING_DEBUG_FLAG_DISABLE_CSM_SHADOWS
	);
	if(ImGui::Checkbox("Disable CSM Shadow Evaluation", &disableCsmShadows)){
		SetFlag(
			settings,
			LIGHTING_DEBUG_FLAG_DISABLE_CSM_SHADOWS,
			disableCsmShadows
		);
	}

	bool disablePointShadows = HasFlag(
		settings,
		LIGHTING_DEBUG_FLAG_DISABLE_POINT_SHADOWS
	);
	if(ImGui::Checkbox("Disable Point Shadow Evaluation", &disablePointShadows)){
		SetFlag(
			settings,
			LIGHTING_DEBUG_FLAG_DISABLE_POINT_SHADOWS,
			disablePointShadows
		);
	}

	bool showCsmCascades = HasFlag(
		settings,
		LIGHTING_DEBUG_FLAG_SHOW_CSM_CASCADES
	);
	if(ImGui::Checkbox("Show CSM Cascade Coloring", &showCsmCascades)){
		SetFlag(
			settings,
			LIGHTING_DEBUG_FLAG_SHOW_CSM_CASCADES,
			showCsmCascades
		);
	}
	if(ImGui::IsItemHovered()){
		ImGui::SetTooltip(
			"Tints pixels by the cascade actually sampled: "
			"0=Red 1=Green 2=Blue 3=Yellow. Shadow pattern stays visible. "
			"Integrity fallthrough may sample a later cascade (Step19A6)."
		);
	}

	bool disableCsmTexelBias = HasFlag(
		settings,
		LIGHTING_DEBUG_FLAG_DISABLE_CSM_TEXEL_BIAS
	);
	if(ImGui::Checkbox("Disable CSM Texel Proportional Bias", &disableCsmTexelBias)){
		SetFlag(
			settings,
			LIGHTING_DEBUG_FLAG_DISABLE_CSM_TEXEL_BIAS,
			disableCsmTexelBias
		);
	}
	if(ImGui::IsItemHovered()){
		ImGui::SetTooltip(
			"A/B switch. Texel proportional bias is ON by default "
			"(Step19A5 contract, verified 2026-07-09). "
			"Param.w is not reinterpreted."
		);
	}

	ImGui::SetNextItemWidth(260.0f);
	ImGui::SliderFloat(
		"CSM Texel Bias Scale",
		&settings.LightingDebugCsmTexelBiasScale,
		0.0f,
		4.0f,
		settings.LightingDebugCsmTexelBiasScale <= 0.0f ? "Default (x1)" : "x%.2f"
	);
	if(ImGui::IsItemHovered()){
		ImGui::SetTooltip(
			"Scales only the texel proportional bias floor. "
			"Lower if contact shadows detach (Peter Panning), "
			"raise if far cascade acne returns. 0 means default x1."
		);
	}

	ImGui::SetNextItemWidth(260.0f);
	ImGui::SliderFloat(
		"CSM Bias Debug Scale",
		&settings.LightingDebugCsmBiasScale,
		0.0f,
		8.0f,
		settings.LightingDebugCsmBiasScale <= 0.0f ? "Default (x1)" : "x%.2f"
	);
	if(ImGui::IsItemHovered()){
		ImGui::SetTooltip(
			"Temporary multiplier on CSM Param.w for isolation (Step19A6). "
			"0 means default x1. Result is still capped by MAX_NDC_BIAS. "
			"Not saved to Scene."
		);
	}

	bool disableEnvironment = HasFlag(
		settings,
		LIGHTING_DEBUG_FLAG_DISABLE_ENVIRONMENT
	);
	if(ImGui::Checkbox("Disable Environment Reflection", &disableEnvironment)){
		SetFlag(
			settings,
			LIGHTING_DEBUG_FLAG_DISABLE_ENVIRONMENT,
			disableEnvironment
		);
	}

	static constexpr const char* kPcfModes =
		"Material Default\0"
		"1x1 (Radius 0)\0"
		"3x3 (Radius 1)\0"
		"5x5 (Radius 2)\0";
	ImGui::SetNextItemWidth(260.0f);
	ImGui::Combo(
		"PCF Override",
		&settings.LightingDebugPcfMode,
		kPcfModes
	);

	ImGui::SetNextItemWidth(260.0f);
	ImGui::SliderInt(
		"Maximum Logical Lights",
		&settings.LightingDebugMaxActiveLights,
		0,
		LIGHT_MAX_COUNT
	);
	ImGui::TextDisabled(
		"0 means all logical lights. CSM cascades and point faces remain grouped."
	);

	if(managerContext && managerContext->graphics &&
		managerContext->graphics->GetLight()){
		const LightBuffer* lights = managerContext->graphics->GetLight();
		const int activeEntryCount = (std::clamp)(
			lights->ActiveLightCount,
			0,
			LIGHT_MAX_COUNT
		);
		const int logicalLightCount =
			PackedLightEntryTraversal::CountLogicalLights(*lights);
		const int shadowLogicalLightCount =
			PackedLightEntryTraversal::CountShadowCastingLogicalLights(*lights);
		int shadowEntryCount = 0;
		for(int index = 0; index < activeEntryCount; ++index){
			const LIGHT& light = lights->Lights[index];
			if(light.Enable != 0 && light.CastShadow != 0){
				++shadowEntryCount;
			}
		}
		ImGui::Text("Packed GPU entries: %d", activeEntryCount);
		ImGui::Text("Logical lights: %d", logicalLightCount);
		ImGui::Text(
			"Shadow logical lights: %d / Shadow entries: %d",
			shadowLogicalLightCount,
			shadowEntryCount
		);

		if(ImGui::TreeNodeEx("Logical Light Layout", ImGuiTreeNodeFlags_DefaultOpen)){
			int entryIndex = 0;
			int logicalIndex = 0;
			while(entryIndex < activeEntryCount){
				const LIGHT& light = lights->Lights[entryIndex];
				const int span = PackedLightEntryTraversal::ResolveEntrySpan(
					light,
					activeEntryCount - entryIndex
				);
				ImGui::BulletText(
					"Logical %d: %s / Entry %d / Span %d / Shadow %s",
					logicalIndex,
					PackedLightEntryTraversal::LightTypeName(light.LightType),
					entryIndex,
					span,
					light.CastShadow != 0 ? "ON" : "OFF"
				);
				entryIndex += span;
				++logicalIndex;
			}
			ImGui::TreePop();
		}
	}

	auto applyBaseline = [&settings](){
		settings = CbLightingDebug{};
	};
	auto applyFlagOnly = [&settings](unsigned int flag){
		settings = CbLightingDebug{};
		settings.LightingDebugFlags = flag;
	};
	auto applyPcf = [&settings](int pcfMode){
		settings = CbLightingDebug{};
		settings.LightingDebugPcfMode = pcfMode;
	};
	auto applyLightLimit = [&settings](int maxLights){
		settings = CbLightingDebug{};
		settings.LightingDebugMaxActiveLights = maxLights;
	};

	ImGui::SeparatorText("A/B Presets");
	if(ImGui::Button("Baseline")){
		applyBaseline();
	}
	ImGui::SameLine();
	if(ImGui::Button("No Shadow")){
		applyFlagOnly(LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS);
	}
	ImGui::SameLine();
	if(ImGui::Button("No CSM Shadow")){
		applyFlagOnly(LIGHTING_DEBUG_FLAG_DISABLE_CSM_SHADOWS);
	}
	ImGui::SameLine();
	if(ImGui::Button("No Point Shadow")){
		applyFlagOnly(LIGHTING_DEBUG_FLAG_DISABLE_POINT_SHADOWS);
	}

	if(ImGui::Button("No Environment")){
		applyFlagOnly(LIGHTING_DEBUG_FLAG_DISABLE_ENVIRONMENT);
	}
	ImGui::SameLine();
	if(ImGui::Button("PCF 1x1")){
		applyPcf(LIGHTING_DEBUG_PCF_1X1);
	}
	ImGui::SameLine();
	if(ImGui::Button("PCF 3x3")){
		applyPcf(LIGHTING_DEBUG_PCF_3X3);
	}
	ImGui::SameLine();
	if(ImGui::Button("PCF 5x5")){
		applyPcf(LIGHTING_DEBUG_PCF_5X5);
	}

	auto applyCsmBiasScale = [&settings](float scale){
		settings = CbLightingDebug{};
		settings.LightingDebugCsmBiasScale = scale;
	};
	if(ImGui::Button("CSM Cascade View")){
		applyFlagOnly(LIGHTING_DEBUG_FLAG_SHOW_CSM_CASCADES);
	}
	ImGui::SameLine();
	if(ImGui::Button("No CSM Texel Bias")){
		applyFlagOnly(LIGHTING_DEBUG_FLAG_DISABLE_CSM_TEXEL_BIAS);
	}
	ImGui::SameLine();
	if(ImGui::Button("CSM Bias x2")){
		applyCsmBiasScale(2.0f);
	}
	ImGui::SameLine();
	if(ImGui::Button("CSM Bias x4")){
		applyCsmBiasScale(4.0f);
	}

	if(ImGui::Button("Lights 1")){
		applyLightLimit(1);
	}
	ImGui::SameLine();
	if(ImGui::Button("Lights 2")){
		applyLightLimit(2);
	}
	ImGui::SameLine();
	if(ImGui::Button("Lights All")){
		applyLightLimit(0);
	}

	ImGui::SeparatorText("Measurement Capture");
	const bool captureShadowsDisabled = HasFlag(
		settings,
		LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS
	);
	const bool captureCsmDisabled = HasFlag(
		settings,
		LIGHTING_DEBUG_FLAG_DISABLE_CSM_SHADOWS
	);
	const bool capturePointDisabled = HasFlag(
		settings,
		LIGHTING_DEBUG_FLAG_DISABLE_POINT_SHADOWS
	);
	const bool captureEnvironmentDisabled = HasFlag(
		settings,
		LIGHTING_DEBUG_FLAG_DISABLE_ENVIRONMENT
	);
	char captureLabel[256]{};
	if(settings.LightingDebugMaxActiveLights == 0){
		std::snprintf(
			captureLabel,
			sizeof(captureLabel),
			"Shadow:%s / CSM:%s / Point:%s / PCF:%s / Env:%s / Logical:All",
			captureShadowsDisabled ? "OFF" : "ON",
			captureCsmDisabled ? "OFF" : "ON",
			capturePointDisabled ? "OFF" : "ON",
			PcfModeName(settings.LightingDebugPcfMode),
			captureEnvironmentDisabled ? "OFF" : "ON"
		);
	}else{
		std::snprintf(
			captureLabel,
			sizeof(captureLabel),
			"Shadow:%s / CSM:%s / Point:%s / PCF:%s / Env:%s / Logical:%d",
			captureShadowsDisabled ? "OFF" : "ON",
			captureCsmDisabled ? "OFF" : "ON",
			capturePointDisabled ? "OFF" : "ON",
			PcfModeName(settings.LightingDebugPcfMode),
			captureEnvironmentDisabled ? "OFF" : "ON",
			settings.LightingDebugMaxActiveLights
		);
	}
	ImGui::TextDisabled("Current preset: %s", captureLabel);

	const bool wasCapturingBeforeStart = capture.IsCapturing();
	if(wasCapturingBeforeStart){
		ImGui::BeginDisabled();
	}
	if(ImGui::Button("Start 60 Warm-up + 120 Sample Capture")){
		capture.Start(captureLabel, 60, 120);
	}
	if(wasCapturingBeforeStart){
		ImGui::EndDisabled();
	}
	if(capture.IsCapturing()){
		ImGui::SameLine();
		if(ImGui::Button("Cancel Capture")){
			capture.Cancel();
		}
	}

	if(capture.IsCapturing()){
		if(capture.WarmupRemaining() > 0){
			const float progress =
				1.0f - static_cast<float>(capture.WarmupRemaining()) / 60.0f;
			char overlay[64]{};
			std::snprintf(
				overlay,
				sizeof(overlay),
				"Warm-up: %zu / 60",
				60 - capture.WarmupRemaining()
			);
			ImGui::ProgressBar(progress, ImVec2(-FLT_MIN, 0.0f), overlay);
		}else{
			const float progress = static_cast<float>(capture.SamplesCollected()) /
				static_cast<float>(capture.TargetSampleCount());
			char overlay[64]{};
			std::snprintf(
				overlay,
				sizeof(overlay),
				"Samples: %zu / %zu",
				capture.SamplesCollected(),
				capture.TargetSampleCount()
			);
			ImGui::ProgressBar(progress, ImVec2(-FLT_MIN, 0.0f), overlay);
		}
	}

	const LightingDiagnosticCapture::Summary& summary = capture.GetSummary();
	if(summary.valid){
		ImGui::TextWrapped("Result: %s", summary.label.c_str());
		if(ImGui::BeginTable(
			"LightingDiagnosticCaptureResult",
			3,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
		)){
			ImGui::TableSetupColumn("Scope");
			ImGui::TableSetupColumn("Average (ms)");
			ImGui::TableSetupColumn("P95 (ms)");
			ImGui::TableHeadersRow();

			auto row = [](const char* name,
				const LightingDiagnosticCapture::MetricSummary& metric){
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(name);
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%.4f", metric.averageMilliseconds);
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%.4f", metric.p95Milliseconds);
			};
			row("GPU Frame", summary.gpuFrame);
			row("Player Lighting", summary.playerLighting);
			row("Player Shadow", summary.playerShadow);
			row("Player Post Effect", summary.playerPostEffect);
			ImGui::EndTable();
		}
		ImGui::TextDisabled("Resolved samples: %zu", summary.sampleCount);
	}

	ImGui::SeparatorText("Measurement Order");
	ImGui::BulletText("Baseline / No Shadow");
	ImGui::BulletText("No CSM Shadow / No Point Shadow");
	ImGui::BulletText("Logical Lights 1 / 2");
	ImGui::BulletText("PCF 1x1 / 3x3 / 5x5");
	ImGui::TextDisabled(
		"Keep resolution, camera, scene, and Post Effect settings unchanged."
	);
}

} // namespace LightingDiagnosticUI
