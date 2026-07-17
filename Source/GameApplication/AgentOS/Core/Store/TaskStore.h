// =======================================================================
//
// TaskStore.h
//
// Task / Evidence / Logic / Command / Session / Conversation MemoryをSQLiteへ永続化する。
// Conversation Turnの原文は削除せず、累積要約は別レコードで管理する。
//
// =======================================================================
#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "../AgentOsTypes.h"
#include "../Json.h"
#include "../Evidence/Evidence.h"

#include "SqliteDb.h"

namespace agentos {

struct TaskRow {
	TaskId id = kInvalidId;
	SessionId sessionId = kInvalidId;
	TaskId parentId = kInvalidId;
	std::string type;
	TaskState state = TaskState::Pending;
	int depth = 0;
	int retryCount = 0;
	Json spec = Json::object();
	Json result = Json::object();
};

class TaskStore {
public:
	TaskStore() = default;
	~TaskStore() = default;

	TaskStore(const TaskStore&) = delete;
	TaskStore& operator=(const TaskStore&) = delete;

	Result Open(const std::string& path);

	// Session / Conversation Turn
	SessionId CreateSession(const Json& goal);
	Result UpdateSessionState(SessionId sessionId, const std::string& state);
	Result SetConversationResponse(SessionId sessionId, const std::string& assistantText);

	// beforeSessionIdより前の全完了Turnを対象にする。
	// 原文TurnはDBに全件残し、返却Contextは
	// {summary, summarizedThroughSessionId, recentTurns, totalTurns}。
	Json GetConversationContext(SessionId beforeSessionId);
	Result UpdateConversationSummary(
		const std::string& summary,
		SessionId summarizedThroughSessionId);

	// Task
	TaskId CreateTask(SessionId sessionId, TaskId parentId, const std::string& type,
	                   const Json& spec, int depth);
	Result UpdateTaskState(TaskId taskId, TaskState newState);
	Result SetTaskResult(TaskId taskId, const Json& result);
	Result IncrementRetry(TaskId taskId, int* newCount);

	std::optional<TaskRow> GetTask(TaskId taskId);
	std::vector<TaskRow> GetChildren(TaskId parentId);
	std::vector<TaskRow> GetTasksByState(SessionId sessionId, TaskState state);

	// Evidence
	EvidenceId AddEvidence(const Evidence& evidence);
	std::optional<Evidence> GetEvidence(EvidenceId evidenceId);
	std::vector<Evidence> GetEvidenceForTask(TaskId taskId);
	std::vector<Evidence> GetEvidenceForSession(SessionId sessionId);

	// Logic
	LogicNodeId AddLogicNode(TaskId taskId, const std::string& hypothesis, double confidence);
	Result UpdateLogicNode(LogicNodeId nodeId, double confidence, const std::string& status);
	Result AddLogicEdge(std::int64_t fromId, std::int64_t toId, const std::string& relation);

	// Command
	Result RecordCommand(TaskId taskId, const std::string& issuer, const std::string& tool,
	                      const Json& args, const std::string& validationStatus,
	                      const std::string& executionStatus, const Json& result);

	Json GetSessionSummary(SessionId sessionId);

private:
	Result CreateSchema();
	TaskRow RowFromStatement(Statement& stmt);
	Evidence EvidenceFromStatement(Statement& stmt);
	Result SetConversationResponseLocked(SessionId sessionId, const std::string& assistantText);

	SqliteDb db_;
	std::mutex mutex_;
};

} // namespace agentos
