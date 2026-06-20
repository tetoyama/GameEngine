#pragma once

namespace DependencyScheduleDetail {

inline void Runner::RunMainQueue(
	const std::shared_ptr<DependencyScheduleState>& state
) {
	for(;;) {
		size_t nodeIndex = 0;
		{
			std::unique_lock lock(state->mutex);
			state->condition.wait(lock, [&state] {
				return state->remaining == 0 || !state->mainReady.empty();
			});

			if(state->remaining == 0) return;

			nodeIndex = state->mainReady.top();
			state->mainReady.pop();
		}

		RunNode(state, nodeIndex);
	}
}

inline void Runner::Dispatch(
	const std::shared_ptr<DependencyScheduleState>& state,
	size_t nodeIndex
) {
	const size_t taskIndex = state->schedule->nodes[nodeIndex].taskIndex;
	SystemTask& task = (*state->tasks)[taskIndex];

	if(task.threadAffinity == ThreadAffinity::AnyWorker &&
		state->jobs->IsRunning()) {
		try {
			state->jobs->Submit([state, nodeIndex](JobThreadContext&) {
				RunNode(state, nodeIndex);
			});
			return;
		} catch(...) {
			RecordException(state, std::current_exception());
		}
	}

	{
		std::scoped_lock lock(state->mutex);
		state->mainReady.push(nodeIndex);
	}
	state->condition.notify_one();
}

} // namespace DependencyScheduleDetail
