// =======================================================================
// 
// DebugLogWindow.h
// 
// =======================================================================
#pragma once
#include "Editor/InterFace/IEditorUI.h"
#include "Service/DebugTools/DebugSystem.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_set>
#include <vector>

// デバッグログ表示ウィンドウ
class DebugLogWindow: public IEditorUI{
public:
	void Initialize(EditorService* editor) override;
	void Finalize() override{}
	void Draw(const EditorDrawContext ctx) override;
	DebugLogWindow();

private:
	struct VisibleLogRow {
		size_t entryIndex = 0;
		int repeatCount = 1;
	};

	static constexpr size_t InvalidEntryIndex =
		(std::numeric_limits<size_t>::max)();

	EditorService* m_editor = nullptr;
	std::shared_ptr<MemoryLogSink> logSink;

	char searchBuffer[128] = "";
	bool autoScroll = true;
	bool capturePaused = false;
	bool collapseDuplicates = true;
	std::unordered_set<LogLevel> levelFilter;

	std::vector<LogEntry> cachedEntries;
	std::vector<VisibleLogRow> visibleRows;
	std::array<int, 6> levelCounts{};
	uint64_t cachedRevision = UINT64_MAX;
	std::string cachedSearch;
	uint32_t cachedFilterMask = 0;
	bool cachedCollapseDuplicates = true;
	int filteredEntryCount = 0;
	size_t selectedEntryIndex = InvalidEntryIndex;

	bool PassesFilter(const LogEntry& entry) const;
	bool IsSameLog(const LogEntry& left, const LogEntry& right) const;
	const char* LevelToString(LogLevel level) const;
	std::string LevelFilterString(LogLevel level) const;
	ImVec4 GetColorForLevel(LogLevel level) const;
	uint32_t BuildFilterMask() const;
	void RefreshCache(bool force = false);
	void RebuildVisibleRows();
	void ClearLocalCache();
	std::string FormatTimestamp(
		const std::chrono::system_clock::time_point& timestamp
	) const;
	std::string BuildEntryDetails(const LogEntry& entry) const;

	std::string ToU8String(const char* cstr){
		if(!cstr) return {};
		return std::string(cstr, cstr + std::strlen(cstr));
	}
	std::string ToU8String(const std::string& s){
		return std::string(s.begin(), s.end());
	}
};
