#pragma once

#include <algorithm>
#include <cstdint>

#include "Backends/ImGui/imgui.h"
#include "Graphics/graphicsContext.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/systemRegistry.h"
#include "System/Render/RenderSystem/renderSystem.h"

namespace LightingDiagnosticUI {

inline void Draw(SceneManager* sceneManager){
	SystemRegistry* registry =
		sceneManager ? sceneManager->GetSystemRegistry() : nullptr;
	RenderSystem* renderSystem = registry
		? registry->GetSystem<RenderSystem>()
		: nullptr;
	if(!renderSystem){
		ImGui::TextDisabled("RenderSystem is not available.");
		return;
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
