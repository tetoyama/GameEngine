#pragma once

namespace DependencyScheduleDetail {

inline void Runner::Execute(
	const CompiledSystemSchedule& schedule,
	std::vector<SystemTask>& tasks,
	JobSystem& jobs,
	const SystemTaskContext& context,
	SystemScheduleProfiler* profiler
) {
	ValidateSchedule(schedule);

	std::shared_ptr<SystemScheduleProfileSession> profileSession =
		profiler
			? profiler->Begin(schedule.domain, jobs.WorkerCount(), jobs.IsRunning())
			: nullptr;

	if(schedule.nodes.empty()) {
		if(profiler && profileSession) {
			profiler->Publish(profileSession);
		}
		return;
	}

	auto state = std::make_shared<DependencyScheduleState>(
		schedule,
		tasks,
		jobs,
		context,
		profileSession
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

	if(jobs.IsRunning() && !failed) {
		try {
			jobs.FlushThreadCommands();
		} catch(...) {
			RecordException(state, std::current_exception());
		}
	}

	if(profiler && profileSession) {
		profiler->Publish(profileSession);
	}

	RethrowScheduleException(state);
}

} // namespace DependencyScheduleDetail
