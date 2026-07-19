// =======================================================================
//
// MainThreadDispatcher.h
//
// Orchestratorのワーカースレッドからエンジン（Scene / SystemRegistry等）へ
// 安全にアクセスするためのMain Thread実行キュー。
//
// Structural-change制約（Schedule実行中の直接Component追加/削除はassert）を
// 回避するため、AgentOSのEngineToolはすべてMain Threadのエディタ描画タイミング
// （AgentOSPanel::Draw → AgentOSService::PumpMainThread → Pump()）で実行する。
//
// =======================================================================
#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "../Core/Json.h"

namespace agentos {

class MainThreadDispatcher {
public:
	// closureをMain Threadのキューへ積み、Pump()によって実行されるまで待つ。
	// 5秒以内に実行開始されなければQueueからキャンセルしてtimeoutを返す。
	// 実行開始後は完了まで待ち、"timeoutと報告した後で遅延実行される"状態を防ぐ。
	Json RunOnMainThread(std::function<Json()> fn);

	// Main Threadから毎フレーム呼び出す。
	void Pump();

	// Shutdown時に未実行Jobをキャンセルし、待機中Workerを起床させる。
	// 既にMain Thread上で実行開始済みのJobは完了まで待つ。
	void CancelPending();

private:
	enum class JobState {
		Queued,
		Running,
		Completed,
		Cancelled,
	};

	struct PendingJob {
		std::function<Json()> fn;
		Json result;
		JobState state = JobState::Queued;
	};

	std::mutex m_mutex;
	std::condition_variable m_cv;
	std::vector<std::shared_ptr<PendingJob>> m_queue;
	std::thread::id m_mainThreadId{};
};

} // namespace agentos
