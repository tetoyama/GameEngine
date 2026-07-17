// =======================================================================
//
// MainThreadDispatcher.cpp
//
// =======================================================================
#include "MainThreadDispatcher.h"

#include <chrono>

namespace agentos {

Json MainThreadDispatcher::RunOnMainThread(std::function<Json()> fn) {
	auto job = std::make_shared<PendingJob>();
	job->fn = std::move(fn);

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_queue.push_back(job);
	}

	std::unique_lock<std::mutex> lock(m_mutex);
	const bool completed = m_cv.wait_for(
		lock,
		std::chrono::seconds(5),
		[&job](){ return job->done; }
	);

	if(!completed){
		Json error = Json::object();
		error["error"] = "main thread timeout";
		error["infrastructure"] = true;
		return error;
	}

	return job->result;
}

void MainThreadDispatcher::Pump() {
	std::vector<std::shared_ptr<PendingJob>> jobs;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if(m_queue.empty()) return;
		jobs.swap(m_queue);
	}

	// ジョブ本体はロック外で実行する（fnがMain Thread上でエンジンへアクセスする間、
	// 他スレッドからのRunOnMainThreadのキュー追加をブロックしないため）。
	for(auto& job : jobs){
		job->result = job->fn ? job->fn() : Json::object();
	}

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		for(auto& job : jobs){
			job->done = true;
		}
	}
	m_cv.notify_all();
}

} // namespace agentos
