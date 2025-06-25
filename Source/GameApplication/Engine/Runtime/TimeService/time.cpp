// Runtime/TimeService.cpp
#include "time.h"
#include <Windows.h>

void TimeService::Initialize(){
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	frequency_ = static_cast<double>(freq.QuadPart);

	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	startTime_ = prevTime_ = prevDeltaTime_ = prevFixedTime_ = prevDrawTime_ = now.QuadPart;

	deltaTime_ = 0.0f;
	totalTime_ = 0.0f;
}

void TimeService::Tick(){
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	long long current = now.QuadPart;
	deltaTime_ = static_cast<float>(current - prevTime_) / static_cast<float>(frequency_);
	totalTime_ = static_cast<float>(current - startTime_) / static_cast<float>(frequency_);
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

void TimeService::EndDeltaUpdate(){

	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	long long current = now.QuadPart;
	
	deltaUpdateTime_ = static_cast<double>(current - prevDeltaTime_) / static_cast<double>(frequency_);
	prevDeltaTime_ = current;

	deltaUpdateTimer_ += deltaUpdateTime_;
	daltaUpdateFrameCount_++;

	deltaUpdateTime_ = static_cast<double>(current - prevTime_) / static_cast<double>(frequency_);

	if(1.0f <= deltaUpdateTimer_){

		daltaUpdateFPS_ = static_cast<double>(daltaUpdateFrameCount_) / deltaUpdateTimer_;

		deltaUpdateTimer_ = 0.0f;
		daltaUpdateFrameCount_ = 0;
	}
}

void TimeService::EndFixedUpdate(){
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	long long current = now.QuadPart;

	fixedUpdateTime_ = static_cast<double>(current - prevFixedTime_) / static_cast<double>(frequency_);
	prevFixedTime_ = current;

	fixedUpdateTimer_ += fixedUpdateTime_;
	fixedUpdateFrameCount_++;

	fixedUpdateTime_ = static_cast<double>(current - prevTime_ - deltaUpdateTime_) / static_cast<double>(frequency_);

	if(1.0f <= fixedUpdateTimer_){
		fixedUpdateFPS_ = static_cast<double>(fixedUpdateFrameCount_) / fixedUpdateTimer_;
		fixedUpdateTimer_ = 0.0f;
		fixedUpdateFrameCount_ = 0;
	}
}

void TimeService::EndDraw(){
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	long long current = now.QuadPart;

	drawTime_ = static_cast<double>(current - prevDrawTime_) / static_cast<double>(frequency_);
	prevDrawTime_ = current;

	drawTimer_ += drawTime_;
	drawFrameCount_++;

	drawTime_ = static_cast<double>(current - prevTime_ - deltaUpdateTime_) / static_cast<double>(frequency_);

	if(1.0f <= drawTimer_){
		drawFPS_ = static_cast<double>(drawFrameCount_) / drawTimer_;
		drawTimer_ = 0.0f;
		drawFrameCount_ = 0;
	}
}
