// =======================================================================
//
// Supervisor.h
//
// 推論を行わない番人（構想§7）。担当はTask状態遷移の合法性検査・
// 再帰深度チェック・リトライ上限のみ。実際の合法性判定はTaskStore
// （IsLegalTransition経由）に委譲し、ここでは二重に判断しない。
//
// =======================================================================
#pragma once

#include <string>

#include "../AgentOsTypes.h"
#include "../Json.h"
#include "../Budget/Budget.h"
#include "../Store/TaskStore.h"

namespace agentos {

class Supervisor {
public:
	explicit Supervisor(TaskStore* store) : store_(store) {}

	// Pending -> Running
	Result StartTask(TaskId taskId);

	// Running -> Succeeded（resultを保存してから遷移する）
	Result CompleteTask(TaskId taskId, const Json& result);

	// Running -> Failed（errorをresultとして保存してから遷移する）
	Result FailTask(TaskId taskId, const std::string& error);

	// リトライ回数をTaskStore経由でインクリメントし、maxRetriesを超えていればFail。
	Result RequestRetry(TaskId taskId, int maxRetries);

	// 再帰深度がBudget.maxDepthを超えていないかを検査する。
	Result CheckDepth(int depth, const Budget& budget) const;

private:
	TaskStore* store_ = nullptr;
};

} // namespace agentos
