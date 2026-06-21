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
		const size_t taskIndex =
			state->schedule->nodes[nodeIndex].taskIndex;
		SystemTask& task = (*state->tasks)[taskIndex];

		SystemTaskContext taskContext = state->context;
		if(state->jobs->IsRunning()) {
			taskContext.jobContext = &state->jobs->CurrentContext();
		}

		const size_t workerIndex = taskContext.jobContext
			? taskContext.jobContext->workerIndex
			: JobSystem::InvalidWorkerIndex;
		const double startMilliseconds = state->profileSession
			? state->profileSession->TimestampMilliseconds()
			: 0.0;
		bool succeeded = true;

		try {
			if(task.execute) task.execute(taskContext);
		} catch(...) {
			succeeded = false;
			RecordException(state, std::current_exception());
		}

		if(state->profileSession) {
			const double endMilliseconds =
				state->profileSession->TimestampMilliseconds();
			state->profileSession->RecordTask(
				nodeIndex,
				taskIndex,
				task,
				workerIndex,
				startMilliseconds,
				endMilliseconds,
				succeeded
			);
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
