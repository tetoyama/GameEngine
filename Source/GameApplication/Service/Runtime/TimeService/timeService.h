// =======================================================================
// 
// timeService.h
// 
// =======================================================================
#pragma once
#include "Service/IService.h"
#include "buildSetting.h"
#include "gameApplication.h"
// フレームタイミング・デルタ時間・FPS計測を管理するサービス
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

	void EndDeltaUpdate();
	void EndFixedUpdate();
	void EndDraw();

	double GetDeltaUpdateTime() const{
		return deltaUpdateTime_;
	} // 前回からの経過時間（秒）

	double GetFixedUpdateTime() const{
		return fixedUpdateTime_;
	} // 前回の固定更新時間（秒）

	double GetDrawTime() const{
		return drawTime_;
	} // 前回の描画時間（秒）

	double GetDeltaFPS() const{
		return deltaUpdateFPS_;
	} // 前回の更新FPS

	double GetFixedUpdateFPS() const{
		return fixedUpdateFPS_;
	} // 前回の固定更新FPS

	double GetDrawFPS() const{
		return drawFPS_;
	} // 前回の描画FPS

private:
	double frequency_;             // カウンタの1秒あたりのカウント数
	long long startTime_;         // 開始時刻
	long long prevTime_;          // 前フレームの時刻
	float deltaTime_;             // 前回からの経過時間（秒）
	float totalTime_;             // 起動からの累積時間（秒）

	double deltaUpdateTime_ = 0.0f;	// 前回の更新時間（秒）
	double fixedUpdateTime_ = 0.0f;	// 前回の固定更新時間（秒）
	double drawTime_ = 0.0f;			// 前回の描画時間（秒）


	long long prevDeltaTime_ = 0;          // 前フレームの時刻
	long long prevFixedTime_ = 0;          // 前フレームの時刻
	long long prevDrawTime_ = 0;          // 前フレームの時刻

	double deltaUpdateTimer_ = 0.0f;	// 前回の更新時間（秒）
	double fixedUpdateTimer_ = 0.0f;	// 前回の固定更新時間（秒）
	double drawTimer_ = 0.0f;			// 前回の描画時間（秒）

	int fixedUpdateFrameCount_ = 0;			// 固定更新フレームカウント
	int deltaUpdateFrameCount_ = 0;		// 更新フレームカウント
	int drawFrameCount_ = 0;			// 描画フレームカウント

	double fixedUpdateFPS_ = 0;			// 固定更新FPS
	double deltaUpdateFPS_ = 0;		// 更新FPS
	double drawFPS_ = 0;			// 描画FPS

	// 固定更新用
	float fixedDeltaTime_ = 1.0f / (float)TARGET_FPS;
	float fixedTimeAccumulator_ = 0.0f;
};
