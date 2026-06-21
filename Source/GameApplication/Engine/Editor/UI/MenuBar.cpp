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
	(void)ctx;

	if(ImGui::IsKeyPressed(ImGuiKey_F3, false)){
		showMenuBar = !showMenuBar;

		showSceneHierarchy = showMenuBar;
		showInspector = showMenuBar;
		showConsole = showMenuBar;
		showAssetsBrowser = showMenuBar;
		showEditorView = showMenuBar;
		showPlayerView = showMenuBar;
		showPerformanceMonitor = showMenuBar;
		if(!showMenuBar){
			showProjectSettings = false;
		}
	}

	if(ImGui::IsKeyPressed(ImGuiKey_Z, false) && ImGui::GetIO().KeyCtrl){
		Invoke(MenuEvent::Edit_Undo);
	}
	if(ImGui::IsKeyPressed(ImGuiKey_Y, false) && ImGui::GetIO().KeyCtrl){
		Invoke(MenuEvent::Edit_Redo);
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
			if(ImGui::MenuItem("Debug Log", nullptr, showConsole)){
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
			if(ImGui::MenuItem("Performance Monitor", nullptr, showPerformanceMonitor)){
				showPerformanceMonitor = !showPerformanceMonitor;
			}
			ImGui::Separator();
			if(ImGui::MenuItem("Project Settings", nullptr, showProjectSettings)){
				showProjectSettings = !showProjectSettings;
			}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
}

void MenuBar::Invoke(MenuEvent event){
	auto it = m_eventCallbacks.find(event);
	if(it != m_eventCallbacks.end()){
		it->second();
	}
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
		const bool canUndo = m_editor && m_editor->commandManager.CanUndo();
		const bool canRedo = m_editor && m_editor->commandManager.CanRedo();

		const std::string undoDesc = canUndo
			? m_editor->commandManager.PeekUndoDescription()
			: "";
		const std::string undoLabel = undoDesc.empty()
			? "Undo"
			: "Undo: " + undoDesc;
		if(ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, canUndo)){
			Invoke(MenuEvent::Edit_Undo);
		}

		const std::string redoDesc = canRedo
			? m_editor->commandManager.PeekRedoDescription()
			: "";
		const std::string redoLabel = redoDesc.empty()
			? "Redo"
			: "Redo: " + redoDesc;
		if(ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, canRedo)){
			Invoke(MenuEvent::Edit_Redo);
		}

		ImGui::Separator();
		if(ImGui::MenuItem("Cut", "Ctrl+X")){}
		if(ImGui::MenuItem("Copy", "Ctrl+C")){}
		if(ImGui::MenuItem("Paste", "Ctrl+V")){}

		ImGui::Separator();
		if(ImGui::MenuItem("Project Settings...")){
			showProjectSettings = true;
		}

		ImGui::EndMenu();
	}
}
