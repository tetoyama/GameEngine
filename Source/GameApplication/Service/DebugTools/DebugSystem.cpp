// DebugSystem.cpp
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

void DebugLogSystem::Initialize(){

	// 必要であればファイルログなどの初期化
	auto memorySink = std::make_shared<MemoryLogSink>();
	AddSink(memorySink);
	//if(isOpen){
	//	logWindow = std::make_shared<ImGuiLogWindow>();
	//logWindow->SetLogSource(memorySink);
	//logWindow->SetOpenControl(isOpen);
	//}
}

void DebugLogSystem::Shutdown(){
	std::lock_guard<std::mutex> lock(mutex);
	sinks.clear();
}
void DebugLogSystem::Draw(){

}

void DebugLogSystem::AddSink(std::shared_ptr<ILogSink> sink){
	std::lock_guard<std::mutex> lock(mutex);
	sinks.push_back(sink);
}

void DebugLogSystem::RemoveSink(std::shared_ptr<ILogSink> sink){
	std::lock_guard<std::mutex> lock(mutex);
	sinks.erase(std::remove(sinks.begin(), sinks.end(), sink), sinks.end());
}

void DebugLogSystem::Log(LogLevel level,
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
	OutputDebugStringW((Utf8ToWide(message) +L"\n").c_str());

	std::lock_guard<std::mutex> lock(mutex);
	for(const auto& sink : sinks){
		sink->Write(entry);
	}
}

void DebugLogSystem::Log(LogLevel level, const char8_t* message, const std::string& function, const std::string& file, int line) {
	std::string str = reinterpret_cast<const char*>(message);
	Log(level, str, function, file, line);
}

void DebugLogSystem::Info(const std::string& message, const std::string& source){
	Log(LogLevel::Info, message, source, source, 0);
}

void DebugLogSystem::Error(const std::string& message, const std::string& source){
	Log(LogLevel::Error, message, source, source, 0);
}

