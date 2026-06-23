// =======================================================================
// 
// DebugLogWindow.h
// 
// =======================================================================
#pragma once
#include "Editor/InterFace/IEditorUI.h"
#include "Service/DebugTools/DebugSystem.h"

#include <array>
#include <cstdint>
#include <vector>

// デバッグログ表示ウィンドウ
class DebugLogWindow: public IEditorUI{
public:
	void Initialize(EditorService* editor) override;
	void Finalize() override{}
	void Draw(const EditorDrawContext ctx) override;
	DebugLogWindow();

private:
	EditorService* m_editor = nullptr;
	std::shared_ptr<MemoryLogSink> logSink;

	char searchBuffer[128] = "";
	bool autoScroll = true;
	std::unordered_set<LogLevel> levelFilter;

	std::vector<LogEntry> cachedEntries;
	std::vector<size_t> filteredIndices;
	std::array<int, 6> levelCounts{};
	uint64_t cachedRevision = UINT64_MAX;
	std::string cachedSearch;
	uint32_t cachedFilterMask = 0;

	bool PassesFilter(const LogEntry& entry) const;
	const char* LevelToString(LogLevel level) const;
	std::string LevelFilterString(LogLevel level) const;
	ImVec4 GetColorForLevel(LogLevel level) const;
	uint32_t BuildFilterMask() const;
	void RefreshCache(bool force = false);

	std::string ToU8String(const char* cstr){
		if(!cstr) return {};
		return std::string(cstr, cstr + std::strlen(cstr));
	}
	std::string ToU8String(const std::string& s){
		return std::string(s.begin(), s.end());
	}
};
