// =======================================================================
// 
// DebugSystem.cpp
// 
// =======================================================================
#include "DebugSystem.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#ifdef _WIN32
#include <Windows.h>
#endif

void DebugLog(std::string Message){
#ifdef _WIN32
	OutputDebugStringA(Message.c_str());
#else
	std::clog << Message;
#endif
}

void DebugLogService::Initialize(){

	// 必要であればファイルログなどの初期化
	auto memorySink = std::make_shared<MemoryLogSink>();
	AddSink(memorySink);
}

void DebugLogService::Shutdown(){
	std::lock_guard<std::mutex> lock(mutex);
	sinks.clear();
}
void DebugLogService::Draw(){

}

void DebugLogService::AddSink(std::shared_ptr<ILogSink> sink){
	std::lock_guard<std::mutex> lock(mutex);
	sinks.push_back(sink);
}

void DebugLogService::RemoveSink(std::shared_ptr<ILogSink> sink){
	std::lock_guard<std::mutex> lock(mutex);
	sinks.erase(std::remove(sinks.begin(), sinks.end(), sink), sinks.end());
}

void DebugLogService::Log(LogLevel level,
						 const std::string& message,
						 const std::string& function,
						 const std::string& file,
						 int line){
	
	LogEntry entry;
	entry.level = level;
	entry.message = message;
	entry.function = function;
	entry.file = file;
	entry.line = line;
	entry.timestamp = std::chrono::system_clock::now();
#ifdef _WIN32
	const int size=MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, message.c_str(), -1, nullptr, 0);
	if(size>0) {
		std::wstring wide(static_cast<size_t>(size), L'\0');
		if(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, message.c_str(), -1, wide.data(), size)>0) {
			wide.back()=L'\n'; OutputDebugStringW(wide.c_str());
		}
	} else OutputDebugStringA((message+"\n").c_str());
#else
	std::clog << message << '\n';
#endif

	std::lock_guard<std::mutex> lock(mutex);
	for(const auto& sink : sinks){
		sink->Write(entry);
	}
}

void DebugLogService::Log(LogLevel level, const char8_t* message, const std::string& function, const std::string& file, int line) {
	std::string str = reinterpret_cast<const char*>(message);
	Log(level, str, function, file, line);
}

void DebugLogService::Info(const std::string& message, const std::string& source){
	Log(LogLevel::Info, message, source, source, 0);
}

void DebugLogService::Error(const std::string& message, const std::string& source){
	Log(LogLevel::Error, message, source, source, 0);
}

