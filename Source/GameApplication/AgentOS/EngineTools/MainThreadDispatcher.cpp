// =======================================================================
//
// MainThreadDispatcher.cpp
//
// =======================================================================
#include "MainThreadDispatcher.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <string>

namespace agentos {

namespace {

Json MakeInfrastructureError(const std::string& message) {
	Json error = Json::object();
	error["error"] = message;
	error["infrastructure"] = true;
	return error;
}

Json ExecuteSafely(const std::function<Json()>& fn) {
	if(!fn){
		return MakeInfrastructureError("main thread job is empty");
	}
	try{
		return fn();
	} catch(const std::exception& e){
		return MakeInfrastructureError(std::string("main thread exception: ") + e.what());
	} catch(...){
		return MakeInfrastructureError("main thread unknown exception");
	}
}

} // namespace

Json MainThreadDispatcher::RunOnMainThread(std::function<Json()> fn) {
	// Main Threadから呼ばれた場合はQueue待ちによる自己デッドロックを避けて直接実行する。
	bool executeInline = false;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		executeInline = m_mainThreadId != std::thread::id{} &&
			m_mainThreadId == std::this_thread::get_id();
	}
	if(executeInline) return ExecuteSafely(fn);

	auto job = std::make_shared<PendingJob>();
	job->fn = std::move(fn);

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_queue.push_back(job);
	}

	std::unique_lock<std::mutex> lock(m_mutex);
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

	// timeoutは「まだ実行開始されていないQueued状態」にだけ適用する。
	const bool started = m_cv.wait_until(
		lock,
		deadline,
		[&job](){ return job->state != JobState::Queued; }
	);

	if(!started && job->state == JobState::Queued){
		job->state = JobState::Cancelled;
		m_queue.erase(
			std::remove(m_queue.begin(), m_queue.end(), job),
			m_queue.end()
		);
		return MakeInfrastructureError("main thread timeout before execution");
	}

	if(job->state == JobState::Cancelled){
		return MakeInfrastructureError("main thread job cancelled");
	}

	// Runningへ遷移した後は完了まで待つ。ここでtimeoutを返すと、呼び出し側が
	// 失敗扱いした後にModify Toolが実行完了する危険があるため。
	m_cv.wait(lock, [&job](){
		return job->state == JobState::Completed || job->state == JobState::Cancelled;
	});

	if(job->state == JobState::Cancelled){
		return MakeInfrastructureError("main thread job cancelled");
	}
	return job->result;
}

void MainThreadDispatcher::Pump() {
	std::vector<std::shared_ptr<PendingJob>> jobs;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_mainThreadId = std::this_thread::get_id();
		if(m_queue.empty()) return;

		jobs.swap(m_queue);
		for(auto& job : jobs){
			if(job && job->state == JobState::Queued){
				job->state = JobState::Running;
			}
		}
	}
	m_cv.notify_all();

	for(auto& job : jobs){
		if(!job || job->state != JobState::Running) continue;
		Json result = ExecuteSafely(job->fn);
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			job->result = std::move(result);
			job->state = JobState::Completed;
		}
		m_cv.notify_all();
	}
}

void MainThreadDispatcher::CancelPending() {
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		for(auto& job : m_queue){
			if(job && job->state == JobState::Queued){
				job->state = JobState::Cancelled;
			}
		}
		m_queue.clear();
	}
	m_cv.notify_all();
}

} // namespace agentos
