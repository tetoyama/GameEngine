// =======================================================================
// 
// DebugSystem.h
// 
// =======================================================================
#pragma once
#include <string>
#include <chrono>
#include <cstdint>
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


// ログ出力先の基底インターフェース
class ILogSink {
public:
	virtual ~ILogSink() = default;
	virtual void Write(const LogEntry& entry) = 0;
};

// デバッグログの収集・配信を管理するサービス
class DebugLogService : public IService {
public:
	void Initialize(); // 必要ならファイルパスなど渡す
	void Shutdown() override;

	void AddSink(std::shared_ptr<ILogSink> sink);
	void RemoveSink(std::shared_ptr<ILogSink> sink);
	template<typename T>
	std::shared_ptr<T> GetSink(){
		std::lock_guard<std::mutex> lock(mutex);
		for(auto& sink : sinks){
			if(auto casted = std::dynamic_pointer_cast<T>(sink)){
				return casted;
			}
		}
		return nullptr;
	}

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

	std::wstring Utf8ToWide(const std::string& utf8){
		int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
		std::wstring wide(size, 0);
		MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], size);
		wide.pop_back(); // null終端除去
		return wide;
	}
};

// メモリ上にログエントリを蓄積するシンク実装
class MemoryLogSink: public ILogSink {
public:
	void Write(const LogEntry& entry) override{
		std::lock_guard<std::mutex> lock(mutex);
		entries.push_back(entry);
		++revision;
	}

	// Legacy read-only view. New UI code should use GetSnapshot() for thread safety.
	const std::vector<LogEntry>& GetEntries() const{
		return entries;
	}

	std::vector<LogEntry> GetSnapshot() const{
		std::lock_guard<std::mutex> lock(mutex);
		return entries;
	}

	uint64_t GetRevision() const{
		std::lock_guard<std::mutex> lock(mutex);
		return revision;
	}

	void Clear(){
		std::lock_guard<std::mutex> lock(mutex);
		entries.clear();
		++revision;
	}

private:
	std::vector<LogEntry> entries;
	uint64_t revision = 0;
	mutable std::mutex mutex;
};

