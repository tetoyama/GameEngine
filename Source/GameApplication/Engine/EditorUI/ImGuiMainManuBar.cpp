#include "Backends/ImGui/imgui.h"
#include "Engine/DebugTools/ImGuiSystem.h"
#include "ImGuiMainManubar.h"

void ImGuiManubar::Register(MenuEvent event, const Callback& callback){
	m_eventCallbacks[event] = callback;
}

void ImGuiManubar::Render(){

	if(ImGui::BeginMainMenuBar()){
		RenderFileMenu();
		RenderEditMenu();

		if(ImGui::BeginMenu("Window")){
			if(ImGui::MenuItem("Scene Hierarchy", nullptr, showSceneHierarchy)){
				showSceneHierarchy = !showSceneHierarchy;
			}

			if(ImGui::MenuItem("Inspector", nullptr, showInspector)){
				showInspector = !showInspector;
			}
			if(ImGui::MenuItem("Console", nullptr, showConsole)){
				showConsole = !showConsole;
			}
			if(ImGui::MenuItem("Assets Browser", nullptr, showAssetsBrowser)){
				showAssetsBrowser = !showAssetsBrowser;
			}
			ImGui::EndMenu();
		}


		ImGui::EndMainMenuBar();
	}

}

void ImGuiManubar::Invoke(MenuEvent event){
	auto it = m_eventCallbacks.find(event);
	if(it != m_eventCallbacks.end()) it->second();
}

void ImGuiManubar::RenderFileMenu(){
	if(ImGui::BeginMenu("File")){
		if(ImGui::MenuItem("New Scene", "Ctrl+N")){
			Invoke(MenuEvent::File_New);
		}
		if(ImGui::MenuItem("Open...", "Ctrl+O")){
			Invoke(MenuEvent::File_Open);
		}
		if(ImGui::MenuItem("Save", "Ctrl+S")){
			Invoke(MenuEvent::File_Save);
		}

		ImGui::Separator();

		if(ImGui::MenuItem("Exit", "Ctrl+Q")){
			Invoke(MenuEvent::File_Exit);
		}

		ImGui::EndMenu();
	}
}

void ImGuiManubar::RenderEditMenu(){

	if(ImGui::BeginMenu("Edit")){
		if(ImGui::MenuItem("Undo", "Ctrl+Z")){
			Invoke(MenuEvent::Edit_Undo);
		}

		if(ImGui::MenuItem("Redo", "Ctrl+Y")){
			Invoke(MenuEvent::Edit_Redo);
		}
		ImGui::Separator();

		if(ImGui::MenuItem("Cut", "Ctrl+X")){
		}
		if(ImGui::MenuItem("Copy", "Ctrl+C")){
		}
		if(ImGui::MenuItem("Paste", "Ctrl+V")){
		}
		ImGui::EndMenu();
	}
}
