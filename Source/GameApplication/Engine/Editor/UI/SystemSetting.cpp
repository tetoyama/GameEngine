// =======================================================================
//
// SystemSetting.cpp
//
// Project Settings UI。ファイル名は既存Visual Studioプロジェクトとの互換維持のため残す。
//
// =======================================================================
#include "SystemSetting.h"

#include <array>
#include <cfloat>
#include <string>

#include "Backends/ImGui/imgui.h"
#include "Backends/ImGuiFunc.h"
#include "DebugTools/ImGuiSystem.h"
#include "Editor/UI/MenuBar.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/systemRegistry.h"
#include "Service/Config/configSystem.h"
#include <ImGui/imgui_internal.h>

namespace {

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

const char* GetBackendLabel(RHI::BackendType backend){
	for(const BackendOption& option : kBackendOptions){
		if(option.type == backend){
			return option.label;
		}
	}
	return "Unknown";
}

bool BeginSettingsTable(const char* id){
	if(!ImGui::BeginTable(
		id,
		2,
		ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_NoSavedSettings |
		ImGuiTableFlags_BordersInnerV
	)){
		return false;
	}

	ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
	return true;
}

void BeginPropertyRow(const char* label){
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::TableSetColumnIndex(1);
	ImGui::SetNextItemWidth(-FLT_MIN);
}

void DrawRenderingSettings(ConfigService& config){
	ImGui::SeparatorText("Rendering");

	if(BeginSettingsTable("ProjectSettingsRendering")){
		BeginPropertyRow("Rendering API");

		const char* preview = GetBackendLabel(config.engineConfig.graphics.backend);
		if(ImGui::BeginCombo("##RenderingAPI", preview)){
			for(const BackendOption& option : kBackendOptions){
				const bool selected = option.type == config.engineConfig.graphics.backend;
				if(!option.selectable){
					ImGui::BeginDisabled();
				}

				if(ImGui::Selectable(option.label, selected) && option.selectable){
					config.engineConfig.graphics.backend = option.type;
				}

				if(!option.selectable){
					ImGui::EndDisabled();
					if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && option.note){
						ImGui::SetTooltip("%s", option.note);
					}
				}

				if(selected){
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::EndTable();
	}

	ImGui::TextDisabled(
		"Rendering API changes are applied after restarting the application."
	);
}

void DrawApplicationSettings(ConfigService& config){
	APPCONFIG& app = config.appConfig;

	ImGui::SeparatorText("Application");
	if(BeginSettingsTable("ProjectSettingsApplication")){
		BeginPropertyRow("Application Type");
		int appType = static_cast<int>(app.AppType);
		if(ImGui::Combo(
			"##ApplicationType",
			&appType,
			"Editor\0Player\0Debug Player\0Debug Editor\0"
		)){
			app.AppType = static_cast<APPTYPE>(appType);
		}

		BeginPropertyRow("Start Scene");
		ImGui::UndoInputText("##StartScene", &app.startSceneFilePath, 512);

		BeginPropertyRow("Template Directory");
		ImGui::UndoInputText("##TemplateDirectory", &app.templateDir, 512);

		ImGui::EndTable();
	}

	ImGui::SeparatorText("Display");
	if(BeginSettingsTable("ProjectSettingsDisplay")){
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
	if(BeginSettingsTable("ProjectSettingsAudio")){
		BeginPropertyRow("Master Volume");
		ImGui::UndoSliderFloat("##MasterVolume", &app.MasterVolume, 0.0f, 1.0f);

		BeginPropertyRow("BGM Volume");
		ImGui::UndoSliderFloat("##BGMVolume", &app.BGMVolume, 0.0f, 1.0f);

		BeginPropertyRow("SE Volume");
		ImGui::UndoSliderFloat("##SEVolume", &app.SEVolume, 0.0f, 1.0f);

		ImGui::EndTable();
	}
}

} // namespace

void SystemSetting::Draw(const EditorDrawContext ctx){
	(void)ctx;

	MenuBar* menuBar = m_editor ? m_editor->GetUI<MenuBar>() : nullptr;
	bool* showProjectSettings = menuBar ? &menuBar->showProjectSettings : nullptr;
	if(!showProjectSettings || !*showProjectSettings){
		return;
	}

	ImGuiWindowClass windowClass;
	windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&windowClass);
	ImGui::SetNextWindowSize(ImVec2(620.0f, 720.0f), ImGuiCond_FirstUseEver);

	if(!ImGui::Begin("Project Settings", showProjectSettings)){
		ImGui::End();
		return;
	}

	SceneManager* sceneManager = m_editor ? m_editor->sceneManager : nullptr;
	SceneManagerContext* sceneContext = sceneManager ? sceneManager->GetContext() : nullptr;
	ConfigService* config = sceneContext ? sceneContext->config : nullptr;

	if(!config){
		ImGui::TextDisabled("ConfigService is not available.");
		ImGui::End();
		return;
	}

	if(ImGui::BeginTabBar("ProjectSettingsTabs")){
		if(ImGui::BeginTabItem("Application")){
			DrawRenderingSettings(*config);
			DrawApplicationSettings(*config);
			ImGui::EndTabItem();
		}

		if(ImGui::BeginTabItem("Systems")){
			if(!sceneManager || !sceneManager->systemRegistry){
				ImGui::TextDisabled("SystemRegistry is not available.");
			} else {
				auto& systems = sceneManager->systemRegistry->GetSystems();
				bool hasSettings = false;
				for(auto& system : systems){
					if(!system || !system->HasSystemSetting()){
						continue;
					}

					const char* name = system->GetSystemName();
					if(!name){
						continue;
					}

					hasSettings = true;
					if(ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_DefaultOpen)){
						ImGui::PushID(system.get());
						system->SystemSetting();
						ImGui::PopID();
					}
				}

				if(!hasSettings){
					ImGui::TextDisabled("No systems expose project settings.");
				}
			}
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::Spacing();
	ImGui::Separator();
	const float saveButtonWidth = ImGui::GetContentRegionAvail().x;
	if(ImGui::Button(
		"Save Project Settings",
		ImVec2(saveButtonWidth, 0.0f)
	)){
		if(sceneManager && sceneManager->systemRegistry){
			sceneManager->systemRegistry->EncodeAll(config->editorConfig);
		}
		config->SaveEditorConfig(EDITOR_CONFIG_PATH);
		config->SaveApplicationConfig(APPLICATION_CONFIG_PATH);
		RHI::SetRequestedBackend(config->engineConfig.graphics.backend);
		m_lastSaveTime = ImGui::GetTime();
	}

	if(ImGui::GetTime() - m_lastSaveTime < 2.0){
		ImGui::TextDisabled("Project settings saved.");
	}

	ImGui::End();
}
