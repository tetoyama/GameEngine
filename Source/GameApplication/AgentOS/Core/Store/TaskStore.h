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
	Result SetConversationThreadState(SessionId sessionId, const Json& state);

	// beforeSessionIdより前の全完了Turnを対象にする。
	// 原文TurnはDBに全件残し、返却Contextは
	// {summary, summarizedThroughSessionId, recentTurns, threadStates, totalTurns}。
	Json GetConversationContext(SessionId beforeSessionId);
	Result UpdateConversationSummary(
		const std::string& summary,
		SessionId summarizedThroughSessionId);

	// 過去セッションの記録を種別横断で引く（GetConversationHistoryツールの実体）。
	//
	// Intakeのキーワード判定は「履歴が要るか」を先回りで決めてしまうため、
	// 「5件全部知りたい」のような明らかな継続を取りこぼす（実機で観測）。
	// 要ると気づいた側（Planner/Worker/Repair）が自分で取りに行けるようにする。
	//
	// 返すのはJson配列。各要素は {kind, sessionId, ...}。
	//   kind="evidence"     : 過去のTool実行結果（観測。claim/payload/sourceTypeを持つ）
	//   kind="userTurn"     : ユーザの過去発話
	//   kind="threadState"  : Intakeが確定させた構造化要約
	//   kind="assistantTurn": 過去のAgent応答
	// queryが空でなければ本文の部分一致で絞る（大小無視のASCII比較）。
	// 失敗Evidenceのうち、最終応答へ反映されなかったものは返さない。
	// 反映された失敗（例:「そのEntityは存在しなかった」と応答した）は
	// 「前回これは無かった」という知識なので残す。
	// 反映されなかった失敗は、同じ探索を繰り返させるだけで害になる
	// （実機: 存在しない Taro を探した ToolUnsatisfied が次セッションへ
	//  観測として戻り、修復ラウンドを空振りさせた）。
	Json SearchConversationHistory(
		SessionId beforeSessionId,
		const std::string& query,
		const std::vector<std::string>& kinds,
		int limitPerKind);

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

	// 最終応答へ反映されたEvidenceへ印をつける（＝仮説のsupportsに挙がったもの）。
	// 失敗Evidenceを次のセッションへ引き継ぐかの判定に使う。
	Result MarkEvidenceReflected(const std::vector<EvidenceId>& evidenceIds);

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
	// 既存DBに列が在るか（後から足した列の移行判定に使う）。
	bool HasColumn(const std::string& table, const std::string& column);

	Result CreateSchema();
	TaskRow RowFromStatement(Statement& stmt);
	Evidence EvidenceFromStatement(Statement& stmt);
	Result SetConversationResponseLocked(SessionId sessionId, const std::string& assistantText);

	SqliteDb db_;
	std::mutex mutex_;
};

} // namespace agentos
