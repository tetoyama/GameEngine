// =======================================================================
//
// DependencyScheduleExecutor.h
//
// =======================================================================
#pragma once

#include "System/Job/JobSystem.h"
#include "System/Scheduler/SystemScheduleCompiler.h"

#include <vector>

class DependencyScheduleExecutor {
public:
	static void Execute(
		const CompiledSystemSchedule& schedule,
		std::vector<SystemTask>& tasks,
		JobSystem& jobs,
		const SystemTaskContext& context
	);
};
