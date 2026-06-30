#pragma once

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>

#include "Backends/ImGui/imgui.h"
#include "Editor/UI/LightingDiagnosticCapture.h"
#include "Graphics/graphicsContext.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/systemRegistry.h"
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
	settings._LightingDebugPad = 0;

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

	bool disableShadows =
		(settings.LightingDebugFlags &
		 LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS) != 0u;
	if(ImGui::Checkbox("Disable Shadow Evaluation", &disableShadows)){
		if(disableShadows){
			settings.LightingDebugFlags |=
				LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS;
		}else{
			settings.LightingDebugFlags &=
				~LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS;
		}
	}
	if(ImGui::IsItemHovered()){
		ImGui::SetTooltip(
			"Disables shadow sampling inside Lighting only. "
			"Shadow Map generation remains active for isolated timing."
		);
	}

	bool disableEnvironment =
		(settings.LightingDebugFlags &
		 LIGHTING_DEBUG_FLAG_DISABLE_ENVIRONMENT) != 0u;
	if(ImGui::Checkbox("Disable Environment Reflection", &disableEnvironment)){
		if(disableEnvironment){
			settings.LightingDebugFlags |=
				LIGHTING_DEBUG_FLAG_DISABLE_ENVIRONMENT;
		}else{
			settings.LightingDebugFlags &=
				~LIGHTING_DEBUG_FLAG_DISABLE_ENVIRONMENT;
		}
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
		"Maximum Active Lights",
		&settings.LightingDebugMaxActiveLights,
		0,
		LIGHT_MAX_COUNT
	);
	ImGui::TextDisabled("0 means all active GPU light entries.");

	if(managerContext && managerContext->graphics &&
		managerContext->graphics->GetLight()){
		const LightBuffer* lights = managerContext->graphics->GetLight();
		const int activeCount = (std::clamp)(
			lights->ActiveLightCount,
			0,
			LIGHT_MAX_COUNT
		);
		int shadowEntryCount = 0;
		for(int index = 0; index < activeCount; ++index){
			const LIGHT& light = lights->Lights[index];
			if(light.Enable != 0 && light.CastShadow != 0){
				++shadowEntryCount;
			}
		}
		ImGui::Text("Current GPU ActiveLightCount: %d", activeCount);
		ImGui::Text("Shadow-casting GPU entries: %d", shadowEntryCount);
		ImGui::TextDisabled(
			"CSM cascades and point-light faces are counted as GPU entries."
		);
	}

	auto applyBaseline = [&settings](){
		settings = CbLightingDebug{};
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
		applyBaseline();
		settings.LightingDebugFlags =
			LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS;
	}
	ImGui::SameLine();
	if(ImGui::Button("No Environment")){
		applyBaseline();
		settings.LightingDebugFlags =
			LIGHTING_DEBUG_FLAG_DISABLE_ENVIRONMENT;
	}

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

	if(ImGui::Button("Lights 1")){
		applyLightLimit(1);
	}
	ImGui::SameLine();
	if(ImGui::Button("Lights 2")){
		applyLightLimit(2);
	}
	ImGui::SameLine();
	if(ImGui::Button("Lights 4")){
		applyLightLimit(4);
	}
	ImGui::SameLine();
	if(ImGui::Button("Lights 8")){
		applyLightLimit(8);
	}
	ImGui::SameLine();
	if(ImGui::Button("Lights All")){
		applyLightLimit(0);
	}

	ImGui::SeparatorText("Measurement Capture");
	const bool captureShadowsDisabled =
		(settings.LightingDebugFlags &
		 LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS) != 0u;
	const bool captureEnvironmentDisabled =
		(settings.LightingDebugFlags &
		 LIGHTING_DEBUG_FLAG_DISABLE_ENVIRONMENT) != 0u;
	char captureLabel[160]{};
	if(settings.LightingDebugMaxActiveLights == 0){
		std::snprintf(
			captureLabel,
			sizeof(captureLabel),
			"Shadow:%s / PCF:%s / Environment:%s / Lights:All",
			captureShadowsDisabled ? "OFF" : "ON",
			PcfModeName(settings.LightingDebugPcfMode),
			captureEnvironmentDisabled ? "OFF" : "ON"
		);
	}else{
		std::snprintf(
			captureLabel,
			sizeof(captureLabel),
			"Shadow:%s / PCF:%s / Environment:%s / Lights:%d",
			captureShadowsDisabled ? "OFF" : "ON",
			PcfModeName(settings.LightingDebugPcfMode),
			captureEnvironmentDisabled ? "OFF" : "ON",
			settings.LightingDebugMaxActiveLights
		);
	}
	ImGui::TextDisabled("Current preset: %s", captureLabel);

	if(capture.IsCapturing()){
		ImGui::BeginDisabled();
	}
	if(ImGui::Button("Start 60 Warm-up + 120 Sample Capture")){
		capture.Start(captureLabel, 60, 120);
	}
	if(capture.IsCapturing()){
		ImGui::EndDisabled();
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
	ImGui::BulletText("Baseline");
	ImGui::BulletText("No Shadow");
	ImGui::BulletText("PCF 1x1 / 3x3 / 5x5");
	ImGui::BulletText("No Environment");
	ImGui::BulletText("Max Lights 1 / 2 / 4 / 8 / All");
	ImGui::TextDisabled(
		"Keep resolution, camera, scene, and Post Effect settings unchanged."
	);
}

} // namespace LightingDiagnosticUI
