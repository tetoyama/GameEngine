// DebugSystem.h
#pragma once
#include <string>
#include <chrono>
#include <memory>
#include <vector>
#include <mutex>
#include <Windows.h>

#include <unordered_map>
#include <unordered_set>
#include "Service/IService.h"
#include "Backends/ImGui/imgui.h"

#define LOG_TRACE(msg)    Log(LogLevel::Trace,   msg, __FUNCTION__, __FILE__, __LINE__)
#define LOG_DEBUG(msg)    Log(LogLevel::Debug,   msg, __FUNCTION__, __FILE__, __LINE__)
#define LOG_INFO(msg)     Log(LogLevel::Info,    msg, __FUNCTION__, __FILE__, __LINE__)
#define LOG_WARNING(msg)  Log(LogLevel::Warning, msg, __FUNCTION__, __FILE__, __LINE__)
#define LOG_ERROR(msg)    Log(LogLevel::Error,   msg, __FUNCTION__, __FILE__, __LINE__)
#define LOG_CRITICAL(msg) Log(LogLevel::Critical,msg, __FUNCTION__, __FILE__, __LINE__)

enum class LogLevel {
	Trace,
	Debug,
	Info,
	Warning,
	Error,
	Critical
};

struct LogEntry {
	LogLevel level;
	std::string message;
	std::string function;
	std::string file;
	int line;
	std::chrono::system_clock::time_point timestamp;
};


class ILogSink {
public:
	virtual ~ILogSink() = default;
	virtual void Write(const LogEntry& entry) = 0;
};


class ImGuiLogWindow;
class DebugLogSystem : IService {
public:
	void Initialize(); // 必要ならファイルパスなど渡す
	void Shutdown() override;

	void AddSink(std::shared_ptr<ILogSink> sink);
	void RemoveSink(std::shared_ptr<ILogSink> sink);

	void Log(LogLevel level,
			 const std::string& message,
			 const std::string& function,
			 const std::string& file,
			 int line);

	void Log(LogLevel level,
			 const char8_t* message,
			 const std::string& function,
			 const std::string& file,
			 int line);

	// 便利関数
	void Info(const std::string& message, const std::string& source = "");
	void Error(const std::string& message, const std::string& source = "");

	void Draw();
private:
	std::vector<std::shared_ptr<ILogSink>> sinks;
	std::mutex mutex; // マルチスレッド対応

	std::shared_ptr<ImGuiLogWindow> logWindow;

	std::wstring Utf8ToWide(const std::string& utf8){
		int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
		std::wstring wide(size, 0);
		MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], size);
		wide.pop_back(); // null終端除去
		return wide;
	}
};

class MemoryLogSink: public ILogSink {
public:
	void Write(const LogEntry& entry) override{
		std::lock_guard<std::mutex> lock(mutex);
		entries.push_back(entry);
	}

	const std::vector<LogEntry>& GetEntries() const{
		return entries;
	}

	void Clear(){
		std::lock_guard<std::mutex> lock(mutex);
		entries.clear();
	}

private:
	std::vector<LogEntry> entries;
	mutable std::mutex mutex;
};

class ImGuiLogWindow {
public:
	ImGuiLogWindow();
	void Draw();  // ImGui::Begin〜Endの中で呼ぶ
	void SetLogSource(std::shared_ptr<MemoryLogSink> sink);

private:
	std::shared_ptr<MemoryLogSink> logSink;

	char searchBuffer[128] = "";
	bool autoScroll = true;
	std::unordered_set<LogLevel> levelFilter;

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
