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
// エンジンヘッダに依存しないため、Linux上でも
//   g++ -std=c++20 -fsyntax-only MainThreadDispatcher.cpp -I Source/GameApplication -I Source/GameApplication/Backends/llama/vendor
// で構文チェック可能。
//
// =======================================================================
#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "../Core/Json.h"

namespace agentos {

// ---------------------------------
// MainThreadDispatcher
// スレッドセーフ。RunOnMainThreadは任意スレッドから、Pumpは必ずMain Threadから呼ぶ。
// ---------------------------------
class MainThreadDispatcher {
public:
	// closureをMain Threadのキューへ積み、Pump()によって実行されるまで
	// 呼び出しスレッドをブロックする。5秒以内にPumpされなければ
	// {"error":"main thread timeout","infrastructure":true} を返す。
	Json RunOnMainThread(std::function<Json()> fn);

	// Main Threadから毎フレーム呼び出す。キュー中のジョブをすべて実行し、
	// 待機中のRunOnMainThread呼び出し側を起床させる。
	void Pump();

private:
	struct PendingJob {
		std::function<Json()> fn;
		Json result;
		bool done = false;
	};

	std::mutex m_mutex;
	std::condition_variable m_cv;
	std::vector<std::shared_ptr<PendingJob>> m_queue;
};

} // namespace agentos
