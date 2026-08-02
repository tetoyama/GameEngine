// =======================================================================
//
// DebugLogWindow.cpp
//
// =======================================================================
#include "DebugLogWindow.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>
#include <windows.h>

#include <ImGui/imgui_internal.h>

#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"
#include "Editor/UI/ModernImGui/ModernImGui.h"

namespace {

std::string Lowercase(std::string value){
	std::transform(
		value.begin(),
		value.end(),
		value.begin(),
		[](unsigned char character){
			return static_cast<char>(std::tolower(character));
		}
	);
	return value;
}

std::string TruncateToWidth(std::string text, float width){
	if(width <= 0.0f) return {};
	if(ImGui::CalcTextSize(text.c_str()).x <= width) return text;
	while(!text.empty() &&
		ImGui::CalcTextSize((text + "...").c_str()).x > width){
		text.pop_back();
	}
	return text.empty() ? "..." : text + "...";
}

} // namespace

DebugLogWindow::DebugLogWindow(){
	for(int index = static_cast<int>(LogLevel::Trace);
		index <= static_cast<int>(LogLevel::Critical);
		++index){
		levelFilter.insert(static_cast<LogLevel>(index));
	}
}

bool DebugLogWindow::PassesFilter(const LogEntry& entry) const{
	if(levelFilter.find(entry.level) == levelFilter.end()) return false;
	if(searchBuffer[0] == '\0') return true;

	const std::string search = Lowercase(searchBuffer);
	return Lowercase(entry.message).find(search) != std::string::npos ||
		Lowercase(entry.function).find(search) != std::string::npos ||
		Lowercase(entry.file).find(search) != std::string::npos;
}

bool DebugLogWindow::IsSameLog(
	const LogEntry& left,
	const LogEntry& right
) const{
	return left.level == right.level &&
		left.message == right.message &&
		left.function == right.function &&
		left.file == right.file &&
		left.line == right.line;
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
	const std::size_t index = static_cast<std::size_t>(level);
	const int count = index < levelCounts.size() ? levelCounts[index] : 0;
	return std::string(LevelToString(level)) + " " + std::to_string(count);
}

std::string DebugLogWindow::FormatTimestamp(const LogEntry& entry) const{
	const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
		entry.timestamp.time_since_epoch()
	) % 1000;
	const std::time_t time = std::chrono::system_clock::to_time_t(entry.timestamp);
	std::tm local{};
	localtime_s(&local, &time);

	char buffer[32]{};
	std::snprintf(
		buffer,
		sizeof(buffer),
		"%02d:%02d:%02d.%03lld",
		local.tm_hour,
		local.tm_min,
		local.tm_sec,
		static_cast<long long>(milliseconds.count())
	);
	return buffer;
}

std::string DebugLogWindow::FormatSource(const LogEntry& entry) const{
	const std::string filename = std::filesystem::path(entry.file).filename().string();
	return filename + ":" + std::to_string(entry.line) + " · " + entry.function;
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
	if(!force &&
		revision == cachedRevision &&
		filterMask == cachedFilterMask &&
		search == cachedSearch &&
		collapseDuplicates == cachedCollapseDuplicates){
		return;
	}

	if(revision != cachedRevision){
		cachedEntries = logSink->GetSnapshot();
		levelCounts.fill(0);
		for(const LogEntry& entry : cachedEntries){
			const std::size_t index = static_cast<std::size_t>(entry.level);
			if(index < levelCounts.size()) ++levelCounts[index];
		}
		if(selectedEntryIndex >= cachedEntries.size()){
			selectedEntryIndex = static_cast<std::size_t>(-1);
		}
	}

	visibleRows.clear();
	visibleRows.reserve(cachedEntries.size());
	for(std::size_t index = 0; index < cachedEntries.size(); ++index){
		if(!PassesFilter(cachedEntries[index])) continue;

		if(collapseDuplicates && !visibleRows.empty()){
			VisibleLogRow& previous = visibleRows.back();
			if(IsSameLog(cachedEntries[previous.entryIndex], cachedEntries[index])){
				++previous.repeatCount;
				continue;
			}
		}
		visibleRows.push_back({index, 1});
	}

	cachedRevision = revision;
	cachedFilterMask = filterMask;
	cachedSearch = search;
	cachedCollapseDuplicates = collapseDuplicates;
}

void DebugLogWindow::Initialize(EditorService* editor){
	m_editor = editor;
	logSink = editor->debugLogSystem->GetSink<MemoryLogSink>();
	RefreshCache(true);
}

void DebugLogWindow::DrawToolbar(bool& filterChanged){
	const MImGui::Theme& theme = MImGui::GetTheme();
	const float available = ImGui::GetContentRegionAvail().x;
	const bool wide = available >= 620.0f;
	const float clearWidth = 62.0f;
	const float stateWidth = 74.0f;
	const float searchWidth = wide
		? (std::max)(
			160.0f,
			available - clearWidth - stateWidth * 2.0f -
				ImGui::GetStyle().ItemSpacing.x * 3.0f
		)
		: -1.0f;

	filterChanged |= MImGui::SearchField(
		"##LogSearch",
		"Search messages, functions or files...",
		searchBuffer,
		sizeof(searchBuffer),
		searchWidth
	);
	if(wide) ImGui::SameLine();
	else ImGui::Spacing();

	if(MImGui::Button(
		"Clear##LogClear",
		ImVec2(clearWidth, theme.compactHeight),
		MImGui::ButtonKind::Ghost
	) && logSink){
		logSink->Clear();
		selectedEntryIndex = static_cast<std::size_t>(-1);
		filterChanged = true;
	}
	ImGui::SameLine();
	if(MImGui::Button(
		"Follow##LogFollow",
		ImVec2(stateWidth, theme.compactHeight),
		autoScroll ? MImGui::ButtonKind::Secondary : MImGui::ButtonKind::Ghost
	)){
		autoScroll = !autoScroll;
	}
	ImGui::SameLine();
	if(MImGui::Button(
		"Group##LogGroup",
		ImVec2(stateWidth, theme.compactHeight),
		collapseDuplicates
			? MImGui::ButtonKind::Secondary
			: MImGui::ButtonKind::Ghost
	)){
		collapseDuplicates = !collapseDuplicates;
		filterChanged = true;
	}

	ImGui::TextDisabled(
		"%llu logs · %llu visible rows",
		static_cast<unsigned long long>(cachedEntries.size()),
		static_cast<unsigned long long>(visibleRows.size())
	);
}

void DebugLogWindow::DrawLevelFilters(bool& filterChanged){
	const float width = ImGui::GetContentRegionAvail().x;
	const int columns = width >= 760.0f ? 6 : (width >= 420.0f ? 3 : 2);
	if(!ImGui::BeginTable(
		"LogLevelFilters",
		columns,
		ImGuiTableFlags_SizingStretchSame |
		ImGuiTableFlags_NoSavedSettings |
		ImGuiTableFlags_NoPadOuterX
	)) return;

	for(int index = static_cast<int>(LogLevel::Trace);
		index <= static_cast<int>(LogLevel::Critical);
		++index){
		const LogLevel level = static_cast<LogLevel>(index);
		bool selected = levelFilter.find(level) != levelFilter.end();
		ImGui::TableNextColumn();
		ImGui::PushID(index);
		const std::string label = "   " + LevelFilterString(level);
		if(MImGui::Button(
			label.c_str(),
			ImVec2(-1.0f, MImGui::GetTheme().compactHeight),
			selected ? MImGui::ButtonKind::Secondary : MImGui::ButtonKind::Ghost
		)){
			selected = !selected;
			if(selected) levelFilter.insert(level);
			else levelFilter.erase(level);
			filterChanged = true;
		}

		const ImVec2 minimum = ImGui::GetItemRectMin();
		const ImVec2 maximum = ImGui::GetItemRectMax();
		ImGui::GetWindowDrawList()->AddCircleFilled(
			ImVec2(minimum.x + 10.0f, (minimum.y + maximum.y) * 0.5f),
			3.0f,
			ImGui::GetColorU32(GetColorForLevel(level))
		);
		ImGui::PopID();
	}
	ImGui::EndTable();
}

void DebugLogWindow::DrawLogRows(){
	const bool hasSelection =
		selectedEntryIndex < cachedEntries.size();
	const float availableHeight = ImGui::GetContentRegionAvail().y;
	const float detailsHeight = hasSelection && availableHeight >= 260.0f
		? (std::min)(170.0f, availableHeight * 0.34f)
		: 0.0f;
	const float listHeight = detailsHeight > 0.0f
		? -detailsHeight - ImGui::GetStyle().ItemSpacing.y
		: 0.0f;

	const MImGui::Theme& theme = MImGui::GetTheme();
	ImGui::PushStyleColor(
		ImGuiCol_ChildBg,
		MImGui::WithAlpha(theme.panel, 0.38f)
	);
	if(ImGui::BeginChild("LogRegion", ImVec2(0.0f, listHeight), false)){
		const bool wasAtBottom =
			ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f;
		const float rowHeight = ImGui::GetTextLineHeightWithSpacing() * 2.25f;
		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(visibleRows.size()), rowHeight);
		while(clipper.Step()){
			for(int displayIndex = clipper.DisplayStart;
				displayIndex < clipper.DisplayEnd;
				++displayIndex){
				const VisibleLogRow& row =
					visibleRows[static_cast<std::size_t>(displayIndex)];
				if(row.entryIndex >= cachedEntries.size()) continue;
				const LogEntry& entry = cachedEntries[row.entryIndex];

				ImGui::PushID(displayIndex);
				const ImVec2 minimum = ImGui::GetCursorScreenPos();
				const ImVec2 size(ImGui::GetContentRegionAvail().x, rowHeight);
				ImGui::InvisibleButton("##LogRow", size);
				const ImVec2 maximum(minimum.x + size.x, minimum.y + size.y);
				const bool hovered = ImGui::IsItemHovered();
				const bool selected = selectedEntryIndex == row.entryIndex;
				if(ImGui::IsItemClicked()) selectedEntryIndex = row.entryIndex;

				ImDrawList* drawList = ImGui::GetWindowDrawList();
				if(selected || hovered || (displayIndex & 1) != 0){
					drawList->AddRectFilled(
						minimum,
						maximum,
						ImGui::GetColorU32(MImGui::WithAlpha(
							selected ? theme.selected : theme.raised,
							selected ? 0.28f : (hovered ? 0.22f : 0.10f)
						)),
						3.0f
					);
				}
				if(selected){
					drawList->AddLine(
						minimum,
						ImVec2(minimum.x, maximum.y),
						ImGui::GetColorU32(theme.accent),
						2.0f
					);
				}

				const std::string timestamp = FormatTimestamp(entry);
				const std::string source = FormatSource(entry);
				const float firstLineY = minimum.y + 4.0f;
				const float secondLineY = minimum.y +
					ImGui::GetTextLineHeightWithSpacing() + 3.0f;
				const float timestampWidth = 92.0f;
				const float levelWidth = 62.0f;
				const float repeatWidth = row.repeatCount > 1 ? 48.0f : 0.0f;
				const float messageX = minimum.x + timestampWidth + levelWidth;
				const float messageWidth =
					maximum.x - messageX - repeatWidth - 10.0f;

				drawList->AddText(
					ImVec2(minimum.x + 8.0f, firstLineY),
					ImGui::GetColorU32(theme.textDisabled),
					timestamp.c_str()
				);
				drawList->AddText(
					ImVec2(minimum.x + timestampWidth, firstLineY),
					ImGui::GetColorU32(GetColorForLevel(entry.level)),
					LevelToString(entry.level)
				);
				const std::string message =
					TruncateToWidth(entry.message, messageWidth);
				drawList->AddText(
					ImVec2(messageX, firstLineY),
					ImGui::GetColorU32(theme.textPrimary),
					message.c_str()
				);
				const std::string sourceText =
					TruncateToWidth(source, maximum.x - minimum.x - 18.0f);
				drawList->AddText(
					ImVec2(minimum.x + 8.0f, secondLineY),
					ImGui::GetColorU32(theme.textSecondary),
					sourceText.c_str()
				);

				if(row.repeatCount > 1){
					char repeat[32]{};
					std::snprintf(
						repeat,
						sizeof(repeat),
						"x%llu",
						static_cast<unsigned long long>(row.repeatCount)
					);
					const ImVec2 repeatSize = ImGui::CalcTextSize(repeat);
					drawList->AddText(
						ImVec2(maximum.x - repeatSize.x - 8.0f, firstLineY),
						ImGui::GetColorU32(theme.textSecondary),
						repeat
					);
				}

				if(hovered) ImGui::SetTooltip("%s", entry.message.c_str());
				if(ImGui::BeginPopupContextItem("LogRowContext")){
					if(ImGui::MenuItem("Copy Message")){
						ImGui::SetClipboardText(entry.message.c_str());
					}
					if(ImGui::MenuItem("Copy Source")){
						ImGui::SetClipboardText(source.c_str());
					}
					if(ImGui::MenuItem("Copy Full Entry")){
						const std::string full = timestamp + " [" +
							LevelToString(entry.level) + "] " + entry.message +
							"\n" + source;
						ImGui::SetClipboardText(full.c_str());
					}
					ImGui::EndPopup();
				}
				ImGui::PopID();
			}
		}
		if(autoScroll && wasAtBottom) ImGui::SetScrollHereY(1.0f);
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();

	if(detailsHeight > 0.0f) DrawSelectedDetails();
}

void DebugLogWindow::DrawSelectedDetails(){
	if(selectedEntryIndex >= cachedEntries.size()) return;
	const LogEntry& entry = cachedEntries[selectedEntryIndex];
	const MImGui::Theme& theme = MImGui::GetTheme();

	ImGui::PushStyleColor(
		ImGuiCol_ChildBg,
		MImGui::WithAlpha(theme.raised, 0.44f)
	);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, theme.cornerRadius);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(9.0f, 8.0f));
	if(ImGui::BeginChild("LogDetails", ImVec2(0.0f, 0.0f), false)){
		ImGui::PushStyleColor(ImGuiCol_Text, GetColorForLevel(entry.level));
		ImGui::TextUnformatted(LevelToString(entry.level));
		ImGui::PopStyleColor();
		ImGui::SameLine();
		ImGui::TextDisabled("%s", FormatTimestamp(entry).c_str());

		const float closeWidth = 30.0f;
		const float closeX = ImGui::GetWindowContentRegionMax().x - closeWidth;
		if(closeX > ImGui::GetCursorPosX()){
			ImGui::SameLine();
			ImGui::SetCursorPosX(closeX);
			if(MImGui::Button(
				"x##CloseLogDetails",
				ImVec2(closeWidth, MImGui::GetTheme().compactHeight),
				MImGui::ButtonKind::Ghost
			)){
				selectedEntryIndex = static_cast<std::size_t>(-1);
			}
		}

		ImGui::Separator();
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextUnformatted(entry.message.c_str());
		ImGui::PopTextWrapPos();
		ImGui::Spacing();
		ImGui::TextDisabled("Source");
		ImGui::SameLine();
		ImGui::TextUnformatted(FormatSource(entry).c_str());
		ImGui::TextDisabled("Path");
		ImGui::SameLine();
		ImGui::TextWrapped("%s", entry.file.c_str());

		if(MImGui::Button(
			"Copy Message",
			ImVec2(108.0f, MImGui::GetTheme().compactHeight),
			MImGui::ButtonKind::Secondary
		)){
			ImGui::SetClipboardText(entry.message.c_str());
		}
		ImGui::SameLine();
		if(MImGui::Button(
			"Copy Source",
			ImVec2(102.0f, MImGui::GetTheme().compactHeight),
			MImGui::ButtonKind::Ghost
		)){
			const std::string source = FormatSource(entry);
			ImGui::SetClipboardText(source.c_str());
		}

		std::error_code error;
		const std::filesystem::path sourcePath(entry.file);
		if(std::filesystem::exists(sourcePath, error)){
			ImGui::SameLine();
			if(MImGui::Button(
				"Show in Explorer",
				ImVec2(128.0f, MImGui::GetTheme().compactHeight),
				MImGui::ButtonKind::Ghost
			)){
				ShellExecuteA(
					nullptr,
					"open",
					sourcePath.parent_path().string().c_str(),
					nullptr,
					nullptr,
					SW_SHOW
				);
			}
		}
	}
	ImGui::EndChild();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();
}

void DebugLogWindow::Draw(const EditorDrawContext ctx){
	(void)ctx;
	bool* showDebugLogWindow = &m_editor->GetUI<MenuBar>()->showConsole;
	if(!showDebugLogWindow || !*showDebugLogWindow) return;

	RefreshCache();

	ImGuiWindowClass windowClass;
	windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&windowClass);
	if(!ImGui::Begin("Debug Log", showDebugLogWindow, 0)){
		ImGui::End();
		return;
	}

	bool filterChanged = false;
	DrawToolbar(filterChanged);
	ImGui::Spacing();
	DrawLevelFilters(filterChanged);
	if(filterChanged) RefreshCache(true);
	ImGui::Spacing();
	DrawLogRows();
	ImGui::End();
}
