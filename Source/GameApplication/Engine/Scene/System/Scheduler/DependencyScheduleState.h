// =======================================================================
//
// DependencyScheduleState.h
//
// =======================================================================
#pragma once

#include "System/Job/JobSystem.h"
#include "System/Scheduler/SystemScheduleCompiler.h"
#include "System/Scheduler/SystemScheduleProfiler.h"

#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

struct DependencyScheduleState {
	DependencyScheduleState(
		const CompiledSystemSchedule& compiled,
		std::vector<SystemTask>& taskList,
		JobSystem& jobSystem,
		const SystemTaskContext& taskContext,
		std::shared_ptr<SystemScheduleProfileSession> profilerSession
	)
		: schedule(&compiled),
		  tasks(&taskList),
		  jobs(&jobSystem),
		  context(taskContext),
		  profileSession(std::move(profilerSession)),
		  unresolved(compiled.nodes.size(), 0),
		  remaining(compiled.nodes.size()) {
	}

	const CompiledSystemSchedule* schedule = nullptr;
	std::vector<SystemTask>* tasks = nullptr;
	JobSystem* jobs = nullptr;
	SystemTaskContext context;
	std::shared_ptr<SystemScheduleProfileSession> profileSession;

	std::mutex mutex;
	std::condition_variable condition;
	std::vector<size_t> unresolved;
	size_t remaining = 0;
	bool failed = false;
	std::exception_ptr exception;

	std::priority_queue<
		size_t,
		std::vector<size_t>,
		std::greater<size_t>
	> mainReady;
};
