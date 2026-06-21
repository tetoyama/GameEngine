#pragma once

namespace DependencyScheduleDetail {

inline void Runner::RunNode(
	const std::shared_ptr<DependencyScheduleState>& state,
	size_t nodeIndex
) noexcept {
	bool shouldExecute = false;
	{
		std::scoped_lock lock(state->mutex);
		shouldExecute = !state->failed;
	}

	if(shouldExecute) {
		try {
			const size_t taskIndex =
				state->schedule->nodes[nodeIndex].taskIndex;
			SystemTask& task = (*state->tasks)[taskIndex];

			SystemTaskContext taskContext = state->context;
			if(state->jobs->IsRunning()) {
				taskContext.jobContext = &state->jobs->CurrentContext();
			}

			if(task.execute) task.execute(taskContext);
		} catch(...) {
			RecordException(state, std::current_exception());
		}
	}

	CompleteNode(state, nodeIndex);
}

inline void Runner::CompleteNode(
	const std::shared_ptr<DependencyScheduleState>& state,
	size_t nodeIndex
) noexcept {
	std::vector<size_t> ready;
	{
		std::scoped_lock lock(state->mutex);
		for(size_t dependent : state->schedule->nodes[nodeIndex].dependents) {
			if(state->unresolved[dependent] == 0) continue;
			--state->unresolved[dependent];
			if(state->unresolved[dependent] == 0) ready.push_back(dependent);
		}

		if(state->remaining != 0) --state->remaining;
	}

	for(size_t dependent : ready) Dispatch(state, dependent);
	state->condition.notify_all();
}

} // namespace DependencyScheduleDetail
