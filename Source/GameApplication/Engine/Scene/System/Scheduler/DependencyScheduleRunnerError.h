#pragma once

#include <stdexcept>

namespace DependencyScheduleDetail {

inline void ValidateSchedule(const CompiledSystemSchedule& schedule) {
	if(schedule.valid) return;
	throw std::runtime_error(
		schedule.error.empty()
			? "Compiled system schedule is invalid."
			: schedule.error
	);
}

inline void Runner::RecordException(
	const std::shared_ptr<DependencyScheduleState>& state,
	std::exception_ptr exception
) noexcept {
	std::scoped_lock lock(state->mutex);
	state->failed = true;
	if(!state->exception) state->exception = std::move(exception);
}

inline void RethrowScheduleException(
	const std::shared_ptr<DependencyScheduleState>& state
) {
	std::exception_ptr exception;
	{
		std::scoped_lock lock(state->mutex);
		exception = state->exception;
	}
	if(exception) std::rethrow_exception(exception);
}

} // namespace DependencyScheduleDetail
