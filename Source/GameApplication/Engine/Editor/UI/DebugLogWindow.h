// =======================================================================
//
// DebugLogWindow.h
//
// =======================================================================
#pragma once

#include "Editor/InterFace/IEditorUI.h"
#include "Service/DebugTools/DebugSystem.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

class DebugLogWindow: public IEditorUI{
public:
	DebugLogWindow();
	void Initialize(EditorService* editor) override;
	void Finalize() override{}
	void Draw(const EditorDrawContext ctx) override;

private:
	struct VisibleLogRow {
		std::size_t entryIndex = 0;
		std::size_t repeatCount = 1;
	};

	EditorService* m_editor = nullptr;
	std::shared_ptr<MemoryLogSink> logSink;

	char searchBuffer[256]{};
	bool autoScroll = true;
	bool collapseDuplicates = true;
	std::unordered_set<LogLevel> levelFilter;

	std::vector<LogEntry> cachedEntries;
	std::vector<VisibleLogRow> visibleRows;
	std::array<int, 6> levelCounts{};
	std::size_t selectedEntryIndex = static_cast<std::size_t>(-1);
	uint64_t cachedRevision = UINT64_MAX;
	std::string cachedSearch;
	uint32_t cachedFilterMask = 0;
	bool cachedCollapseDuplicates = true;

	bool PassesFilter(const LogEntry& entry) const;
	bool IsSameLog(const LogEntry& left, const LogEntry& right) const;
	const char* LevelToString(LogLevel level) const;
	std::string LevelFilterString(LogLevel level) const;
	std::string FormatTimestamp(const LogEntry& entry) const;
	std::string FormatSource(const LogEntry& entry) const;
	ImVec4 GetColorForLevel(LogLevel level) const;
	uint32_t BuildFilterMask() const;
	void RefreshCache(bool force = false);
	void DrawToolbar(bool& filterChanged);
	void DrawLevelFilters(bool& filterChanged);
	void DrawLogRows();
	void DrawSelectedDetails();
};
