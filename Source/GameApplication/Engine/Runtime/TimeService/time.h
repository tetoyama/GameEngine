// Runtime/TimeService.h
#pragma once
#include "Service/IService.h"

class TimeService : public IService
{
public:
	void Initialize();
	void Shutdown()override {}

	void Tick();                    // 毎フレーム更新
	bool ShouldRunFixedUpdate();   // 固定更新すべきか

	float GetDeltaTime() const;
	float GetTotalTime() const;
	float GetFixedDeltaTime() const;

private:
	double frequency_;             // カウンタの1秒あたりのカウント数
	long long startTime_;         // 開始時刻
	long long prevTime_;          // 前フレームの時刻
	float deltaTime_;             // 前回からの経過時間（秒）
	float totalTime_;             // 起動からの累積時間（秒）

	// 固定更新用
	float fixedDeltaTime_ = 1.0f / 60.0f;
	float fixedTimeAccumulator_ = 0.0f;
};
