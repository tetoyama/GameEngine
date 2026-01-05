#include "SystemSetting.h"
#include "Backends/ImGui/imgui.h"
#include "DebugTools/ImGuiSystem.h"
#include "Editor/UI/MenuBar.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/systemRegistry.h"

void SystemSetting::Draw(const EditorDrawContext ctx) {
	bool* showSystemSetting = &m_editor->GetUI<MenuBar>()->showSystemSetting;
	if(!showSystemSetting || !*showSystemSetting){
		return;
	}

	if(ImGui::Begin("SystemSetting", showSystemSetting)){
		auto& systems = m_editor->sceneManager->systemRegistry->GetSystems();
		for(auto& s : systems){
			const char* name = typeid(*s).name();

			if(ImGui::TreeNodeEx(name,ImGuiTreeNodeFlags_DefaultOpen)){
				s->SystemSetting();
				ImGui::TreePop();
			}
		}
	}
	ImGui::End();
}
