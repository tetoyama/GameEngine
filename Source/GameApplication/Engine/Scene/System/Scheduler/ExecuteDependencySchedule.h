#pragma once

#include "DependencyScheduleRunnerAll.h"

inline void ExecuteDependencySchedule(
	const CompiledSystemSchedule& schedule,
	std::vector<SystemTask>& tasks,
	JobSystem& jobs,
	const SystemTaskContext& context,
	SystemScheduleProfiler* profiler = nullptr
) {
	DependencyScheduleDetail::Runner::Execute(
		schedule,
		tasks,
		jobs,
		context,
		profiler
	);
}
