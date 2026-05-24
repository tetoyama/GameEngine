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
#include <ImGui/imgui_internal.h>

void DebugLog(std::string Message){
	OutputDebugStringA(Message.c_str());
}

void DebugLogService::Initialize(){

	// 必要であればファイルログなどの初期化
	auto m_MemorySink= std::make_shared<MemoryLogSink>();
	AddSink(m_MemorySink);
}

void DebugLogService::Shutdown(){
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_Sinks.clear();
}
void DebugLogService::Draw(){

}

void DebugLogService::AddSink(std::shared_ptr<ILogSink> sink){
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_Sinks.push_back(sink);
}

void DebugLogService::RemoveSink(std::shared_ptr<ILogSink> sink){
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_Sinks.erase(std::remove(m_Sinks.begin(), m_Sinks.end(), sink), m_Sinks.end());
}

void DebugLogService::Log(LogLevel level,
						 const std::string& message,
						 const std::string& function,
						 const std::string& file,
						 int line){
	
	LogEntry m_Entry;
	m_Entry.level = level;
	m_Entry.message = message;
	m_Entry.function = function;
	m_Entry.file = file;
	m_Entry.line = line;
	m_Entry.timestamp = std::chrono::system_clock::now();
	OutputDebugStringW((Utf8ToWide(message) +L"\n").c_str());

	std::lock_guard<std::mutex> lock(m_Mutex);
	for(const auto& sink : m_Sinks){
		sink->Write(m_Entry);
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

