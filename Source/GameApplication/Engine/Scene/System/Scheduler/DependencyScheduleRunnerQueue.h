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

		const size_t taskIndex = state->schedule->nodes[nodeIndex].taskIndex;
		const SystemTask& task = (*state->tasks)[taskIndex];

		// Worker-local CommandをEntityCommandBufferへ転送してから、
		// Domain末尾のExclusive構造変更Taskを実行する。
		if(task.access.structuralAccess == StructuralAccess::ExclusiveWorldWrite &&
			state->jobs->IsRunning()) {
			bool failed = false;
			{
				std::scoped_lock lock(state->mutex);
				failed = state->failed;
			}

			if(!failed) {
				try {
					state->jobs->FlushThreadCommands();
				} catch(...) {
					RecordException(state, std::current_exception());
				}
			}
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

	// MainThreadと、専用RenderThread未導入のRenderThread Taskを
	// 呼び出しThread上の安定した論理順序で処理する。
	{
		std::scoped_lock lock(state->mutex);
		state->mainReady.push(nodeIndex);
	}
	state->condition.notify_one();
}

} // namespace DependencyScheduleDetail
