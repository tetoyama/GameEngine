// =======================================================================
// 
// DebugLogWindow.h
// 
// =======================================================================
#pragma once
#include "Editor/InterFace/IEditorUI.h"
#include "Service/DebugTools/DebugSystem.h"

// デバッグログ表示ウィンドウ
class DebugLogWindow: public IEditorUI{
public:
	void Initialize(EditorService* editor) override;
	void Finalize() override{}
	void Draw(const EditorDrawContext ctx) override;


	DebugLogWindow();

private:
	EditorService* m_pEditor = nullptr;

	std::shared_ptr<MemoryLogSink> m_LogSink;

	char m_SearchBuffer[128] = "";
	bool m_AutoScroll= true;
	std::unordered_set<LogLevel> m_LevelFilter;

	bool PassesFilter(const LogEntry& entry) const;
	const char* LevelToString(LogLevel level) const;
	std::string LevelFilterString(LogLevel level) const;
	ImVec4 GetColorForLevel(LogLevel level) const;

	// char* → UTF-8 相当の std::string（C++11～17 互換）
	std::string ToU8String(const char* cstr){
		if(!cstr) return {};
		return std::string(cstr, cstr + std::strlen(cstr));
	}

	// オーバーロード：std::string から
	std::string ToU8String(const std::string& s){
		return std::string(s.begin(), s.end());
	}
};

