#include "Backends/ImGui/imgui.h"
#include "DebugTools/ImGuiSystem.h"
#include "MenuBar.h"

void MenuBar::Register(MenuEvent event, const Callback& callback){
	m_eventCallbacks[event] = callback;
}

void MenuBar::Draw(EditorDrawContext ctx){

	if(ImGui::IsKeyPressed(ImGuiKey_F3, false)){
		showMenuBar = !showMenuBar;

		showSceneHierarchy = showMenuBar;
		showInspector = showMenuBar;
		showConsole = showMenuBar;
		showAssetsBrowser = showMenuBar;
		showEditorView = showMenuBar;
		showPlayerView = showMenuBar;
		showParformanceMonitor = showMenuBar;
		showSystemSetting = showMenuBar;
	}

	if(showMenuBar && ImGui::BeginMainMenuBar()){
		RenderFileMenu();
		RenderEditMenu();

		if(ImGui::BeginMenu("Window")){
			if(ImGui::MenuItem("Scene Hierarchy", nullptr, showSceneHierarchy)){
				showSceneHierarchy = !showSceneHierarchy;
			}

			if(ImGui::MenuItem("Inspector", nullptr, showInspector)){
				showInspector = !showInspector;
			}
			if(ImGui::MenuItem("DebugLog", nullptr, showConsole)){
				showConsole = !showConsole;
			}
			if(ImGui::MenuItem("Assets Browser", nullptr, showAssetsBrowser)){
				showAssetsBrowser = !showAssetsBrowser;
			}
			if(ImGui::MenuItem("Editor View", nullptr, showEditorView)){
				showEditorView = !showEditorView;
			}
			if(ImGui::MenuItem("Player View", nullptr, showPlayerView)){
				showPlayerView = !showPlayerView;
			}
			if(ImGui::MenuItem("ParformanceMonitor", nullptr, showParformanceMonitor)){
				showParformanceMonitor = !showParformanceMonitor;
			}
			if (ImGui::MenuItem("SystemSetting", nullptr, showSystemSetting)) {
				showSystemSetting = !showSystemSetting;
			}

			ImGui::EndMenu();
		}


		ImGui::EndMainMenuBar();
	}

}

void MenuBar::Invoke(MenuEvent event){
	auto it = m_eventCallbacks.find(event);
	if(it != m_eventCallbacks.end()) it->second();
}

void MenuBar::RenderFileMenu(){
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

void MenuBar::RenderEditMenu(){

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
