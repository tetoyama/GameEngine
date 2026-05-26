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
		m_LevelFilter.insert(static_cast<LogLevel>(i));
	}
}

bool DebugLogWindow::PassesFilter(const LogEntry& entry) const{
	if(m_LevelFilter.find(entry.level) == m_LevelFilter.end()) return false;

	if(strlen(m_SearchBuffer) > 0){
		if(entry.message.find(m_SearchBuffer) == std::string::npos &&
		   entry.function.find(m_SearchBuffer) == std::string::npos)
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

	int count= 0;

	const auto& entries = m_LogSink->GetEntries();
	for(const auto& entry : entries){
		if(entry.level != level) continue;
		count++;
	}

	std::string m_A= LevelToString(level);
	m_A = m_A + "(" + std::to_string(count) + ")";
	return m_A;
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
	m_pEditor = editor;
	m_LogSink = editor->pDebugLogSystem->GetSink<MemoryLogSink>();
}

void DebugLogWindow::Draw(const EditorDrawContext ctx){

	bool* showDebugLogWindow = &m_pEditor->GetUI<MenuBar>()->showConsole;
	if(!showDebugLogWindow || !*showDebugLogWindow){
		return;
	}

	ImGuiWindowClass windowClass;
	windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&windowClass);

	//ImGuiWindowFlags toolbar_window_flags = ImGuiWindowFlags_NoCollapse;
	ImGuiWindowFlags toolbarWindowFlags= 0;
	if(ImGui::Begin("Debug Log", showDebugLogWindow, toolbarWindowFlags)){

		ImGui::InputText("Search", m_SearchBuffer, sizeof(m_SearchBuffer));
		ImGui::SameLine();
		if(ImGui::Button("Clear") && m_LogSink){
			m_LogSink->Clear();
		}
		ImGui::SameLine();
		ImGui::Checkbox("Auto Scroll", &m_AutoScroll);
		ImGui::Separator();

		for(int i = (int)LogLevel::Trace; i <= (int)LogLevel::Critical; ++i){
			LogLevel m_Level= static_cast<LogLevel>(i);
			bool m_Selected= m_LevelFilter.find(m_Level) != m_LevelFilter.end();
			if(ImGui::Checkbox(LevelFilterString(m_Level).c_str(), &m_Selected)){
				if(m_Selected)
					m_LevelFilter.insert(m_Level);
				else
					m_LevelFilter.erase(m_Level);
			}
			if(i < (int)LogLevel::Critical) ImGui::SameLine();
		}

		if(ImGui::BeginChild("LogRegion", ImVec2(0, 0), false)){

			if(m_LogSink){
				const auto& entries = m_LogSink->GetEntries();
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

				if(m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f)
					ImGui::SetScrollHereY(1.0f);
			}
		}
		ImGui::EndChild();
	}
	ImGui::End();
}
