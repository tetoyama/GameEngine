// DebugSystem.cpp
#include "DebugSystem.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <unordered_set>

void DebugLog(std::string Message){
	OutputDebugStringA(Message.c_str());
}

void DebugLogSystem::Initialize(bool* isOpen){

	// 必要であればファイルログなどの初期化
	auto memorySink = std::make_shared<MemoryLogSink>();
	AddSink(memorySink);
	logWindow = std::make_shared<ImGuiLogWindow>();
	logWindow->SetLogSource(memorySink);
	logWindow->SetOpenControl(isOpen);
}

void DebugLogSystem::Shutdown(){
	std::lock_guard<std::mutex> lock(mutex);
	sinks.clear();
}
void DebugLogSystem::Draw(){
	logWindow->Draw();

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

ImGuiLogWindow::ImGuiLogWindow(){

	for(int i = (int)LogLevel::Trace; i <= (int)LogLevel::Critical; ++i){
		levelFilter.insert(static_cast<LogLevel>(i));
	}
}

void ImGuiLogWindow::SetLogSource(std::shared_ptr<MemoryLogSink> sink){
	logSink = sink;
}

bool ImGuiLogWindow::PassesFilter(const LogEntry& entry) const{
	if(levelFilter.find(entry.level) == levelFilter.end()) return false;

	if(strlen(searchBuffer) > 0){
		if(entry.message.find(searchBuffer) == std::string::npos &&
		   entry.function.find(searchBuffer) == std::string::npos)
			return false;
	}
	return true;
}

const char* ImGuiLogWindow::LevelToString(LogLevel level) const{
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

std::string ImGuiLogWindow::LevelFilterString(LogLevel level) const{

	int Count = 0;

	const auto& entries = logSink->GetEntries();
	for(const auto& entry : entries){
		if(entry.level != level) continue;
		Count++;
	}

	std::string a = LevelToString(level);
	a = a + "(" + std::to_string(Count) +  ")";
	return a;
}

ImVec4 ImGuiLogWindow::GetColorForLevel(LogLevel level) const{
	switch(level){
		case LogLevel::Trace:    return ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
		case LogLevel::Debug:    return ImVec4(0.8f, 0.8f, 1.0f, 1.0f);
		case LogLevel::Info:     return ImVec4(0.6f, 1.0f, 0.6f, 1.0f);
		case LogLevel::Warning:  return ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
		case LogLevel::Error:    return ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
		case LogLevel::Critical: return ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
		default: return ImVec4(1, 1, 1, 1);
	}
}

void ImGuiLogWindow::Draw(){
	if(isOpen && *isOpen){

		if(ImGui::Begin("Debug Log", isOpen)){

			ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer));
			ImGui::SameLine();
			if(ImGui::Button("Clear") && logSink){
				logSink->Clear();
			}
			ImGui::SameLine();
			ImGui::Checkbox("Auto Scroll", &autoScroll);

			ImGui::Separator();

			for(int i = (int)LogLevel::Trace; i <= (int)LogLevel::Critical; ++i){
				LogLevel level = static_cast<LogLevel>(i);
				bool selected = levelFilter.find(level) != levelFilter.end();
				if(ImGui::Checkbox(LevelFilterString(level).c_str(), &selected)){
					if(selected)
						levelFilter.insert(level);
					else
						levelFilter.erase(level);
				}
				if(i < (int)LogLevel::Critical) ImGui::SameLine();
			}

			if(ImGui::BeginChild("LogRegion", ImVec2(0, 0), false)){

				if(logSink){
					const auto& entries = logSink->GetEntries();
					for(const auto& entry : entries){
						if(!PassesFilter(entry)) continue;

						ImVec4 color = GetColorForLevel(entry.level);
						ImGui::PushStyleColor(ImGuiCol_Text, color);
						ImGui::Text(ToU8String((const char*)u8"[%s] %s\n       (関数名 %s,ファイル %s ,行 %d)").c_str(),
									LevelToString(entry.level),
									entry.message.c_str(),
									entry.function.c_str(),
									entry.file.c_str(),
									entry.line);
						ImGui::PopStyleColor();
					}

					if(autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f)
						ImGui::SetScrollHereY(1.0f);
				}


				ImGui::EndChild();
			}

		}
		ImGui::End();
	}
}
