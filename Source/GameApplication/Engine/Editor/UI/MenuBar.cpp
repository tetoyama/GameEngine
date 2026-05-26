// =======================================================================
// 
// MenuBar.cpp
// 
// =======================================================================
#include "Backends/ImGui/imgui.h"
#include "DebugTools/ImGuiSystem.h"
#include "MenuBar.h"

void MenuBar::Register(MenuEvent event, const Callback& callback){
	m_eventCallbacks[event] = callback;
}

void MenuBar::Draw(const EditorDrawContext ctx){

	if(ImGui::IsKeyPressed(ImGuiKey_F3, false)){
		m_showMenuBar = !m_showMenuBar;

		showSceneHierarchy = m_showMenuBar;
		showInspector = m_showMenuBar;
		showConsole = m_showMenuBar;
		showAssetsBrowser = m_showMenuBar;
		showEditorView = m_showMenuBar;
		showPlayerView = m_showMenuBar;
		showPerformanceMonitor = m_showMenuBar;
		showSystemSetting = m_showMenuBar;
	}

	// Ctrl+Z でアンドゥ
	if (ImGui::IsKeyPressed(ImGuiKey_Z, false) && ImGui::GetIO().KeyCtrl) {
		Invoke(MenuEvent::Edit_Undo);
	}
	// Ctrl+Y でリドゥ
	if (ImGui::IsKeyPressed(ImGuiKey_Y, false) && ImGui::GetIO().KeyCtrl) {
		Invoke(MenuEvent::Edit_Redo);
	}

	if(m_showMenuBar && ImGui::BeginMainMenuBar()){
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
			if(ImGui::MenuItem("PerformanceMonitor", nullptr, showPerformanceMonitor)){
				showPerformanceMonitor = !showPerformanceMonitor;
			}
			if(ImGui::MenuItem("SystemSetting", nullptr, showSystemSetting)){
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
		// Undo/Redo の有効フラグと操作名を取得
		bool canUndo = m_editor && m_editor->commandManager.CanUndo();
		bool canRedo = m_editor && m_editor->commandManager.CanRedo();

		std::string undoDesc = canUndo ? m_editor->commandManager.PeekUndoDescription() : "";
		std::string undoLabel = undoDesc.empty() ? "Undo" : "Undo: " + undoDesc;
		if(ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, canUndo)){
			Invoke(MenuEvent::Edit_Undo);
		}

		std::string redoDesc = canRedo ? m_editor->commandManager.PeekRedoDescription() : "";
		std::string redoLabel = redoDesc.empty() ? "Redo" : "Redo: " + redoDesc;
		if(ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, canRedo)){
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
