// =======================================================================
// 
// timeService.h
// 
// =======================================================================
#pragma once
#include "Service/IService.h"
#include "buildSetting.h"

// Drawフェーズ内でCPU計測する区間。
// GPU実行時間は含めず、Timestamp Queryによる計測と明確に分離する。
enum class DrawTimingSection : unsigned char {
	FramePacingWait,
	FrameSetup,
	ImGuiBegin,
	RenderSchedule,
	DebugDraw,
	EditorUIBuild,
	ImGuiRender,
	Present
};

// 直前に完了したDrawフレームのCPU時間内訳。
// 値の単位はTimeServiceの他の時間情報と同じ秒。
struct DrawTimingBreakdown {
	double framePacingWait = 0.0;
	double frameSetup = 0.0;
	double imguiBegin = 0.0;
	double renderSchedule = 0.0;
	double debugDraw = 0.0;
	double editorUIBuild = 0.0;
	double imguiRender = 0.0;
	double present = 0.0;
	double total = 0.0;

	double GetAccountedTime() const {
		return framePacingWait +
			frameSetup +
			imguiBegin +
			renderSchedule +
			debugDraw +
			editorUIBuild +
			imguiRender +
			present;
	}

	double GetUnaccountedTime() const {
		const double unaccounted = total - GetAccountedTime();
		return unaccounted > 0.0 ? unaccounted : 0.0;
	}
};

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

	// Drawフェーズ内の非ネスト区間を計測する。
	// BeginDrawSection / EndDrawSectionは同じsectionを対にして呼ぶ。
	void BeginDrawSection(DrawTimingSection section);
	void EndDrawSection(DrawTimingSection section);

	double GetDeltaUpdateTime() const{
		return deltaUpdateTime_;
	} // 前回からの経過時間（秒）

	double GetFixedUpdateTime() const{
		return fixedUpdateTime_;
	} // 前回の固定更新時間（秒）

	double GetDrawTime() const{
		return drawTime_;
	} // 前回の描画時間（秒）

	const DrawTimingBreakdown& GetDrawTimingBreakdown() const {
		return completedDrawTiming_;
	}

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
	void AccumulateDrawSection(DrawTimingSection section, double elapsedSeconds);

	double frequency_;             // カウンタの1秒あたりのカウント数
	long long startTime_;         // 開始時刻
	long long prevTime_;          // 前フレームの時刻
	long long updateBeginTime_ = 0;
	long long drawBeginTime_ = 0;
	long long drawSectionBeginTime_ = 0;
	long long frameBeginTime_ = 0;

	float deltaTime_;             // 前回からの経過時間（秒）
	float totalTime_;             // 起動からの累積時間（秒）

	double deltaUpdateTime_ = 0.0f;	// 前回の更新時間（秒）
	double fixedUpdateTime_ = 0.0f;	// 前回の固定更新時間（秒）
	double drawTime_ = 0.0f;			// 前回の描画時間（秒）

	DrawTimingSection activeDrawSection_ = DrawTimingSection::FrameSetup;
	bool drawSectionActive_ = false;
	DrawTimingBreakdown currentDrawTiming_{};
	DrawTimingBreakdown completedDrawTiming_{};

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
