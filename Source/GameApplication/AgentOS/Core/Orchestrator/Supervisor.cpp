// =======================================================================
//
// Supervisor.cpp
//
// =======================================================================
#include "Supervisor.h"

namespace agentos {

Result Supervisor::StartTask(TaskId taskId) {
	return store_->UpdateTaskState(taskId, TaskState::Running);
}

Result Supervisor::CompleteTask(TaskId taskId, const Json& result) {
	Result setResult = store_->SetTaskResult(taskId, result);
	if (!setResult) {
		return setResult;
	}
	return store_->UpdateTaskState(taskId, TaskState::Succeeded);
}

Result Supervisor::FailTask(TaskId taskId, const std::string& error) {
	// resultの保存に失敗しても（監査上望ましくはないが）状態遷移は試みる。
	store_->SetTaskResult(taskId, Json::object({{"error", error}}));
	return store_->UpdateTaskState(taskId, TaskState::Failed);
}

Result Supervisor::RequestRetry(TaskId taskId, int maxRetries) {
	int newCount = -1;
	Result incrementResult = store_->IncrementRetry(taskId, &newCount);
	if (!incrementResult) {
		return incrementResult;
	}
	if (newCount > maxRetries) {
		return Result::Fail(
			"retry limit exceeded: " + std::to_string(newCount) + " > " + std::to_string(maxRetries));
	}
	return Result::Ok();
}

Result Supervisor::CheckDepth(int depth, const Budget& budget) const {
	if (depth > budget.maxDepth) {
		return Result::Fail(
			"depth " + std::to_string(depth) + " exceeds budget.maxDepth=" + std::to_string(budget.maxDepth));
	}
	return Result::Ok();
}

} // namespace agentos
