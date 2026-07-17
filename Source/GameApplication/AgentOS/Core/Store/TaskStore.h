// =======================================================================
//
// TaskStore.h
//
// Task / Evidence / LogicNode / LogicEdge / Command / Session の永続化。
// SQLite(WAL)上に構想§8のスキーマを構築し、単一mutexでスレッドセーフに操作する。
// 全ての変更操作はTransactionで包む（構想§7: Task単位でトランザクション）。
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

// ---------------------------------
// Taskの1行分（読み取り専用ビュー）
// ---------------------------------
struct TaskRow {
	TaskId id = kInvalidId;
	SessionId sessionId = kInvalidId;
	TaskId parentId = kInvalidId; // 0ならroot
	std::string type;
	TaskState state = TaskState::Pending;
	int depth = 0;
	int retryCount = 0;
	Json spec = Json::object();
	Json result = Json::object();
};

// ---------------------------------
// TaskStore
// ---------------------------------
class TaskStore {
public:
	TaskStore() = default;
	~TaskStore() = default;

	TaskStore(const TaskStore&) = delete;
	TaskStore& operator=(const TaskStore&) = delete;

	Result Open(const std::string& path);

	// -----------------------------
	// Session
	// -----------------------------
	SessionId CreateSession(const Json& goal);
	Result UpdateSessionState(SessionId sessionId, const std::string& state);

	// -----------------------------
	// Task
	// -----------------------------
	TaskId CreateTask(SessionId sessionId, TaskId parentId, const std::string& type,
	                   const Json& spec, int depth);
	Result UpdateTaskState(TaskId taskId, TaskState newState);
	Result SetTaskResult(TaskId taskId, const Json& result);
	Result IncrementRetry(TaskId taskId, int* newCount);

	std::optional<TaskRow> GetTask(TaskId taskId);
	std::vector<TaskRow> GetChildren(TaskId parentId);
	std::vector<TaskRow> GetTasksByState(SessionId sessionId, TaskState state);

	// -----------------------------
	// Evidence
	// -----------------------------
	EvidenceId AddEvidence(const Evidence& evidence);
	std::optional<Evidence> GetEvidence(EvidenceId evidenceId);
	std::vector<Evidence> GetEvidenceForTask(TaskId taskId);
	std::vector<Evidence> GetEvidenceForSession(SessionId sessionId);

	// -----------------------------
	// Logic
	// -----------------------------
	LogicNodeId AddLogicNode(TaskId taskId, const std::string& hypothesis, double confidence);
	Result UpdateLogicNode(LogicNodeId nodeId, double confidence, const std::string& status);
	Result AddLogicEdge(std::int64_t fromId, std::int64_t toId, const std::string& relation);

	// -----------------------------
	// Command
	// -----------------------------
	Result RecordCommand(TaskId taskId, const std::string& issuer, const std::string& tool,
	                      const Json& args, const std::string& validationStatus,
	                      const std::string& executionStatus, const Json& result);

	// -----------------------------
	// Summary
	// -----------------------------
	Json GetSessionSummary(SessionId sessionId);

private:
	Result CreateSchema();
	TaskRow RowFromStatement(Statement& stmt);
	Evidence EvidenceFromStatement(Statement& stmt);

	SqliteDb db_;
	std::mutex mutex_;
};

} // namespace agentos
