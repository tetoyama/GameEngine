// =======================================================================
// 
// DebugLogWindow.cpp
// 
// =======================================================================
#include "DebugLogWindow.h"

#include <ImGui/imgui_internal.h>

#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <sstream>

#include <sceneManager.h>

#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"
#include "Editor/UI/ModernImGui/ModernImGui.h"

namespace {

std::string ToLowerCopy(const std::string& value){
	std::string result = value;
	std::transform(
		result.begin(),
		result.end(),
		result.begin(),
		[](unsigned char character){
			return static_cast<char>(std::tolower(character));
		}
	);
	return result;
}

bool ContainsCaseInsensitive(const std::string& value, const std::string& query){
	if(query.empty()) return true;
	return ToLowerCopy(value).find(query) != std::string::npos;
}

const char* FileNameOnly(const std::string& path){
	const size_t separator = path.find_last_of("/\\");
	return separator == std::string::npos
		? path.c_str()
		: path.c_str() + separator + 1;
}

void AddClippedText(
	ImDrawList* drawList,
	const ImVec2& position,
	ImU32 color,
	const char* text,
	const ImVec4& clipRect
){
	drawList->AddText(
		nullptr,
		0.0f,
		position,
		color,
		text,
		nullptr,
		0.0f,
		&clipRect
	);
}

} // namespace

DebugLogWindow::DebugLogWindow(){
	for(int i = static_cast<int>(LogLevel::Trace);
		i <= static_cast<int>(LogLevel::Critical);
		++i){
		levelFilter.insert(static_cast<LogLevel>(i));
	}
}

bool DebugLogWindow::PassesFilter(const LogEntry& entry) const{
	if(levelFilter.find(entry.level) == levelFilter.end()) return false;

	const std::string query = ToLowerCopy(searchBuffer);
	if(query.empty()) return true;
	return ContainsCaseInsensitive(entry.message, query) ||
		ContainsCaseInsensitive(entry.function, query) ||
		ContainsCaseInsensitive(entry.file, query);
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

void DebugLogWindow::RebuildVisibleRows(){
	visibleRows.clear();
	visibleRows.reserve(cachedEntries.size());
	filteredEntryCount = 0;

	for(size_t index = 0; index < cachedEntries.size(); ++index){
		const LogEntry& entry = cachedEntries[index];
		if(!PassesFilter(entry)) continue;
		++filteredEntryCount;

		if(collapseDuplicates && !visibleRows.empty()){
			VisibleLogRow& previous = visibleRows.back();
			if(IsSameLog(cachedEntries[previous.entryIndex], entry)){
				++previous.repeatCount;
				continue;
			}
		}
		visibleRows.push_back({index, 1});
	}
}

void DebugLogWindow::RefreshCache(bool force){
	if(!logSink) return;

	const uint64_t revision = logSink->GetRevision();
	const uint32_t filterMask = BuildFilterMask();
	const std::string search = ToLowerCopy(searchBuffer);
	const bool sourceChanged = cachedRevision == UINT64_MAX ||
		revision != cachedRevision;
	const bool filterChanged = filterMask != cachedFilterMask ||
		search != cachedSearch ||
		collapseDuplicates != cachedCollapseDuplicates;

	bool snapshotUpdated = false;
	if(!capturePaused && sourceChanged){
		cachedEntries = logSink->GetSnapshot();
		cachedRevision = revision;
		levelCounts.fill(0);
		for(const LogEntry& entry : cachedEntries){
			const size_t index = static_cast<size_t>(entry.level);
			if(index < levelCounts.size()) ++levelCounts[index];
		}
		if(selectedEntryIndex >= cachedEntries.size()){
			selectedEntryIndex = InvalidEntryIndex;
		}
		snapshotUpdated = true;
	}

	if(!force && !snapshotUpdated && !filterChanged) return;
	RebuildVisibleRows();
	cachedFilterMask = filterMask;
	cachedSearch = search;
	cachedCollapseDuplicates = collapseDuplicates;
}

void DebugLogWindow::ClearLocalCache(){
	cachedEntries.clear();
	visibleRows.clear();
	levelCounts.fill(0);
	filteredEntryCount = 0;
	selectedEntryIndex = InvalidEntryIndex;
	cachedRevision = logSink ? logSink->GetRevision() : UINT64_MAX;
	cachedFilterMask = BuildFilterMask();
	cachedSearch = ToLowerCopy(searchBuffer);
	cachedCollapseDuplicates = collapseDuplicates;
}

std::string DebugLogWindow::FormatTimestamp(
	const std::chrono::system_clock::time_point& timestamp
) const{
	const std::time_t time = std::chrono::system_clock::to_time_t(timestamp);
	std::tm localTime{};
	localtime_s(&localTime, &time);
	const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
		timestamp.time_since_epoch()
	).count() % 1000;

	char buffer[32]{};
	std::snprintf(
		buffer,
		sizeof(buffer),
		"%02d:%02d:%02d.%03lld",
		localTime.tm_hour,
		localTime.tm_min,
		localTime.tm_sec,
		static_cast<long long>(milliseconds)
	);
	return buffer;
}

std::string DebugLogWindow::BuildEntryDetails(const LogEntry& entry) const{
	std::ostringstream details;
	details << '[' << FormatTimestamp(entry.timestamp) << "] "
		<< LevelToString(entry.level) << '\n'
		<< entry.message << '\n'
		<< entry.function << '\n'
		<< entry.file << ':' << entry.line;
	return details.str();
}

void DebugLogWindow::Initialize(EditorService* editor){
	m_editor = editor;
	logSink = editor->debugLogSystem->GetSink<MemoryLogSink>();
	RefreshCache(true);
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

	const MImGui::Theme& theme = MImGui::GetTheme();
	const ImGuiStyle& style = ImGui::GetStyle();
	const float spacing = style.ItemSpacing.x;
	const float toolbarWidth = ImGui::GetContentRegionAvail().x;
	const bool compactToolbar = toolbarWidth < 540.0f;
	const float pauseWidth = capturePaused ? 78.0f : 68.0f;
	const float clearWidth = 62.0f;
	const float searchWidth = compactToolbar
		? toolbarWidth
		: (std::max)(
			140.0f,
			toolbarWidth - pauseWidth - clearWidth - spacing * 2.0f
		);

	bool viewChanged = MImGui::SearchField(
		"##LogSearch",
		"Search messages, functions or files...",
		searchBuffer,
		sizeof(searchBuffer),
		searchWidth
	);
	if(compactToolbar){
		ImGui::Dummy(ImVec2(0.0f, 2.0f));
	}else{
		ImGui::SameLine();
	}

	if(MImGui::Button(
		capturePaused ? "Resume##LogCapture" : "Pause##LogCapture",
		ImVec2(pauseWidth, theme.compactHeight),
		capturePaused ? MImGui::ButtonKind::Secondary : MImGui::ButtonKind::Ghost
	)){
		capturePaused = !capturePaused;
		if(!capturePaused) RefreshCache(true);
	}
	ImGui::SameLine();
	if(MImGui::Button(
		"Clear##LogClear",
		ImVec2(clearWidth, theme.compactHeight),
		MImGui::ButtonKind::Ghost
	) && logSink){
		logSink->Clear();
		ClearLocalCache();
	}

	ImGui::Dummy(ImVec2(0.0f, 2.0f));
	MImGui::Toggle("Auto Scroll##LogAutoScroll", &autoScroll);
	const float collapseWidth = ImGui::CalcTextSize("Collapse Repeats").x + 48.0f;
	if(ImGui::GetContentRegionAvail().x > collapseWidth + spacing){
		ImGui::SameLine();
	}
	if(MImGui::Toggle("Collapse Repeats##LogCollapse", &collapseDuplicates)){
		viewChanged = true;
	}

	if(viewChanged) RefreshCache(true);

	const uint64_t sinkRevision = logSink ? logSink->GetRevision() : cachedRevision;
	const uint64_t pendingUpdates = capturePaused &&
		cachedRevision != UINT64_MAX &&
		sinkRevision > cachedRevision
		? sinkRevision - cachedRevision
		: 0;
	ImGui::SameLine();
	if(capturePaused){
		ImGui::TextDisabled(
			"Paused · %llu update%s pending",
			static_cast<unsigned long long>(pendingUpdates),
			pendingUpdates == 1 ? "" : "s"
		);
	}else{
		ImGui::TextDisabled(
			"%d entries · %llu rows",
			filteredEntryCount,
			static_cast<unsigned long long>(visibleRows.size())
		);
	}

	ImGui::Dummy(ImVec2(0.0f, 3.0f));
	float chipLineWidth = 0.0f;
	const float availableChipWidth = ImGui::GetContentRegionAvail().x;
	for(int index = static_cast<int>(LogLevel::Trace);
		index <= static_cast<int>(LogLevel::Critical);
		++index){
		const LogLevel level = static_cast<LogLevel>(index);
		const bool selected = levelFilter.find(level) != levelFilter.end();
		const std::string visibleLabel = LevelFilterString(level);
		const float chipWidth = (std::max)(
			72.0f,
			ImGui::CalcTextSize(visibleLabel.c_str()).x + 30.0f
		);
		if(chipLineWidth > 0.0f &&
			chipLineWidth + spacing + chipWidth > availableChipWidth){
			chipLineWidth = 0.0f;
		}else if(chipLineWidth > 0.0f){
			ImGui::SameLine();
			chipLineWidth += spacing;
		}

		ImGui::PushID(index);
		const std::string buttonLabel = "   " + visibleLabel + "##LevelFilter";
		if(MImGui::Button(
			buttonLabel.c_str(),
			ImVec2(chipWidth, theme.compactHeight),
			selected ? MImGui::ButtonKind::Secondary : MImGui::ButtonKind::Ghost
		)){
			if(selected) levelFilter.erase(level);
			else levelFilter.insert(level);
			viewChanged = true;
		}
		const ImVec2 itemMin = ImGui::GetItemRectMin();
		const ImVec2 itemMax = ImGui::GetItemRectMax();
		ImGui::GetWindowDrawList()->AddCircleFilled(
			ImVec2(itemMin.x + 10.0f, (itemMin.y + itemMax.y) * 0.5f),
			3.0f,
			ImGui::GetColorU32(GetColorForLevel(level))
		);
		ImGui::PopID();
		chipLineWidth += chipWidth;
	}
	if(viewChanged) RefreshCache(true);

	ImGui::Dummy(ImVec2(0.0f, 3.0f));
	const bool hasSelection = selectedEntryIndex < cachedEntries.size();
	const float contentHeight = ImGui::GetContentRegionAvail().y;
	const bool showDetailPanel = hasSelection && contentHeight >= 210.0f;
	const float detailHeight = showDetailPanel
		? (std::min)(150.0f, contentHeight * 0.36f)
		: 0.0f;
	const float listHeight = (std::max)(
		80.0f,
		contentHeight - detailHeight - (showDetailPanel ? style.ItemSpacing.y : 0.0f)
	);

	ImGui::PushStyleColor(ImGuiCol_ChildBg, MImGui::WithAlpha(theme.panel, 0.42f));
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, theme.cornerRadius);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 5.0f));
	if(ImGui::BeginChild("LogRegion", ImVec2(0.0f, listHeight), false)){
		const bool wasNearBottom =
			ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f;
		const float rowHeight = 31.0f;
		if(visibleRows.empty()){
			const char* emptyText = cachedEntries.empty()
				? "No log entries captured."
				: "No entries match the active filters.";
			const ImVec2 textSize = ImGui::CalcTextSize(emptyText);
			const ImVec2 available = ImGui::GetContentRegionAvail();
			ImGui::SetCursorPos(ImVec2(
				(std::max)(0.0f, (available.x - textSize.x) * 0.5f),
				(std::max)(0.0f, (available.y - textSize.y) * 0.35f)
			));
			ImGui::TextDisabled("%s", emptyText);
		}else{
			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(visibleRows.size()), rowHeight);
			while(clipper.Step()){
				for(int rowIndex = clipper.DisplayStart;
					rowIndex < clipper.DisplayEnd;
					++rowIndex){
					const VisibleLogRow& row = visibleRows[static_cast<size_t>(rowIndex)];
					const LogEntry& entry = cachedEntries[row.entryIndex];
					ImGui::PushID(static_cast<int>(row.entryIndex & 0x7fffffff));

					const ImVec2 rowMin = ImGui::GetCursorScreenPos();
					const float rowWidth = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
					ImGui::InvisibleButton("##LogRow", ImVec2(rowWidth, rowHeight));
					const bool hovered = ImGui::IsItemHovered();
					const bool selected = selectedEntryIndex == row.entryIndex;
					if(ImGui::IsItemClicked()) selectedEntryIndex = row.entryIndex;
					if(hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){
						ImGui::SetClipboardText(entry.message.c_str());
					}

					const ImVec2 rowMax(rowMin.x + rowWidth, rowMin.y + rowHeight);
					ImDrawList* drawList = ImGui::GetWindowDrawList();
					if(selected){
						drawList->AddRectFilled(
							rowMin,
							rowMax,
							ImGui::GetColorU32(MImGui::WithAlpha(theme.selected, 0.52f)),
							4.0f
						);
					}else if(hovered){
						drawList->AddRectFilled(
							rowMin,
							rowMax,
							ImGui::GetColorU32(MImGui::WithAlpha(theme.hover, 0.38f)),
							4.0f
						);
					}else if((rowIndex & 1) != 0){
						drawList->AddRectFilled(
							rowMin,
							rowMax,
							ImGui::GetColorU32(MImGui::WithAlpha(theme.raised, 0.10f)),
							4.0f
						);
					}

					const ImVec4 levelColor = GetColorForLevel(entry.level);
					drawList->AddRectFilled(
						ImVec2(rowMin.x, rowMin.y + 4.0f),
						ImVec2(rowMin.x + 2.0f, rowMax.y - 4.0f),
						ImGui::GetColorU32(levelColor),
						1.0f
					);

					const std::string timestamp = FormatTimestamp(entry.timestamp);
					const float textY = rowMin.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f;
					const float timeX = rowMin.x + 8.0f;
					const float levelX = timeX + 92.0f;
					const float messageX = levelX + 72.0f;
					AddClippedText(
						drawList,
						ImVec2(timeX, textY),
						ImGui::GetColorU32(theme.textSecondary),
						timestamp.c_str(),
						ImVec4(timeX, rowMin.y, levelX - 5.0f, rowMax.y)
					);
					AddClippedText(
						drawList,
						ImVec2(levelX, textY),
						ImGui::GetColorU32(levelColor),
						LevelToString(entry.level),
						ImVec4(levelX, rowMin.y, messageX - 5.0f, rowMax.y)
					);

					float actualMessageX = messageX;
					if(row.repeatCount > 1){
						char repeatText[24]{};
						std::snprintf(repeatText, sizeof(repeatText), "x%d", row.repeatCount);
						const ImVec2 repeatSize = ImGui::CalcTextSize(repeatText);
						const ImVec2 badgeMin(messageX, rowMin.y + 6.0f);
						const ImVec2 badgeMax(
							badgeMin.x + repeatSize.x + 12.0f,
							rowMax.y - 6.0f
						);
						drawList->AddRectFilled(
							badgeMin,
							badgeMax,
							ImGui::GetColorU32(MImGui::WithAlpha(levelColor, 0.18f)),
							(badgeMax.y - badgeMin.y) * 0.5f
						);
						drawList->AddText(
							ImVec2(badgeMin.x + 6.0f, textY),
							ImGui::GetColorU32(levelColor),
							repeatText
						);
						actualMessageX = badgeMax.x + 7.0f;
					}
					AddClippedText(
						drawList,
						ImVec2(actualMessageX, textY),
						ImGui::GetColorU32(theme.textPrimary),
						entry.message.c_str(),
						ImVec4(actualMessageX, rowMin.y, rowMax.x - 7.0f, rowMax.y)
					);

					if(hovered){
						ImGui::SetTooltip(
							"%s\n%s\n%s:%d%s",
							entry.message.c_str(),
							entry.function.c_str(),
							entry.file.c_str(),
							entry.line,
							row.repeatCount > 1 ? "\nRepeated entries are collapsed." : ""
						);
					}
					ImGui::PopID();
				}
			}
		}

		if(autoScroll && !capturePaused && wasNearBottom){
			ImGui::SetScrollHereY(1.0f);
		}
	}
	ImGui::EndChild();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();

	if(showDetailPanel){
		ImGui::Dummy(ImVec2(0.0f, 2.0f));
		const LogEntry& selectedEntry = cachedEntries[selectedEntryIndex];
		const ImVec4 levelColor = GetColorForLevel(selectedEntry.level);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, MImGui::WithAlpha(theme.raised, 0.52f));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, theme.cornerRadius);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
		if(ImGui::BeginChild("LogDetails", ImVec2(0.0f, detailHeight), false)){
			MImGui::Badge(LevelToString(selectedEntry.level), &levelColor);
			ImGui::SameLine();
			ImGui::TextDisabled("%s", FormatTimestamp(selectedEntry.timestamp).c_str());

			const float buttonWidth = 92.0f;
			const float closeWidth = 54.0f;
			const float buttonsWidth = buttonWidth * 2.0f + closeWidth + spacing * 2.0f;
			if(ImGui::GetContentRegionAvail().x > buttonsWidth + 20.0f){
				ImGui::SameLine();
				ImGui::SetCursorPosX(
					ImGui::GetWindowContentRegionMax().x - buttonsWidth
				);
			}
			if(MImGui::Button(
				"Copy Message##LogCopyMessage",
				ImVec2(buttonWidth, theme.compactHeight),
				MImGui::ButtonKind::Ghost
			)){
				ImGui::SetClipboardText(selectedEntry.message.c_str());
			}
			ImGui::SameLine();
			if(MImGui::Button(
				"Copy Details##LogCopyDetails",
				ImVec2(buttonWidth, theme.compactHeight),
				MImGui::ButtonKind::Ghost
			)){
				const std::string details = BuildEntryDetails(selectedEntry);
				ImGui::SetClipboardText(details.c_str());
			}
			ImGui::SameLine();
			if(MImGui::Button(
				"Close##LogCloseDetails",
				ImVec2(closeWidth, theme.compactHeight),
				MImGui::ButtonKind::Ghost
			)){
				selectedEntryIndex = InvalidEntryIndex;
			}

			ImGui::TextWrapped("%s", selectedEntry.message.c_str());
			ImGui::TextDisabled(
				"%s  ·  %s:%d",
				selectedEntry.function.c_str(),
				FileNameOnly(selectedEntry.file),
				selectedEntry.line
			);
			if(ImGui::IsItemHovered()){
				ImGui::SetTooltip("%s", selectedEntry.file.c_str());
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor();
	}

	ImGui::End();
}
