#pragma once

namespace DependencyScheduleDetail {

inline void Runner::Execute(
	const CompiledSystemSchedule& schedule,
	std::vector<SystemTask>& tasks,
	JobSystem& jobs,
	const SystemTaskContext& context
) {
	ValidateSchedule(schedule);
	if(schedule.nodes.empty()) return;

	auto state = std::make_shared<DependencyScheduleState>(
		schedule,
		tasks,
		jobs,
		context
	);

	std::vector<size_t> ready;
	ready.reserve(schedule.nodes.size());
	for(size_t index = 0; index < schedule.nodes.size(); ++index) {
		state->unresolved[index] = schedule.nodes[index].dependencies.size();
		if(state->unresolved[index] == 0) ready.push_back(index);
	}

	for(size_t nodeIndex : ready) Dispatch(state, nodeIndex);
	RunMainQueue(state);

	bool failed = false;
	{
		std::scoped_lock lock(state->mutex);
		failed = state->failed;
	}

	// Structural Commitが存在しないScheduleでも、成功時には残りの
	// Worker-local CommandをMainThreadで適用する。
	if(jobs.IsRunning() && !failed) {
		try {
			jobs.FlushThreadCommands();
		} catch(...) {
			RecordException(state, std::current_exception());
		}
	}

	RethrowScheduleException(state);
}

} // namespace DependencyScheduleDetail
