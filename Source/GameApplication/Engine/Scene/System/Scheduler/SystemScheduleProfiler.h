// =======================================================================
//
// SystemScheduleProfiler.h
//
// Scheduler Taskの実測開始時刻・終了時刻・実行WorkerをDomain単位で記録する。
// 実行中SessionはWorker間で共有し、完了後のSnapshotだけをUIへ公開する。
//
// =======================================================================
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "System/Scheduler/SystemTask.h"

struct SystemTaskProfileSample {
	size_t nodeIndex = 0;
	size_t taskIndex = 0;
	std::string taskName;
	SystemPhase phase = SystemPhase::Default;
	int32_t priority = 0;
	uint64_t registrationOrder = 0;
	ThreadAffinity declaredAffinity = ThreadAffinity::MainThread;
	size_t workerIndex = static_cast<size_t>(-1);
	double startMilliseconds = 0.0;
	double endMilliseconds = 0.0;
	double durationMilliseconds = 0.0;
	bool succeeded = true;
};

struct SystemScheduleProfileSnapshot {
	SystemTaskDomain domain = SystemTaskDomain::Frame;
	uint64_t captureIndex = 0;
	size_t workerCount = 0;
	bool jobSystemRunning = false;
	double totalMilliseconds = 0.0;
	std::vector<SystemTaskProfileSample> samples;
};

class SystemScheduleProfileSession {
public:
	using Clock = std::chrono::steady_clock;

	SystemScheduleProfileSession(
		SystemTaskDomain domain,
		uint64_t captureIndex,
		size_t workerCount,
		bool jobSystemRunning
	)
		: m_domain(domain),
		  m_captureIndex(captureIndex),
		  m_workerCount(workerCount),
		  m_jobSystemRunning(jobSystemRunning),
		  m_startTime(Clock::now()) {
	}

	double TimestampMilliseconds() const noexcept {
		return std::chrono::duration<double, std::milli>(
			Clock::now() - m_startTime
		).count();
	}

	void RecordTask(
		size_t nodeIndex,
		size_t taskIndex,
		const SystemTask& task,
		size_t workerIndex,
		double startMilliseconds,
		double endMilliseconds,
		bool succeeded
	) {
		SystemTaskProfileSample sample;
		sample.nodeIndex = nodeIndex;
		sample.taskIndex = taskIndex;
		sample.taskName = task.name;
		sample.phase = task.order.phase;
		sample.priority = task.order.priority;
		sample.registrationOrder = task.order.registrationOrder;
		sample.declaredAffinity = task.threadAffinity;
		sample.workerIndex = workerIndex;
		sample.startMilliseconds = startMilliseconds;
		sample.endMilliseconds = endMilliseconds;
		sample.durationMilliseconds =
			(std::max)(0.0, endMilliseconds - startMilliseconds);
		sample.succeeded = succeeded;

		std::scoped_lock lock(m_mutex);
		m_samples.emplace_back(std::move(sample));
	}

	SystemScheduleProfileSnapshot Finish() {
		SystemScheduleProfileSnapshot snapshot;
		snapshot.domain = m_domain;
		snapshot.captureIndex = m_captureIndex;
		snapshot.workerCount = m_workerCount;
		snapshot.jobSystemRunning = m_jobSystemRunning;
		snapshot.totalMilliseconds = TimestampMilliseconds();

		{
			std::scoped_lock lock(m_mutex);
			snapshot.samples = m_samples;
		}

		std::sort(
			snapshot.samples.begin(),
			snapshot.samples.end(),
			[](const SystemTaskProfileSample& lhs,
			   const SystemTaskProfileSample& rhs) {
				if(lhs.startMilliseconds != rhs.startMilliseconds) {
					return lhs.startMilliseconds < rhs.startMilliseconds;
				}
				return lhs.registrationOrder < rhs.registrationOrder;
			}
		);
		return snapshot;
	}

private:
	SystemTaskDomain m_domain = SystemTaskDomain::Frame;
	uint64_t m_captureIndex = 0;
	size_t m_workerCount = 0;
	bool m_jobSystemRunning = false;
	Clock::time_point m_startTime;

	std::mutex m_mutex;
	std::vector<SystemTaskProfileSample> m_samples;
};

class SystemScheduleProfiler {
public:
	using Session = SystemScheduleProfileSession;

	bool IsEnabled() const noexcept {
		return m_enabled.load(std::memory_order_acquire);
	}

	void SetEnabled(bool enabled) noexcept {
		m_enabled.store(enabled, std::memory_order_release);
	}

	std::shared_ptr<Session> Begin(
		SystemTaskDomain domain,
		size_t workerCount,
		bool jobSystemRunning
	) {
		if(!IsEnabled()) {
			return nullptr;
		}

		const uint64_t captureIndex =
			m_nextCaptureIndex.fetch_add(1, std::memory_order_relaxed);
		return std::make_shared<Session>(
			domain,
			captureIndex,
			workerCount,
			jobSystemRunning
		);
	}

	void Publish(const std::shared_ptr<Session>& session) {
		if(!session) {
			return;
		}

		SystemScheduleProfileSnapshot snapshot = session->Finish();
		const size_t domainIndex = static_cast<size_t>(snapshot.domain);
		if(domainIndex >= m_latestSnapshots.size()) {
			return;
		}

		std::scoped_lock lock(m_mutex);
		m_latestSnapshots[domainIndex] = std::move(snapshot);
	}

	std::optional<SystemScheduleProfileSnapshot> GetLatestSnapshot(
		SystemTaskDomain domain
	) const {
		const size_t domainIndex = static_cast<size_t>(domain);
		if(domainIndex >= m_latestSnapshots.size()) {
			return std::nullopt;
		}

		std::scoped_lock lock(m_mutex);
		return m_latestSnapshots[domainIndex];
	}

	void Clear() {
		std::scoped_lock lock(m_mutex);
		for(auto& snapshot : m_latestSnapshots) {
			snapshot.reset();
		}
	}

private:
	std::atomic<bool> m_enabled{true};
	std::atomic<uint64_t> m_nextCaptureIndex{1};

	mutable std::mutex m_mutex;
	std::array<std::optional<SystemScheduleProfileSnapshot>, 4>
		m_latestSnapshots;
};
