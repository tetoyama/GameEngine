// =======================================================================
//
// SystemSettingImplementation.inl
//
// =======================================================================
#pragma once

#include "SystemSetting.h"

#include <algorithm>
#include <array>
#include <cfloat>

#include "Backends/ImGui/imgui.h"
#include "Backends/ImGuiFunc.h"
#include "DebugTools/ImGuiSystem.h"
#include "Editor/UI/LightingDiagnosticUI.h"
#include "Editor/UI/MenuBar.h"
#include "Editor/UI/ScheduleProfileYamlExportUI.h"
#include "Editor/UI/StaticBatchTelemetryUI.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/systemRegistry.h"
#include "Service/Config/configSystem.h"
#include "Service/Graphics/graphicsContext.h"
#include <ImGui/imgui_internal.h>

namespace SystemSettingImplementationDetail {

struct BackendOption {
	RHI::BackendType type;
	const char* label;
	bool selectable;
	const char* note;
};

constexpr std::array<BackendOption, 4> kBackendOptions = {{
	{RHI::BackendType::Direct3D11, "Direct3D 11", true, nullptr},
	{RHI::BackendType::Direct3D12, "Direct3D 12", false, "Renderer bridge is not implemented yet."},
	{RHI::BackendType::Vulkan, "Vulkan", false, "Renderer bridge is not implemented yet."},
	{RHI::BackendType::Null, "Null (Test)", false, "Null backend is for RHI tests only."}
}};

inline const char* GetBackendLabel(RHI::BackendType backend) {
	for(const BackendOption& option : kBackendOptions) {
		if(option.type == backend) return option.label;
	}
	return "Unknown";
}

inline const char* GetFrameLatencyLabel(uint32_t latency) {
	switch(latency) {
		case 1: return "Low Latency";
		case 2: return "Balanced";
		case 3: return "Maximum Throughput";
	}
	return "Unknown";
}

inline bool BeginSettingsTable(const char* id) {
	if(!ImGui::BeginTable(
		id,
		2,
		ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_NoSavedSettings |
		ImGuiTableFlags_BordersInnerV
	)) return false;

	ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
	return true;
}

inline void BeginPropertyRow(const char* label) {
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::TableSetColumnIndex(1);
	ImGui::SetNextItemWidth(-FLT_MIN);
}

inline void DrawRenderingSettings(
	ConfigService& config,
	GraphicsContext* graphics
) {
	ImGui::SeparatorText("Rendering");
	if(!BeginSettingsTable("ProjectSettingsRendering")) return;

	BeginPropertyRow("Rendering API");
	const char* preview = GetBackendLabel(config.engineConfig.graphics.backend);
	if(ImGui::BeginCombo("##RenderingAPI", preview)) {
		for(const BackendOption& option : kBackendOptions) {
			const bool selected = option.type == config.engineConfig.graphics.backend;
			if(!option.selectable) ImGui::BeginDisabled();
			if(ImGui::Selectable(option.label, selected) && option.selectable) {
				config.engineConfig.graphics.backend = option.type;
			}
			if(!option.selectable) {
				ImGui::EndDisabled();
				if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && option.note) {
					ImGui::SetTooltip("%s", option.note);
				}
			}
			if(selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	BeginPropertyRow("Maximum Frame Latency");
	const uint32_t configuredLatency = (std::max)(
		1u,
		(std::min)(3u, config.engineConfig.graphics.maximumFrameLatency)
	);
	int selectedIndex = static_cast<int>(configuredLatency) - 1;
	static constexpr const char* kFrameLatencyItems =
		"1 - Low Latency\0"
		"2 - Balanced\0"
		"3 - Maximum Throughput\0";
	if(ImGui::Combo("##MaximumFrameLatency", &selectedIndex, kFrameLatencyItems)) {
		const uint32_t requestedLatency = static_cast<uint32_t>(selectedIndex + 1);
		const uint32_t previousLatency = graphics
			? graphics->GetMaximumFrameLatency()
			: configuredLatency;

		const bool applied = !graphics || graphics->SetMaximumFrameLatency(requestedLatency);
		if(applied) {
			config.engineConfig.graphics.maximumFrameLatency = requestedLatency;
			config.appConfig.MaximumFrameLatency = static_cast<int>(requestedLatency);
		} else {
			config.engineConfig.graphics.maximumFrameLatency = previousLatency;
			config.appConfig.MaximumFrameLatency = static_cast<int>(previousLatency);
		}
	}
	if(ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"1: Lowest input latency; may reduce maximum FPS.\n"
			"2: Recommended balance between latency and throughput.\n"
			"3: More CPU/GPU overlap; may increase latency."
		);
	}

	if(graphics) {
		const uint32_t appliedLatency = graphics->GetMaximumFrameLatency();
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextDisabled("Applied Value");
		ImGui::TableSetColumnIndex(1);
		ImGui::TextDisabled(
			"%u (%s)",
			appliedLatency,
			GetFrameLatencyLabel(appliedLatency)
		);
	}

	ImGui::EndTable();
	ImGui::TextDisabled("Rendering API changes require restart. Frame latency changes are applied immediately.");
}

inline void DrawApplicationSettings(
	ConfigService& config,
	GraphicsContext* graphics
) {
	(void)graphics;
	APPCONFIG& app = config.appConfig;

	ImGui::SeparatorText("Application");
	if(BeginSettingsTable("ProjectSettingsApplication")) {
		BeginPropertyRow("Application Type");
		int appType = static_cast<int>(app.AppType);
		if(ImGui::Combo("##ApplicationType", &appType, "Editor\0Player\0Debug Player\0Debug Editor\0")) {
			app.AppType = static_cast<APPTYPE>(appType);
		}
		BeginPropertyRow("Start Scene");
		ImGui::UndoInputText("##StartScene", &app.startSceneFilePath, 512);
		BeginPropertyRow("Template Directory");
		ImGui::UndoInputText("##TemplateDirectory", &app.templateDir, 512);
		ImGui::EndTable();
	}

	ImGui::SeparatorText("Display");
	if(BeginSettingsTable("ProjectSettingsDisplay")) {
		BeginPropertyRow("VSync");
		ImGui::UndoCheckbox("##VSync", &app.Vsync);
		BeginPropertyRow("Full Screen");
		ImGui::UndoCheckbox("##FullScreen", &app.FullScreen);
		BeginPropertyRow("Width");
		ImGui::UndoDragInt("##WindowWidth", &app.Width, 1.0f, 320, 7680);
		BeginPropertyRow("Height");
		ImGui::UndoDragInt("##WindowHeight", &app.Height, 1.0f, 180, 4320);
		ImGui::EndTable();
	}

	ImGui::SeparatorText("Audio");
	if(BeginSettingsTable("ProjectSettingsAudio")) {
		BeginPropertyRow("Master Volume");
		ImGui::UndoSliderFloat("##MasterVolume", &app.MasterVolume, 0.0f, 1.0f);
		BeginPropertyRow("BGM Volume");
		ImGui::UndoSliderFloat("##BGMVolume", &app.BGMVolume, 0.0f, 1.0f);
		BeginPropertyRow("SE Volume");
		ImGui::UndoSliderFloat("##SEVolume", &app.SEVolume, 0.0f, 1.0f);
		ImGui::EndTable();
	}
}

inline void DrawSystemSettings(SceneManager* sceneManager) {
	SystemRegistry* registry =
		sceneManager ? sceneManager->GetSystemRegistry() : nullptr;
	if(!registry) {
		ImGui::TextDisabled("SystemRegistry is not available.");
		return;
	}

	bool hasSettings = false;
	for(auto& system : registry->GetSystems()) {
		if(!system || !system->HasSystemSetting()) continue;
		const char* name = system->GetSystemName();
		if(!name) continue;
		hasSettings = true;
		if(ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::PushID(system.get());
			system->SystemSetting();
			ImGui::PopID();
		}
	}
	if(!hasSettings) ImGui::TextDisabled("No systems expose project settings.");
}

inline void DrawScheduleSettings(
	SceneManager* sceneManager,
	ScheduleProfilerViewState& viewState
) {
	SystemRegistry* registry =
		sceneManager ? sceneManager->GetSystemRegistry() : nullptr;
	if(!registry) {
		ImGui::TextDisabled("SystemRegistry is not available.");
		return;
	}

	ScheduleProfileYamlExportUI::Draw(*registry, viewState);
	ScheduleProfilerView::Draw(*registry, viewState);
}

} // namespace SystemSettingImplementationDetail

void SystemSetting::Draw(const EditorDrawContext ctx) {
	(void)ctx;
	using namespace SystemSettingImplementationDetail;

	MenuBar* menuBar = m_editor ? m_editor->GetUI<MenuBar>() : nullptr;
	bool* showProjectSettings = menuBar ? &menuBar->showProjectSettings : nullptr;
	if(!showProjectSettings || !*showProjectSettings) return;

	ImGuiWindowClass windowClass;
	windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&windowClass);
	ImGui::SetNextWindowSize(ImVec2(900.0f, 760.0f), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Project Settings", showProjectSettings)) {
		ImGui::End();
		return;
	}

	SceneManager* sceneManager = m_editor ? m_editor->sceneManager : nullptr;
	SceneManagerContext* sceneContext = sceneManager ? sceneManager->GetContext() : nullptr;
	ConfigService* config = sceneContext ? sceneContext->config : nullptr;
	if(!config) {
		ImGui::TextDisabled("ConfigService is not available.");
		ImGui::End();
		return;
	}

	if(ImGui::BeginTabBar("ProjectSettingsTabs")) {
		if(ImGui::BeginTabItem("Application")) {
			DrawRenderingSettings(*config, sceneContext ? sceneContext->graphics : nullptr);
			DrawApplicationSettings(*config, sceneContext ? sceneContext->graphics : nullptr);
			ImGui::EndTabItem();
		}
		if(ImGui::BeginTabItem("Lighting")) {
			LightingDiagnosticUI::Draw(sceneManager);
			ImGui::EndTabItem();
		}
		if(ImGui::BeginTabItem("Systems")) {
			DrawSystemSettings(sceneManager);
			ImGui::EndTabItem();
		}
		if(ImGui::BeginTabItem("Schedule")) {
			DrawScheduleSettings(sceneManager, m_scheduleViewState);
			ImGui::EndTabItem();
		}
		if(ImGui::BeginTabItem("Static Batch")) {
			StaticBatchTelemetryUI::Draw(sceneManager);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::Spacing();
	ImGui::Separator();
	if(ImGui::Button("Save Project Settings", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
		if(SystemRegistry* registry =
			sceneManager ? sceneManager->GetSystemRegistry() : nullptr) {
			registry->EncodeAll(config->editorConfig);
		}
		config->SaveEditorConfig(EDITOR_CONFIG_PATH);
		config->SaveApplicationConfig(APPLICATION_CONFIG_PATH);
		m_lastSaveTime = ImGui::GetTime();
	}
	if(ImGui::GetTime() - m_lastSaveTime < 2.0) {
		ImGui::TextDisabled("Project settings saved.");
	}

	ImGui::End();
}
