#include "ImGuiMainManubar.h"
#include "Backends/ImGui/imgui.h"

void ImGuiManubar::Register(MenuEvent event, const Callback& callback){
	m_eventCallbacks[event] = callback;
}

void ImGuiManubar::Render(){
	//if(ImGui::BeginMainMenuBar()){
	//	RenderFileMenu();
	//	RenderEditMenu();
	//	ImGui::EndMainMenuBar();
	//}
	if(ImGui::BeginMainMenuBar()){

		// ロゴ・タイトル
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.56f, 1.00f, 1.00f));
		ImGui::Text("GameEngine");
		ImGui::PopStyleColor();

		ImGui::Separator();

		if(ImGui::BeginMenu("File")){
			if(ImGui::MenuItem("New Scene", "Ctrl+N")){
				// 新しいシーンの作成
			}
			if(ImGui::MenuItem("Open Scene", "Ctrl+O")){
				// シーンを開く
			}
			if(ImGui::MenuItem("Save Scene", "Ctrl+S")){
				// シーンを保存
			}
			ImGui::Separator();
			if(ImGui::MenuItem("Exit")){

			}
			ImGui::EndMenu();
		}

		if(ImGui::BeginMenu("Edit")){
			if(ImGui::MenuItem("Undo", "Ctrl+Z")){
			}
			if(ImGui::MenuItem("Redo", "Ctrl+Y")){
			}
			ImGui::Separator();
			if(ImGui::MenuItem("Preferences")){
			}
			ImGui::EndMenu();
		}

		if(ImGui::BeginMenu("Window")){
			ImGui::MenuItem("Scene Hierarchy", nullptr);
			ImGui::MenuItem("Inspector", nullptr);
			ImGui::MenuItem("Console", nullptr);
			ImGui::MenuItem("Assets Browser", nullptr);
			ImGui::EndMenu();
		}

		if(ImGui::BeginMenu("Tools")){
			if(ImGui::MenuItem("Performance Profiler")){
			}
			if(ImGui::MenuItem("Material Editor")){
			}
			if(ImGui::MenuItem("Particle System")){
			}
			ImGui::EndMenu();
		}

		// 右端にステータス表示
		float menuBarWidth = ImGui::GetWindowWidth();
		float statusWidth = 200.0f;
		ImGui::SetCursorPosX(menuBarWidth - statusWidth);

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 0.7f, 1.0f));
		ImGui::Text("FPS: 60 | Objects: 25");
		ImGui::PopStyleColor();

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
