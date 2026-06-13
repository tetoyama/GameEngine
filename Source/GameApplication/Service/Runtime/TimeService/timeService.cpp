// =======================================================================
//
// timeService.cpp
//
// =======================================================================

#include "timeService.h"
#include <Windows.h>

void TimeService::Initialize(){
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	frequency_ = static_cast<double>(freq.QuadPart);

	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	startTime_ = now.QuadPart;
	prevTime_ = now.QuadPart;

	prevDeltaTime_ = now.QuadPart;
	prevFixedTime_ = now.QuadPart;
	prevDrawTime_ = now.QuadPart;

	updateBeginTime_ = now.QuadPart;
	drawBeginTime_ = now.QuadPart;

	deltaTime_ = 0.0f;
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
}

void TimeService::Tick(){
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	long long current = now.QuadPart;

	deltaTime_ =
		static_cast<float>(
			static_cast<double>(current - prevTime_) /
			frequency_);

	totalTime_ =
		static_cast<float>(
			static_cast<double>(current - startTime_) /
			frequency_);

	prevTime_ = current;

	updateBeginTime_ = current;

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

void TimeService::EndDeltaUpdate(){
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	long long current = now.QuadPart;

	// Updateフェーズのみ
	deltaUpdateTime_ =
		static_cast<double>(
			current - updateBeginTime_) /
		frequency_;

	drawBeginTime_ = current;

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
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	long long current = now.QuadPart;

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

void TimeService::EndDraw(){
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	long long current = now.QuadPart;

	// Drawフェーズのみ
	drawTime_ =
		static_cast<double>(
			current - drawBeginTime_) /
		frequency_;

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