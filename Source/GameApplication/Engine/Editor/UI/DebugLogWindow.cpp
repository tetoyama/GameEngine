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
	const size_t index = static_cast<size_t>(level);
	const int count = index < levelCounts.size() ? levelCounts[index] : 0;
	return std::string(LevelToString(level)) + "(" + std::to_string(count) + ")";
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

uint32_t DebugLogWindow::BuildFilterMask() const{
	uint32_t mask = 0;
	for(LogLevel level : levelFilter){
		mask |= 1u << static_cast<uint32_t>(level);
	}
	return mask;
}

void DebugLogWindow::RefreshCache(bool force){
	if(!logSink) return;
	const uint64_t revision = logSink->GetRevision();
	const uint32_t filterMask = BuildFilterMask();
	const std::string search = searchBuffer;
	if(!force && revision == cachedRevision && filterMask == cachedFilterMask && search == cachedSearch){
		return;
	}

	if(revision != cachedRevision){
		cachedEntries = logSink->GetSnapshot();
		levelCounts.fill(0);
		for(const LogEntry& entry : cachedEntries){
			const size_t index = static_cast<size_t>(entry.level);
			if(index < levelCounts.size()) ++levelCounts[index];
		}
	}

	filteredIndices.clear();
	filteredIndices.reserve(cachedEntries.size());
	for(size_t i = 0; i < cachedEntries.size(); ++i){
		if(PassesFilter(cachedEntries[i])) filteredIndices.push_back(i);
	}

	cachedRevision = revision;
	cachedFilterMask = filterMask;
	cachedSearch = search;
}

void DebugLogWindow::Initialize(EditorService* editor){
	m_editor = editor;
	logSink = editor->debugLogSystem->GetSink<MemoryLogSink>();
	RefreshCache(true);
}

void DebugLogWindow::Draw(const EditorDrawContext ctx){
	bool* showDebugLogWindow = &m_editor->GetUI<MenuBar>()->showConsole;
	if(!showDebugLogWindow || !*showDebugLogWindow){
		return;
	}

	RefreshCache();

	ImGuiWindowClass windowClass;
	windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&windowClass);
	if(!ImGui::Begin("Debug Log", showDebugLogWindow, 0)){
		ImGui::End();
		return;
	}

	bool filterChanged = ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer));
	ImGui::SameLine();
	if(ImGui::Button("Clear") && logSink){
		logSink->Clear();
		filterChanged = true;
	}
	ImGui::SameLine();
	ImGui::Checkbox("Auto Scroll", &autoScroll);
	ImGui::Separator();

	for(int i = static_cast<int>(LogLevel::Trace); i <= static_cast<int>(LogLevel::Critical); ++i){
		const LogLevel level = static_cast<LogLevel>(i);
		bool selected = levelFilter.find(level) != levelFilter.end();
		if(ImGui::Checkbox(LevelFilterString(level).c_str(), &selected)){
			if(selected) levelFilter.insert(level);
			else levelFilter.erase(level);
			filterChanged = true;
		}
		if(i < static_cast<int>(LogLevel::Critical)) ImGui::SameLine();
	}
	if(filterChanged) RefreshCache(true);

	if(ImGui::BeginChild("LogRegion", ImVec2(0, 0), false)){
		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(filteredIndices.size()), ImGui::GetTextLineHeightWithSpacing() * 2.0f);
		while(clipper.Step()){
			for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row){
				const LogEntry& entry = cachedEntries[filteredIndices[static_cast<size_t>(row)]];
				ImGui::PushStyleColor(ImGuiCol_Text, GetColorForLevel(entry.level));
				ImGui::Text(
					ToU8String((const char*)u8"[%s] %s\n(関数名 %s,ファイル %s ,行 %d)").c_str(),
					LevelToString(entry.level),
					entry.message.c_str(),
					entry.function.c_str(),
					entry.file.c_str(),
					entry.line
				);
				ImGui::PopStyleColor();
			}
		}
		if(autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f){
			ImGui::SetScrollHereY(1.0f);
		}
	}
	ImGui::EndChild();
	ImGui::End();
}
