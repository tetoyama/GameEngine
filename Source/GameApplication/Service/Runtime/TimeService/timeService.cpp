// =======================================================================
//
// timeService.cpp
//
// =======================================================================

#include "timeService.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include <chrono>
#endif

namespace {
long long ReadCounter() {
#ifdef _WIN32
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
#else
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}
}

void TimeService::Initialize(){
#ifdef _WIN32
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    frequency_ = static_cast<double>(freq.QuadPart);
#else
    frequency_ = 1'000'000'000.0;
#endif

    const auto now = ReadCounter();

	startTime_ = now;
	prevTime_ = now;

	prevDeltaTime_ = now;
	prevFixedTime_ = now;
	prevDrawTime_ = now;

	updateBeginTime_ = now;
	drawBeginTime_ = now;
	drawSectionBeginTime_ = now;
	frameBeginTime_ = now;

	deltaTime_ = 0.0f;
	fixedTimeAccumulator_ = 0.0f;
	frameTime_ = frameFPS_ = frameTimer_ = 0.0;
	frameCount_ = 0;
	totalTime_ = 0.0f;

	deltaUpdateTime_ = 0.0;
	fixedUpdateTime_ = 0.0;
	drawTime_ = 0.0;

	deltaUpdateTimer_ = 0.0;
	fixedUpdateTimer_ = 0.0;
	drawTimer_ = 0.0;

	deltaUpdateFrameCount_ = 0;
	fixedUpdateFrameCount_ = 0;
	drawFrameCount_ = 0;

	deltaUpdateFPS_ = 0.0;
	fixedUpdateFPS_ = 0.0;
	drawFPS_ = 0.0;

	drawSectionActive_ = false;
	currentDrawTiming_ = {};
	completedDrawTiming_ = {};
}

void TimeService::Tick(){
    const auto now = ReadCounter();

	long long current = now;

	frameTime_ =
		static_cast<double>(current - frameBeginTime_) / frequency_;

	if(frameTime_ > 0.0){
		frameFPS_ = 1.0 / frameTime_;
	}

	frameTimer_ += frameTime_;
	frameCount_++;

	if(frameTimer_ >= 1.0){
		frameFPS_ = static_cast<double>(frameCount_) / frameTimer_;
		frameTimer_ = 0.0;
		frameCount_ = 0;
	}
	frameBeginTime_ = now;

	deltaTime_ =
		static_cast<float>(
			static_cast<double>(current - prevTime_) /
			frequency_);

	totalTime_ =
		static_cast<float>(
			static_cast<double>(current - startTime_) /
			frequency_);

	prevTime_ = current;

	fixedTimeAccumulator_ += deltaTime_;
}

bool TimeService::ShouldRunFixedUpdate(){
	if(fixedTimeAccumulator_ >= fixedDeltaTime_){
		fixedTimeAccumulator_ -= fixedDeltaTime_;
		return true;
	}

	return false;
}

float TimeService::GetDeltaTime() const{
	return deltaTime_;
}

float TimeService::GetTotalTime() const{
	return totalTime_;
}

float TimeService::GetFixedDeltaTime() const{
	return fixedDeltaTime_;
}

void TimeService::BeginDeltaUpdate(){
    const auto now = ReadCounter();
	updateBeginTime_ = now;
}

void TimeService::EndDeltaUpdate(){
    const auto now = ReadCounter();

	long long current = now;

	// Updateフェーズのみ
	deltaUpdateTime_ =
		static_cast<double>(
			current - updateBeginTime_) /
		frequency_;

	deltaUpdateTimer_ += deltaUpdateTime_;
	deltaUpdateFrameCount_++;

	if(deltaUpdateTimer_ >= 1.0){
		deltaUpdateFPS_ =
			static_cast<double>(deltaUpdateFrameCount_) /
			deltaUpdateTimer_;

		deltaUpdateTimer_ = 0.0;
		deltaUpdateFrameCount_ = 0;
	}
}

void TimeService::EndFixedUpdate(){
    const auto now = ReadCounter();

	long long current = now;

	fixedUpdateTime_ =
		static_cast<double>(
			current - prevFixedTime_) /
		frequency_;

	prevFixedTime_ = current;

	fixedUpdateTimer_ += fixedUpdateTime_;
	fixedUpdateFrameCount_++;

	if(fixedUpdateTimer_ >= 1.0){
		fixedUpdateFPS_ =
			static_cast<double>(fixedUpdateFrameCount_) /
			fixedUpdateTimer_;

		fixedUpdateTimer_ = 0.0;
		fixedUpdateFrameCount_ = 0;
	}
}

void TimeService::BeginDraw(uint64_t frameSerial){
    const auto now = ReadCounter();
	drawBeginTime_ = now;
	drawSectionBeginTime_ = now;
	drawSectionActive_ = false;
	currentDrawTiming_ = {};
	currentDrawTiming_.frameSerial = frameSerial;
	currentDrawTiming_.update = deltaUpdateTime_;
}

void TimeService::BeginDrawSection(DrawTimingSection section){
    const auto now = ReadCounter();

	activeDrawSection_ = section;
	drawSectionBeginTime_ = now;
	drawSectionActive_ = true;
}

void TimeService::EndDrawSection(DrawTimingSection section){
	if(!drawSectionActive_ || activeDrawSection_ != section){
		return;
	}

    const auto now = ReadCounter();

	const double elapsedSeconds =
		static_cast<double>(now - drawSectionBeginTime_) /
		frequency_;
	AccumulateDrawSection(section, elapsedSeconds);
	drawSectionActive_ = false;
}

void TimeService::EndDraw(){
	if(drawSectionActive_){
		EndDrawSection(activeDrawSection_);
	}

    const auto now = ReadCounter();

	long long current = now;

	// BeginDrawからPresent完了までのDrawフェーズ全体
	drawTime_ =
		static_cast<double>(
			current - drawBeginTime_) /
		frequency_;

	currentDrawTiming_.total = drawTime_;
	completedDrawTiming_ = currentDrawTiming_;

	drawTimer_ += drawTime_;
	drawFrameCount_++;

	if(drawTimer_ >= 1.0){
		drawFPS_ =
			static_cast<double>(drawFrameCount_) /
			drawTimer_;

		drawTimer_ = 0.0;
		drawFrameCount_ = 0;
	}
}

void TimeService::AccumulateDrawSection(
	DrawTimingSection section,
	double elapsedSeconds
){
	switch(section){
		case DrawTimingSection::FramePacingWait:
			currentDrawTiming_.framePacingWait += elapsedSeconds;
			break;
		case DrawTimingSection::FrameSetup:
			currentDrawTiming_.frameSetup += elapsedSeconds;
			break;
		case DrawTimingSection::ImGuiBegin:
			currentDrawTiming_.imguiBegin += elapsedSeconds;
			break;
		case DrawTimingSection::RenderSchedule:
			currentDrawTiming_.renderSchedule += elapsedSeconds;
			break;
		case DrawTimingSection::DebugDraw:
			currentDrawTiming_.debugDraw += elapsedSeconds;
			break;
		case DrawTimingSection::EditorUIBuild:
			currentDrawTiming_.editorUIBuild += elapsedSeconds;
			break;
		case DrawTimingSection::ImGuiRender:
			currentDrawTiming_.imguiRender += elapsedSeconds;
			break;
		case DrawTimingSection::Present:
			currentDrawTiming_.present += elapsedSeconds;
			break;
	}
}
