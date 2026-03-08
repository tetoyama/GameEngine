// =======================================================================
// 
// MenuBar.cpp
// 
// =======================================================================
#include "Backends/ImGui/imgui.h"
#include "DebugTools/ImGuiSystem.h"
#include "MenuBar.h"
#include "UIHelpers.h"

void MenuBar::Register(MenuEvent event, const Callback& callback){
	m_eventCallbacks[event] = callback;
}

void MenuBar::Draw(const EditorDrawContext ctx){

	if(ImGui::IsKeyPressed(ImGuiKey_F3, false)){
		showMenuBar = !showMenuBar;
		SetMainWindowVisibility(showMenuBar);
	}

	// Ctrl+Z でアンドゥ
	if (ImGui::IsKeyPressed(ImGuiKey_Z, false) && ImGui::GetIO().KeyCtrl) {
		Invoke(MenuEvent::Edit_Undo);
	}
	// Ctrl+Y でリドゥ
	if (ImGui::IsKeyPressed(ImGuiKey_Y, false) && ImGui::GetIO().KeyCtrl) {
		Invoke(MenuEvent::Edit_Redo);
	}

	if(showMenuBar && ImGui::BeginMainMenuBar()){
		RenderFileMenu();
		RenderEditMenu();
		RenderWindowMenu();


		ImGui::EndMainMenuBar();
	}

}

void MenuBar::SetMainWindowVisibility(bool isVisible){
	showSceneHierarchy = isVisible;
	showInspector = isVisible;
	showConsole = isVisible;
	showAssetsBrowser = isVisible;
	showEditorView = isVisible;
	showPlayerView = isVisible;
	showPerformanceMonitor = isVisible;
	showSystemSetting = isVisible;
}

void MenuBar::RenderWindowMenu(){
	if(!ImGui::BeginMenu("Window")){
		return;
	}

	EditorUIHelpers::ToggleWindowMenuItem("Scene Hierarchy", showSceneHierarchy);
	EditorUIHelpers::ToggleWindowMenuItem("Inspector", showInspector);
	EditorUIHelpers::ToggleWindowMenuItem("DebugLog", showConsole);
	EditorUIHelpers::ToggleWindowMenuItem("Assets Browser", showAssetsBrowser);
	EditorUIHelpers::ToggleWindowMenuItem("Editor View", showEditorView);
	EditorUIHelpers::ToggleWindowMenuItem("Player View", showPlayerView);
	EditorUIHelpers::ToggleWindowMenuItem("PerformanceMonitor", showPerformanceMonitor);
	EditorUIHelpers::ToggleWindowMenuItem("SystemSetting", showSystemSetting);

	ImGui::EndMenu();
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
