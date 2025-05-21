// Runtime/TimeService.cpp
#include "time.h"
#include <Windows.h>

void TimeService::Initialize(){
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	frequency_ = static_cast<double>(freq.QuadPart);

	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	startTime_ = prevTime_ = now.QuadPart;

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
