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
	double m_Frequency;             // カウンタの1秒あたりのカウント数
	long long m_StartTime;         // 開始時刻
	long long m_PrevTime;          // 前フレームの時刻
	float m_DeltaTime;             // 前回からの経過時間（秒）
	float m_TotalTime;             // 起動からの累積時間（秒）

	double m_DeltaUpdateTime= 0.0f;	// 前回の更新時間（秒）
	double m_FixedUpdateTime= 0.0f;	// 前回の固定更新時間（秒）
	double m_DrawTime= 0.0f;			// 前回の描画時間（秒）


	long long m_PrevDeltaTime= 0;          // 前フレームの時刻
	long long m_PrevFixedTime= 0;          // 前フレームの時刻
	long long m_PrevDrawTime= 0;          // 前フレームの時刻

	double m_DeltaUpdateTimer= 0.0f;	// 前回の更新時間（秒）
	double m_FixedUpdateTimer= 0.0f;	// 前回の固定更新時間（秒）
	double m_DrawTimer= 0.0f;			// 前回の描画時間（秒）

	int m_FixedUpdateFrameCount= 0;			// 固定更新フレームカウント
	int m_DeltaUpdateFrameCount= 0;		// 更新フレームカウント
	int m_DrawFrameCount= 0;			// 描画フレームカウント

	double m_FixedUpdateFps= 0;			// 固定更新FPS
	double m_DeltaUpdateFps= 0;		// 更新FPS
	double m_DrawFps= 0;			// 描画FPS

	// 固定更新用
	float m_FixedDeltaTime= 1.0f / (float)TARGET_FPS;
	float m_FixedTimeAccumulator= 0.0f;
};
