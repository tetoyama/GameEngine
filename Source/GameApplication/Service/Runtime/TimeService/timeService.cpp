// =======================================================================
// 
// timeService.cpp
// 
// =======================================================================
#include "timeService.h"
#include <Windows.h>

void TimeService::Initialize(){
	LARGE_INTEGER m_Freq;
	QueryPerformanceFrequency(&freq);
	frequency_ = static_cast<double>(freq.QuadPart);

	LARGE_INTEGER m_Now;
	QueryPerformanceCounter(&now);
	startTime_ = prevTime_ = prevDeltaTime_ = prevFixedTime_ = prevDrawTime_ = now.QuadPart;

	deltaTime_ = 0.0f;
	totalTime_ = 0.0f;
}

void TimeService::Tick(){
	LARGE_INTEGER m_Now;
	QueryPerformanceCounter(&now);

	long long m_Current= now.QuadPart;
	deltaTime_ = static_cast<float>(current - prevTime_) / static_cast<float>(frequency_);
	totalTime_ = static_cast<float>(current - startTime_) / static_cast<float>(frequency_);
	prevTime_ = current;

	fixedTimeAccumulator_ += deltaTime_;
}

bool TimeService::ShouldRunFixedUpdate(){
	if(fixedTimeAccumulator_ >= fixedDeltaTime_){
		fixedTimeAccumulator_ -= fixedDeltaTime_;
		return m_True;
	}
	return m_False;
}

float TimeService::GetDeltaTime() const{
	return m_DeltaTime;
}
float TimeService::GetTotalTime() const{
	return m_TotalTime;
}
float TimeService::GetFixedDeltaTime() const{
	return m_FixedDeltaTime;
}

void TimeService::EndDeltaUpdate(){

	LARGE_INTEGER m_Now;
	QueryPerformanceCounter(&now);

	long long m_Current= now.QuadPart;
	
	deltaUpdateTime_ = static_cast<double>(current - prevDeltaTime_) / static_cast<double>(frequency_);
	prevDeltaTime_ = current;

	deltaUpdateTimer_ += deltaUpdateTime_;
	deltaUpdateFrameCount_++;

	deltaUpdateTime_ = static_cast<double>(current - prevTime_) / static_cast<double>(frequency_);

	if(1.0f <= deltaUpdateTimer_){

		deltaUpdateFPS_ = static_cast<double>(deltaUpdateFrameCount_) / deltaUpdateTimer_;

		deltaUpdateTimer_ = 0.0f;
		deltaUpdateFrameCount_ = 0;
	}
}

void TimeService::EndFixedUpdate(){
	LARGE_INTEGER m_Now;
	QueryPerformanceCounter(&now);

	long long m_Current= now.QuadPart;

	fixedUpdateTime_ = static_cast<double>(current - prevFixedTime_) / static_cast<double>(frequency_);
	prevFixedTime_ = current;

	fixedUpdateTimer_ += fixedUpdateTime_;
	fixedUpdateFrameCount_++;

	// フレーム内での固定更新フェーズの所要時間（秒）
	fixedUpdateTime_ = static_cast<double>(current - prevTime_) / static_cast<double>(frequency_) - deltaUpdateTime_;

	if(1.0f <= fixedUpdateTimer_){
		fixedUpdateFPS_ = static_cast<double>(fixedUpdateFrameCount_) / fixedUpdateTimer_;
		fixedUpdateTimer_ = 0.0f;
		fixedUpdateFrameCount_ = 0;
	}
}

void TimeService::EndDraw(){
	LARGE_INTEGER m_Now;
	QueryPerformanceCounter(&now);

	long long m_Current= now.QuadPart;

	drawTime_ = static_cast<double>(current - prevDrawTime_) / static_cast<double>(frequency_);
	prevDrawTime_ = current;

	drawTimer_ += drawTime_;
	drawFrameCount_++;

	// フレーム内での描画フェーズの所要時間（秒）
	drawTime_ = static_cast<double>(current - prevTime_) / static_cast<double>(frequency_) - deltaUpdateTime_;

	if(1.0f <= drawTimer_){
		drawFPS_ = static_cast<double>(drawFrameCount_) / drawTimer_;
		drawTimer_ = 0.0f;
		drawFrameCount_ = 0;
	}
}
