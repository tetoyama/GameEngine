// =======================================================================
//
// DependencyScheduleRunner.h
//
// =======================================================================
#pragma once

#include "DependencyScheduleState.h"

#include <memory>

namespace DependencyScheduleDetail {

class Runner {
public:
	static void Execute(
		const CompiledSystemSchedule& schedule,
		std::vector<SystemTask>& tasks,
		JobSystem& jobs,
		const SystemTaskContext& context,
		SystemScheduleProfiler* profiler
	);

private:
	static void RunMainQueue(const std::shared_ptr<DependencyScheduleState>& state);
	static void Dispatch(
		const std::shared_ptr<DependencyScheduleState>& state,
		size_t nodeIndex
	);
	static void RunNode(
		const std::shared_ptr<DependencyScheduleState>& state,
		size_t nodeIndex
	) noexcept;
	static void CompleteNode(
		const std::shared_ptr<DependencyScheduleState>& state,
		size_t nodeIndex
	) noexcept;
	static void RecordException(
		const std::shared_ptr<DependencyScheduleState>& state,
		std::exception_ptr exception
	) noexcept;
};

} // namespace DependencyScheduleDetail
