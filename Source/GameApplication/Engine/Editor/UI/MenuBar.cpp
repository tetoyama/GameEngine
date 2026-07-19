// =======================================================================
//
// MenuBar.cpp
//
// =======================================================================
#include "Backends/ImGui/imgui.h"
#include "DebugTools/ImGuiSystem.h"
#include "MenuBar.h"
#include "ModernImGui/EditorIconWidgets.h"

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
			showSceneSettings = false;
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
			if(ImGui::MenuItem("Scene Settings", nullptr, showSceneSettings)){
				showSceneSettings = !showSceneSettings;
			}
			if(ImGui::MenuItem("Project Settings", nullptr, showProjectSettings)){
				showProjectSettings = !showProjectSettings;
			}

			ImGui::EndMenu();
		}

		if(m_editor){
			constexpr float shortcutSpacing = 4.0f;
			const float groupWidth =
				MImGui::PanelShortcutWidth("Hierarchy") +
				MImGui::PanelShortcutWidth("Assets") +
				MImGui::PanelShortcutWidth("Inspector") +
				MImGui::PanelShortcutWidth("Log") +
				MImGui::PanelShortcutWidth("Profiler") +
				shortcutSpacing * 4.0f;
			const float targetX =
				ImGui::GetWindowContentRegionMax().x - groupWidth - 6.0f;

			// Do not degrade to ambiguous icon-only controls on narrow windows.
			// The Window menu remains the reliable fallback.
			if(targetX > ImGui::GetCursorPosX() + 16.0f){
				ImGui::SameLine();
				ImGui::SetCursorPosX(targetX);
				ImGui::PushID("EditorPanelShortcuts");

				auto drawPanelShortcut = [this](
					const char* id,
					const char* label,
					EditorIcon icon,
					bool& visible,
					const char* tooltip
				){
					if(MImGui::PanelShortcutButton(
						id,
						label,
						m_editor->icons.Get(icon),
						visible,
						tooltip
					)){
						visible = !visible;
					}
				};

				drawPanelShortcut(
					"Hierarchy",
					"Hierarchy",
					EditorIcon::Hierarchy,
					showSceneHierarchy,
					"Show or hide Hierarchy"
				);
				ImGui::SameLine(0.0f, shortcutSpacing);
				drawPanelShortcut(
					"Assets",
					"Assets",
					EditorIcon::Assets,
					showAssetsBrowser,
					"Show or hide Assets Browser"
				);
				ImGui::SameLine(0.0f, shortcutSpacing);
				drawPanelShortcut(
					"Inspector",
					"Inspector",
					EditorIcon::Inspector,
					showInspector,
					"Show or hide Inspector"
				);
				ImGui::SameLine(0.0f, shortcutSpacing);
				drawPanelShortcut(
					"Console",
					"Log",
					EditorIcon::Console,
					showConsole,
					"Show or hide Debug Log"
				);
				ImGui::SameLine(0.0f, shortcutSpacing);
				drawPanelShortcut(
					"Performance",
					"Profiler",
					EditorIcon::Performance,
					showPerformanceMonitor,
					"Show or hide Performance Monitor"
				);

				ImGui::PopID();
			}
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
		if(ImGui::MenuItem("Scene Settings...")){
			showSceneSettings = true;
		}
		if(ImGui::MenuItem("Project Settings...")){
			showProjectSettings = true;
		}

		ImGui::EndMenu();
	}
}
