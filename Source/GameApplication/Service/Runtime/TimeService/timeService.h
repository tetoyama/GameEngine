// =======================================================================
// 
// timeService.h
// 
// =======================================================================
#pragma once
#include "Service/IService.h"
#include "buildSetting.h"
// フレームタイミング・デルタ時間・FPS計測を管理するサービス
class TimeService : public IService
{
public:
	// 高精度タイマを初期化し、計測基準時刻を記録する
	void Initialize();
	void Shutdown()override {}

	void Tick();                   // 毎フレーム更新
	bool ShouldRunFixedUpdate();   // 固定更新を実行すべきか判定する

	// 現在の時間情報を取得する
	float GetDeltaTime() const;
	float GetTotalTime() const;
	float GetFixedDeltaTime() const;

	// 各処理区間の計測を終了して所要時間/FPSを更新する
	void BeginDeltaUpdate();
	void EndDeltaUpdate();
	void EndFixedUpdate();
	void BeginDraw();
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

	double GetFrameFPS() const{
		return frameFPS_;
	} // 前回のフレームFPS

private:
	double frequency_;             // カウンタの1秒あたりのカウント数
	long long startTime_;         // 開始時刻
	long long prevTime_;          // 前フレームの時刻
	long long updateBeginTime_ = 0;
	long long drawBeginTime_ = 0;
	long long frameBeginTime_ = 0;

	float deltaTime_;             // 前回からの経過時間（秒）
	float totalTime_;             // 起動からの累積時間（秒）

	double deltaUpdateTime_ = 0.0f;	// 前回の更新時間（秒）
	double fixedUpdateTime_ = 0.0f;	// 前回の固定更新時間（秒）
	double drawTime_ = 0.0f;			// 前回の描画時間（秒）


	long long prevDeltaTime_ = 0;          // 前フレームの時刻
	long long prevFixedTime_ = 0;          // 前フレームの時刻
	long long prevDrawTime_ = 0;          // 前フレームの時刻
	double frameTime_ = 0.0;

	double deltaUpdateTimer_ = 0.0f;	// 前回の更新時間（秒）
	double fixedUpdateTimer_ = 0.0f;	// 前回の固定更新時間（秒）
	double drawTimer_ = 0.0f;			// 前回の描画時間（秒）
	double frameTimer_ = 0.0;

	int fixedUpdateFrameCount_ = 0;			// 固定更新フレームカウント
	int deltaUpdateFrameCount_ = 0;		// 更新フレームカウント
	int drawFrameCount_ = 0;			// 描画フレームカウント
	int frameCount_ = 0;

	double fixedUpdateFPS_ = 0;			// 固定更新FPS
	double deltaUpdateFPS_ = 0;		// 更新FPS
	double drawFPS_ = 0;			// 描画FPS
	double frameFPS_ = 0.0;

	// 固定更新用
	float fixedDeltaTime_ = 1.0f / (float)TARGET_FPS;
	float fixedTimeAccumulator_ = 0.0f;
};
