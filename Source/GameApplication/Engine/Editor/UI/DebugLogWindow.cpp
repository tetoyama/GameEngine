// =======================================================================
// 
// DebugLogWindow.cpp
// 
// =======================================================================
#include "DebugLogWindow.h"
#include "Hierarchy.h"
#include <ImGui/imgui_internal.h>
#include <sceneManager.h>
#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"

DebugLogWindow::DebugLogWindow(){

	for(int i = (int)LogLevel::Trace; i <= (int)LogLevel::Critical; ++i){
		levelFilter.insert(static_cast<LogLevel>(i));
	}
}

bool DebugLogWindow::PassesFilter(const LogEntry& entry) const{
	if(levelFilter.find(entry.level) == levelFilter.end()) return false;

	if(strlen(searchBuffer) > 0){
		if(entry.message.find(searchBuffer) == std::string::npos &&
		   entry.function.find(searchBuffer) == std::string::npos)
			return false;
	}
	return true;
}

const char* DebugLogWindow::LevelToString(LogLevel level) const{
	switch(level){
		case LogLevel::Trace: return "Trace";
		case LogLevel::Debug: return "Debug";
		case LogLevel::Info: return "Info";
		case LogLevel::Warning: return "Warning";
		case LogLevel::Error: return "Error";
		case LogLevel::Critical: return "Critical";
		default: return "Unknown";
	}
}

std::string DebugLogWindow::LevelFilterString(LogLevel level) const{

	int Count = 0;

	const auto& entries = logSink->GetEntries();
	for(const auto& entry : entries){
		if(entry.level != level) continue;
		Count++;
	}

	std::string a = LevelToString(level);
	a = a + "(" + std::to_string(Count) + ")";
	return a;
}

ImVec4 DebugLogWindow::GetColorForLevel(LogLevel level) const{
	switch(level){
		case LogLevel::Trace:    return ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
		case LogLevel::Debug:    return ImVec4(0.8f, 0.8f, 1.0f, 1.0f);
		case LogLevel::Info:     return ImVec4(0.6f, 1.0f, 0.6f, 1.0f);
		case LogLevel::Warning:  return ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
		case LogLevel::Error:    return ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
		case LogLevel::Critical: return ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
		default: return ImVec4(1, 1, 1, 1);
	}
}

void DebugLogWindow::Initialize(EditorService* editor){
	editor = editor;
	logSink = editor->debugLogSystem->GetSink<MemoryLogSink>();
}

void DebugLogWindow::Draw(const EditorDrawContext ctx){

	bool* showDebugLogWindow = &m_editor->GetUI<MenuBar>()->showConsole;
	if(!showDebugLogWindow || !*showDebugLogWindow){
		return;
	}

	ImGuiWindowClass window_class;
	window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&window_class);

	//ImGuiWindowFlags toolbar_window_flags = ImGuiWindowFlags_NoCollapse;
	ImGuiWindowFlags toolbar_window_flags = 0;
	if(ImGui::Begin("Debug Log", showDebugLogWindow, toolbar_window_flags)){

		ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer));
		ImGui::SameLine();
		if(ImGui::Button("Clear") && logSink){
			logSink->Clear();
		}
		ImGui::SameLine();
		ImGui::Checkbox("Auto Scroll", &autoScroll);

		ImGui::Separator();

		for(int i = (int)LogLevel::Trace; i <= (int)LogLevel::Critical; ++i){
			LogLevel level = static_cast<LogLevel>(i);
			bool selected = levelFilter.find(level) != levelFilter.end();
			if(ImGui::Checkbox(LevelFilterString(level).c_str(), &selected)){
				if(selected)
					levelFilter.insert(level);
				else
					levelFilter.erase(level);
			}
			if(i < (int)LogLevel::Critical) ImGui::SameLine();
		}

		if(ImGui::BeginChild("LogRegion", ImVec2(0, 0), false)){

			if(logSink){
				const auto& entries = logSink->GetEntries();
				for(const auto& entry : entries){
					if(!PassesFilter(entry)) continue;

					ImVec4 color = GetColorForLevel(entry.level);
					ImGui::PushStyleColor(ImGuiCol_Text, color);
					ImGui::Text(ToU8String((const char*)u8"[%s] %s\n(関数名 %s,ファイル %s ,行 %d)").c_str(),
								LevelToString(entry.level),
								entry.message.c_str(),
								entry.function.c_str(),
								entry.file.c_str(),
								entry.line);
					ImGui::PopStyleColor();
				}

				if(m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f)
					ImGui::SetScrollHereY(1.0f);
			}
		}
		ImGui::EndChild();
	}
	ImGui::End();
}
