#include "SystemSetting.h"
#include "Backends/ImGui/imgui.h"
#include "DebugTools/ImGuiSystem.h"
#include "Editor/UI/MenuBar.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/systemRegistry.h"
#include "Service/Config/configSystem.h"
#include "BackEnds/ImGuiFunc.h"

void SystemSetting::Draw(const EditorDrawContext ctx) {
	bool* showSystemSetting = &m_editor->GetUI<MenuBar>()->showSystemSetting;
	if(!showSystemSetting || !*showSystemSetting){
		return;
	}

	ImGuiWindowClass window_class;
	window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&window_class);

	if(!ImGui::Begin("SystemSetting", showSystemSetting)){
		ImGui::End();
		return;
	}

	// ===========================================================
	// Application Config セクション
	// ===========================================================
	if(ImGui::CollapsingHeader("Application Config", ImGuiTreeNodeFlags_DefaultOpen)){
		ConfigService* cfg = m_editor->sceneManager->GetContext()->config;
		if(cfg){
			APPCONFIG& app = cfg->appConfig;

			ImGui::SeparatorText("Video");
			ImGui::UndoCheckbox("Vsync",       &app.Vsync);
			ImGui::UndoCheckbox("Full Screen",  &app.FullScreen);
			ImGui::UndoDragInt("Width",         &app.Width,  1.0f, 320, 7680);
			ImGui::UndoDragInt("Height",        &app.Height, 1.0f, 180, 4320);

			ImGui::SeparatorText("Audio");
			ImGui::UndoSliderFloat("Master Volume", &app.MasterVolume, 0.0f, 1.0f);
			ImGui::UndoSliderFloat("BGM Volume",    &app.BGMVolume,    0.0f, 1.0f);
			ImGui::UndoSliderFloat("SE Volume",     &app.SEVolume,     0.0f, 1.0f);

			ImGui::SeparatorText("Scene");
			ImGui::UndoInputText("Start Scene",  &app.startSceneFilePath, 512);
			ImGui::UndoInputText("Template Dir", &app.templateDir,        512);

			ImGui::Spacing();
			if(ImGui::Button("Save Config")){
				cfg->SaveApplicationConfig(APPLICATION_CONFIG_PATH);
			}
		}
	}

	ImGui::Separator();

	// ===========================================================
	// 各システム設定セクション（HasSystemSetting() == true のみ表示）
	// ===========================================================
	auto& systems = m_editor->sceneManager->systemRegistry->GetSystems();
	for(auto& s : systems){
		if(!s->HasSystemSetting()) continue;

		const char* name = s->GetSystemName();
		if(!name) continue;

		if(ImGui::CollapsingHeader(name)){
			ImGui::PushID(name);
			s->SystemSetting();
			ImGui::PopID();
		}
	}

	ImGui::End();
}
