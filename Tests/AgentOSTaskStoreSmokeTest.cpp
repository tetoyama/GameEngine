// =======================================================================
//
// AgentOSTaskStoreSmokeTest.cpp
//
// AgentOS Core / Store (SqliteDb + TaskStore) の自己完結スモークテスト。
// エンジン規約どおりmain()+assertのみで完結する。
//
// =======================================================================
#include "AgentOS/Core/Store/SqliteDb.h"
#include "AgentOS/Core/Store/TaskStore.h"
#include "AgentOS/Core/AgentOsTypes.h"
#include "AgentOS/Core/Json.h"
#include "AgentOS/Core/Evidence/Evidence.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>

using namespace agentos;

namespace {

const char* kDbPath = "/tmp/agentos_store_test.db";

void RemoveIfExists(const std::string& path) {
	std::remove(path.c_str());
	std::remove((path + "-wal").c_str());
	std::remove((path + "-shm").c_str());
	std::remove((path + "-journal").c_str());
}

// ---------------------------------
// WALモードが有効であることを直接確認する。
// ---------------------------------
void TestWalMode() {
	SqliteDb raw;
	Result r = raw.Open(kDbPath);
	assert(r.ok);

	Statement stmt;
	Result pr = raw.Prepare("PRAGMA journal_mode;", &stmt);
	assert(pr.ok);
	assert(stmt.Step() == Statement::StepResult::Row);
	std::string mode = stmt.ColumnText(0);
	assert(mode == "wal");
}

// ---------------------------------
// Session / Task 作成、親子関係、depth
// ---------------------------------
void TestSessionAndTaskHierarchy(TaskStore& store) {
	Json goal = Json::object();
	goal["text"] = "原因調査";
	SessionId sessionId = store.CreateSession(goal);
	assert(sessionId != kInvalidId);

	Result r = store.UpdateSessionState(sessionId, "Running");
	assert(r.ok);

	TaskId rootId = store.CreateTask(sessionId, kInvalidId, "RootTask", Json::object(), 0);
	assert(rootId != kInvalidId);

	TaskId childId = store.CreateTask(sessionId, rootId, "ChildTask", Json::object(), 1);
	assert(childId != kInvalidId);

	auto rootRow = store.GetTask(rootId);
	assert(rootRow.has_value());
	assert(rootRow->parentId == kInvalidId);
	assert(rootRow->depth == 0);
	assert(rootRow->state == TaskState::Pending);

	auto childRow = store.GetTask(childId);
	assert(childRow.has_value());
	assert(childRow->parentId == rootId);
	assert(childRow->depth == 1);

	auto children = store.GetChildren(rootId);
	assert(children.size() == 1);
	assert(children[0].id == childId);
}

// ---------------------------------
// Task状態機械（合法/不正遷移）
// ---------------------------------
void TestStateMachine(TaskStore& store) {
	SessionId sessionId = store.CreateSession(Json::object());
	TaskId taskId = store.CreateTask(sessionId, kInvalidId, "T", Json::object(), 0);

	Result r1 = store.UpdateTaskState(taskId, TaskState::Running);
	assert(r1.ok);

	Result r2 = store.UpdateTaskState(taskId, TaskState::Succeeded);
	assert(r2.ok);

	// 終端状態からの遷移は禁止（Succeeded -> Running）
	Result r3 = store.UpdateTaskState(taskId, TaskState::Running);
	assert(!r3.ok);
	assert(!r3.error.empty());

	// Pending -> Succeeded は禁止（Runningを経由しないため）
	TaskId taskId2 = store.CreateTask(sessionId, kInvalidId, "T2", Json::object(), 0);
	Result r4 = store.UpdateTaskState(taskId2, TaskState::Succeeded);
	assert(!r4.ok);

	auto row = store.GetTask(taskId2);
	assert(row.has_value());
	assert(row->state == TaskState::Pending); // 拒否されたので変化していない
}

// ---------------------------------
// リトライカウント
// ---------------------------------
void TestRetryIncrement(TaskStore& store) {
	SessionId sessionId = store.CreateSession(Json::object());
	TaskId taskId = store.CreateTask(sessionId, kInvalidId, "T", Json::object(), 0);

	int count = -1;
	Result r1 = store.IncrementRetry(taskId, &count);
	assert(r1.ok);
	assert(count == 1);

	Result r2 = store.IncrementRetry(taskId, &count);
	assert(r2.ok);
	assert(count == 2);

	auto row = store.GetTask(taskId);
	assert(row.has_value());
	assert(row->retryCount == 2);
}

// ---------------------------------
// Evidence往復（claim/payload/provenance/frame/confidence）
// ---------------------------------
void TestEvidenceRoundTrip(TaskStore& store) {
	SessionId sessionId = store.CreateSession(Json::object());
	TaskId taskId = store.CreateTask(sessionId, kInvalidId, "T", Json::object(), 0);

	Evidence e;
	e.taskId = taskId;
	e.claim = "VelocitySystem が Transform.position.y を上書きしている";
	e.payload = Json::object();
	e.payload["component"] = "Transform";
	e.payload["field"] = "position.y";
	e.provenance.sourceType = "RuntimeTrace";
	e.provenance.sourceUri = "WriteTrace#3";
	e.provenance.session = "run_51";
	e.provenance.frame = 128;
	e.confidence = 0.85;

	EvidenceId id = store.AddEvidence(e);
	assert(id != kInvalidId);

	auto fetched = store.GetEvidence(id);
	assert(fetched.has_value());
	assert(fetched->claim == e.claim);
	assert(fetched->payload["component"] == "Transform");
	assert(fetched->payload["field"] == "position.y");
	assert(fetched->provenance.sourceType == "RuntimeTrace");
	assert(fetched->provenance.sourceUri == "WriteTrace#3");
	assert(fetched->provenance.session == "run_51");
	assert(fetched->provenance.frame == 128);
	assert(fetched->confidence > 0.849 && fetched->confidence < 0.851);

	auto forTask = store.GetEvidenceForTask(taskId);
	assert(forTask.size() == 1);
	assert(forTask[0].id == id);

	auto forSession = store.GetEvidenceForSession(sessionId);
	assert(forSession.size() == 1);
	assert(forSession[0].id == id);
}

// ---------------------------------
// LogicNode / LogicEdge
// ---------------------------------
void TestLogicGraph(TaskStore& store) {
	SessionId sessionId = store.CreateSession(Json::object());
	TaskId taskId = store.CreateTask(sessionId, kInvalidId, "T", Json::object(), 0);

	Evidence e;
	e.taskId = taskId;
	e.claim = "Logic用Evidence";
	e.provenance.sourceType = "CodeSearch";
	EvidenceId evId = store.AddEvidence(e);
	assert(evId != kInvalidId);

	LogicNodeId nodeId = store.AddLogicNode(taskId, "VelocitySystemが原因", 0.4);
	assert(nodeId != kInvalidId);

	Result ur = store.UpdateLogicNode(nodeId, 0.75, "Confirmed");
	assert(ur.ok);

	Result er = store.AddLogicEdge(nodeId, evId, "supports");
	assert(er.ok);

	// 不明なnodeIdの更新は失敗する
	Result badUpdate = store.UpdateLogicNode(999999, 0.1, "Rejected");
	assert(!badUpdate.ok);
}

// ---------------------------------
// Command監査記録
// ---------------------------------
void TestCommandRecord(TaskStore& store) {
	SessionId sessionId = store.CreateSession(Json::object());
	TaskId taskId = store.CreateTask(sessionId, kInvalidId, "T", Json::object(), 0);

	Json args = Json::object();
	args["entityId"] = 42;
	Json result = Json::object();
	result["ok"] = true;

	Result r = store.RecordCommand(taskId, "PlannerAgent", "ReadComponent", args, "Ok", "Ok", result);
	assert(r.ok);
}

// ---------------------------------
// GetSessionSummary
// ---------------------------------
void TestSessionSummary(TaskStore& store) {
	SessionId sessionId = store.CreateSession(Json::object());
	TaskId t1 = store.CreateTask(sessionId, kInvalidId, "T1", Json::object(), 0);
	TaskId t2 = store.CreateTask(sessionId, kInvalidId, "T2", Json::object(), 0);
	TaskId t3 = store.CreateTask(sessionId, kInvalidId, "T3", Json::object(), 0);

	Result sr1 = store.UpdateTaskState(t1, TaskState::Running);
	assert(sr1.ok);
	Result sr2 = store.UpdateTaskState(t1, TaskState::Succeeded);
	assert(sr2.ok);
	Result sr3 = store.UpdateTaskState(t2, TaskState::Running);
	assert(sr3.ok);
	// t3はPendingのまま

	Evidence e;
	e.taskId = t1;
	e.claim = "summary用Evidence";
	e.provenance.sourceType = "Log";
	EvidenceId evId = store.AddEvidence(e);
	assert(evId != kInvalidId);

	Result cr1 = store.RecordCommand(t1, "Agent", "Tool", Json::object(), "Ok", "Ok", Json::object());
	assert(cr1.ok);
	Result cr2 = store.RecordCommand(t2, "Agent", "Tool", Json::object(), "Ok", "Ok", Json::object());
	assert(cr2.ok);

	Json summary = store.GetSessionSummary(sessionId);
	assert(summary["evidenceCount"].get<std::int64_t>() == 1);
	assert(summary["commandCount"].get<std::int64_t>() == 2);
	assert(summary["tasksByState"]["Succeeded"].get<std::int64_t>() == 1);
	assert(summary["tasksByState"]["Running"].get<std::int64_t>() == 1);
	assert(summary["tasksByState"]["Pending"].get<std::int64_t>() == 1);

	(void)t3;
}

// ---------------------------------
// トランザクションの原子性（存在しないTaskへの操作は綺麗に失敗する）
// ---------------------------------
void TestAtomicityOnNonexistentTask(TaskStore& store) {
	Result r1 = store.UpdateTaskState(999999, TaskState::Running);
	assert(!r1.ok);
	assert(!r1.error.empty());

	Result r2 = store.SetTaskResult(999999, Json::object());
	assert(!r2.ok);

	int count = -1;
	Result r3 = store.IncrementRetry(999999, &count);
	assert(!r3.ok);
	assert(count == -1); // 変更されていないこと
}

} // namespace

int main() {
	RemoveIfExists(kDbPath);

	TestWalMode();

	TaskStore store;
	Result openResult = store.Open(kDbPath);
	assert(openResult.ok);

	TestSessionAndTaskHierarchy(store);
	TestStateMachine(store);
	TestRetryIncrement(store);
	TestEvidenceRoundTrip(store);
	TestLogicGraph(store);
	TestCommandRecord(store);
	TestSessionSummary(store);
	TestAtomicityOnNonexistentTask(store);

	std::cout << "AgentOSTaskStoreSmokeTest: OK" << std::endl;
	return 0;
}
