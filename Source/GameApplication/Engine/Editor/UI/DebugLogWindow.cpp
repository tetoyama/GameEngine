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
#include "Editor/UI/ModernImGui/ModernImGui.h"

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
	return std::string(LevelToString(level)) + " " + std::to_string(count);
}

ImVec4 DebugLogWindow::GetColorForLevel(LogLevel level) const{
	switch(level){
		case LogLevel::Trace:    return ImVec4(0.72f, 0.74f, 0.78f, 1.0f);
		case LogLevel::Debug:    return ImVec4(0.56f, 0.66f, 0.96f, 1.0f);
		case LogLevel::Info:     return ImVec4(0.48f, 0.84f, 0.58f, 1.0f);
		case LogLevel::Warning:  return ImVec4(0.95f, 0.77f, 0.34f, 1.0f);
		case LogLevel::Error:    return ImVec4(0.95f, 0.42f, 0.42f, 1.0f);
		case LogLevel::Critical: return ImVec4(1.00f, 0.28f, 0.32f, 1.0f);
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
	(void)ctx;
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

	const MImGui::Theme& theme = MImGui::GetTheme();
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float clearWidth = 66.0f;
	const float autoScrollWidth = 130.0f;
	const float searchWidth = (std::max)(
		140.0f,
		ImGui::GetContentRegionAvail().x - clearWidth - autoScrollWidth - spacing * 2.0f
	);

	bool filterChanged = MImGui::SearchField(
		"##LogSearch",
		"Search messages, functions or files...",
		searchBuffer,
		sizeof(searchBuffer),
		searchWidth
	);
	ImGui::SameLine();
	if(MImGui::Button(
		"Clear##LogClear",
		ImVec2(clearWidth, theme.compactHeight),
		MImGui::ButtonKind::Ghost
	) && logSink){
		logSink->Clear();
		filterChanged = true;
	}
	ImGui::SameLine();
	MImGui::Toggle("Auto Scroll", &autoScroll);

	ImGui::Dummy(ImVec2(0.0f, 3.0f));
	if(ImGui::BeginTable(
		"LogLevelFilters",
		6,
		ImGuiTableFlags_SizingStretchSame |
		ImGuiTableFlags_NoSavedSettings |
		ImGuiTableFlags_NoPadOuterX
	)){
		for(int i = static_cast<int>(LogLevel::Trace); i <= static_cast<int>(LogLevel::Critical); ++i){
			const LogLevel level = static_cast<LogLevel>(i);
			bool selected = levelFilter.find(level) != levelFilter.end();
			ImGui::TableNextColumn();
			ImGui::PushID(i);

			std::string label = "   " + LevelFilterString(level);
			if(MImGui::Button(
				label.c_str(),
				ImVec2(-1.0f, theme.compactHeight),
				selected ? MImGui::ButtonKind::Secondary : MImGui::ButtonKind::Ghost
			)){
				selected = !selected;
				if(selected) levelFilter.insert(level);
				else levelFilter.erase(level);
				filterChanged = true;
			}

			const ImVec2 itemMin = ImGui::GetItemRectMin();
			const ImVec2 itemMax = ImGui::GetItemRectMax();
			ImGui::GetWindowDrawList()->AddCircleFilled(
				ImVec2(itemMin.x + 10.0f, (itemMin.y + itemMax.y) * 0.5f),
				3.0f,
				ImGui::GetColorU32(GetColorForLevel(level))
			);
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	if(filterChanged) RefreshCache(true);

	ImGui::Dummy(ImVec2(0.0f, 3.0f));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, MImGui::WithAlpha(theme.panel, 0.48f));
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, theme.cornerRadius);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 7.0f));
	if(ImGui::BeginChild("LogRegion", ImVec2(0, 0), false)){
		const float rowHeight = ImGui::GetTextLineHeightWithSpacing() * 2.25f;
		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(filteredIndices.size()), rowHeight);
		while(clipper.Step()){
			for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row){
				const LogEntry& entry = cachedEntries[filteredIndices[static_cast<size_t>(row)]];
				ImGui::PushID(row);

				const ImVec2 rowMin = ImGui::GetCursorScreenPos();
				const ImVec2 rowMax(
					rowMin.x + ImGui::GetContentRegionAvail().x,
					rowMin.y + rowHeight
				);
				if((row & 1) != 0){
					ImGui::GetWindowDrawList()->AddRectFilled(
						rowMin,
						rowMax,
						ImGui::GetColorU32(MImGui::WithAlpha(theme.raised, 0.16f)),
						3.0f
					);
				}

				ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMin.y + 3.0f));
				ImGui::PushStyleColor(ImGuiCol_Text, GetColorForLevel(entry.level));
				ImGui::TextUnformatted(LevelToString(entry.level));
				ImGui::PopStyleColor();
				ImGui::SameLine(0.0f, 8.0f);
				ImGui::TextUnformatted(entry.message.c_str());

				ImGui::TextDisabled(
					"%s  ·  %s:%d",
					entry.function.c_str(),
					entry.file.c_str(),
					entry.line
				);

				ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMax.y));
				ImGui::PopID();
			}
		}
		if(autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f){
			ImGui::SetScrollHereY(1.0f);
		}
	}
	ImGui::EndChild();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();
	ImGui::End();
}
