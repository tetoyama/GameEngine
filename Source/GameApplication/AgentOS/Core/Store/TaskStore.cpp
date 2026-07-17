// =======================================================================
//
// TaskStore.cpp
//
// =======================================================================
#include "TaskStore.h"

#include <string>

namespace agentos {

namespace {

// TEXT列 -> TaskState。ToStringの逆変換。共有ヘッダには置かず本ファイル限定。
static TaskState ParseTaskState(const std::string& text) {
	if (text == "Pending")          return TaskState::Pending;
	if (text == "Running")          return TaskState::Running;
	if (text == "Succeeded")        return TaskState::Succeeded;
	if (text == "Failed")           return TaskState::Failed;
	if (text == "Cancelled")        return TaskState::Cancelled;
	if (text == "AwaitingApproval") return TaskState::AwaitingApproval;
	return TaskState::Pending; // 未知値はPending扱い（本来は到達しない）
}

} // namespace

// ---------------------------------
// Open / Schema
// ---------------------------------
Result TaskStore::Open(const std::string& path) {
	std::lock_guard<std::mutex> lock(mutex_);
	Result r = db_.Open(path);
	if (!r) {
		return r;
	}
	return CreateSchema();
}

Result TaskStore::CreateSchema() {
	static const char* kSchemaSql =
		"CREATE TABLE IF NOT EXISTS Session("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  goal_json TEXT NOT NULL,"
		"  state TEXT NOT NULL,"
		"  created_at TEXT NOT NULL DEFAULT (datetime('now')),"
		"  updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
		");"

		"CREATE TABLE IF NOT EXISTS Task("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  session_id INTEGER NOT NULL REFERENCES Session(id),"
		"  parent_id INTEGER REFERENCES Task(id),"
		"  type TEXT NOT NULL,"
		"  state TEXT NOT NULL,"
		"  depth INTEGER NOT NULL DEFAULT 0,"
		"  retry_count INTEGER NOT NULL DEFAULT 0,"
		"  spec_json TEXT NOT NULL DEFAULT '{}',"
		"  result_json TEXT,"
		"  created_at TEXT NOT NULL DEFAULT (datetime('now')),"
		"  updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
		");"

		"CREATE TABLE IF NOT EXISTS Evidence("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  task_id INTEGER NOT NULL REFERENCES Task(id),"
		"  source_type TEXT NOT NULL,"
		"  source_uri TEXT,"
		"  session TEXT,"
		"  frame INTEGER NOT NULL DEFAULT -1,"
		"  claim TEXT NOT NULL,"
		"  payload_json TEXT NOT NULL DEFAULT '{}',"
		"  confidence REAL NOT NULL DEFAULT 1.0,"
		"  created_at TEXT NOT NULL DEFAULT (datetime('now'))"
		");"

		"CREATE TABLE IF NOT EXISTS LogicNode("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  task_id INTEGER NOT NULL REFERENCES Task(id),"
		"  hypothesis TEXT NOT NULL,"
		"  confidence REAL NOT NULL DEFAULT 0,"
		"  status TEXT NOT NULL DEFAULT 'Proposed'"
		");"

		"CREATE TABLE IF NOT EXISTS LogicEdge("
		"  from_id INTEGER NOT NULL,"
		"  to_id INTEGER NOT NULL,"
		"  relation TEXT NOT NULL,"
		"  PRIMARY KEY(from_id, to_id, relation)"
		");"

		"CREATE TABLE IF NOT EXISTS Command("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  task_id INTEGER,"
		"  issuer TEXT NOT NULL,"
		"  tool TEXT NOT NULL,"
		"  arguments_json TEXT NOT NULL,"
		"  validation_status TEXT NOT NULL,"
		"  execution_status TEXT NOT NULL,"
		"  result_json TEXT,"
		"  created_at TEXT NOT NULL DEFAULT (datetime('now'))"
		");"

		"CREATE INDEX IF NOT EXISTS idx_task_session_id ON Task(session_id);"
		"CREATE INDEX IF NOT EXISTS idx_task_parent_id ON Task(parent_id);"
		"CREATE INDEX IF NOT EXISTS idx_evidence_task_id ON Evidence(task_id);"
		"CREATE INDEX IF NOT EXISTS idx_command_task_id ON Command(task_id);";

	return db_.Exec(kSchemaSql);
}

// ---------------------------------
// Session
// ---------------------------------
SessionId TaskStore::CreateSession(const Json& goal) {
	std::lock_guard<std::mutex> lock(mutex_);
	Transaction tx(db_);

	Statement stmt;
	Result r = db_.Prepare("INSERT INTO Session(goal_json, state) VALUES(?1, ?2);", &stmt);
	if (!r) {
		return kInvalidId;
	}
	stmt.BindText(1, goal.dump());
	stmt.BindText(2, "Created");
	if (stmt.Step() == Statement::StepResult::Error) {
		return kInvalidId;
	}

	SessionId id = db_.LastInsertRowId();
	if (!tx.Commit()) {
		return kInvalidId;
	}
	return id;
}

Result TaskStore::UpdateSessionState(SessionId sessionId, const std::string& state) {
	std::lock_guard<std::mutex> lock(mutex_);
	Transaction tx(db_);

	Statement stmt;
	Result r = db_.Prepare("UPDATE Session SET state=?1, updated_at=datetime('now') WHERE id=?2;", &stmt);
	if (!r) {
		return r;
	}
	stmt.BindText(1, state);
	stmt.BindInt64(2, sessionId);
	if (stmt.Step() == Statement::StepResult::Error) {
		return Result::Fail(sqlite3_errmsg(db_.Handle()));
	}
	if (sqlite3_changes(db_.Handle()) == 0) {
		return Result::Fail("UpdateSessionState: session id=" + std::to_string(sessionId) + " が存在しない");
	}

	return tx.Commit();
}

// ---------------------------------
// Task
// ---------------------------------
TaskId TaskStore::CreateTask(SessionId sessionId, TaskId parentId, const std::string& type,
                              const Json& spec, int depth) {
	std::lock_guard<std::mutex> lock(mutex_);
	Transaction tx(db_);

	Statement stmt;
	Result r = db_.Prepare(
		"INSERT INTO Task(session_id, parent_id, type, state, depth, spec_json) "
		"VALUES(?1, ?2, ?3, ?4, ?5, ?6);",
		&stmt);
	if (!r) {
		return kInvalidId;
	}
	stmt.BindInt64(1, sessionId);
	if (parentId == kInvalidId) {
		stmt.BindNull(2);
	} else {
		stmt.BindInt64(2, parentId);
	}
	stmt.BindText(3, type);
	stmt.BindText(4, ToString(TaskState::Pending));
	stmt.BindInt64(5, depth);
	stmt.BindText(6, spec.dump());
	if (stmt.Step() == Statement::StepResult::Error) {
		return kInvalidId;
	}

	TaskId id = db_.LastInsertRowId();
	if (!tx.Commit()) {
		return kInvalidId;
	}
	return id;
}

Result TaskStore::UpdateTaskState(TaskId taskId, TaskState newState) {
	std::lock_guard<std::mutex> lock(mutex_);
	Transaction tx(db_);

	Statement select;
	Result r = db_.Prepare("SELECT state FROM Task WHERE id=?1;", &select);
	if (!r) {
		return r;
	}
	select.BindInt64(1, taskId);
	if (select.Step() != Statement::StepResult::Row) {
		return Result::Fail("UpdateTaskState: task id=" + std::to_string(taskId) + " が存在しない");
	}
	TaskState currentState = ParseTaskState(select.ColumnText(0));

	if (!IsLegalTransition(currentState, newState)) {
		return Result::Fail(
			std::string("不正な状態遷移: ") + ToString(currentState) + " -> " + ToString(newState));
	}

	Statement update;
	r = db_.Prepare("UPDATE Task SET state=?1, updated_at=datetime('now') WHERE id=?2;", &update);
	if (!r) {
		return r;
	}
	update.BindText(1, ToString(newState));
	update.BindInt64(2, taskId);
	if (update.Step() == Statement::StepResult::Error) {
		return Result::Fail(sqlite3_errmsg(db_.Handle()));
	}

	return tx.Commit();
}

Result TaskStore::SetTaskResult(TaskId taskId, const Json& result) {
	std::lock_guard<std::mutex> lock(mutex_);
	Transaction tx(db_);

	Statement stmt;
	Result r = db_.Prepare("UPDATE Task SET result_json=?1, updated_at=datetime('now') WHERE id=?2;", &stmt);
	if (!r) {
		return r;
	}
	stmt.BindText(1, result.dump());
	stmt.BindInt64(2, taskId);
	if (stmt.Step() == Statement::StepResult::Error) {
		return Result::Fail(sqlite3_errmsg(db_.Handle()));
	}
	if (sqlite3_changes(db_.Handle()) == 0) {
		return Result::Fail("SetTaskResult: task id=" + std::to_string(taskId) + " が存在しない");
	}

	return tx.Commit();
}

Result TaskStore::IncrementRetry(TaskId taskId, int* newCount) {
	std::lock_guard<std::mutex> lock(mutex_);
	Transaction tx(db_);

	Statement stmt;
	Result r = db_.Prepare(
		"UPDATE Task SET retry_count = retry_count + 1, updated_at=datetime('now') WHERE id=?1;", &stmt);
	if (!r) {
		return r;
	}
	stmt.BindInt64(1, taskId);
	if (stmt.Step() == Statement::StepResult::Error) {
		return Result::Fail(sqlite3_errmsg(db_.Handle()));
	}
	if (sqlite3_changes(db_.Handle()) == 0) {
		return Result::Fail("IncrementRetry: task id=" + std::to_string(taskId) + " が存在しない");
	}

	if (newCount != nullptr) {
		Statement query;
		Result r2 = db_.Prepare("SELECT retry_count FROM Task WHERE id=?1;", &query);
		if (!r2) {
			return r2;
		}
		query.BindInt64(1, taskId);
		if (query.Step() == Statement::StepResult::Row) {
			*newCount = static_cast<int>(query.ColumnInt64(0));
		}
	}

	return tx.Commit();
}

std::optional<TaskRow> TaskStore::GetTask(TaskId taskId) {
	std::lock_guard<std::mutex> lock(mutex_);

	Statement stmt;
	Result r = db_.Prepare(
		"SELECT id, session_id, parent_id, type, state, depth, retry_count, spec_json, result_json "
		"FROM Task WHERE id=?1;",
		&stmt);
	if (!r) {
		return std::nullopt;
	}
	stmt.BindInt64(1, taskId);
	if (stmt.Step() != Statement::StepResult::Row) {
		return std::nullopt;
	}
	return RowFromStatement(stmt);
}

std::vector<TaskRow> TaskStore::GetChildren(TaskId parentId) {
	std::lock_guard<std::mutex> lock(mutex_);
	std::vector<TaskRow> rows;

	Statement stmt;
	Result r = db_.Prepare(
		"SELECT id, session_id, parent_id, type, state, depth, retry_count, spec_json, result_json "
		"FROM Task WHERE parent_id=?1 ORDER BY id;",
		&stmt);
	if (!r) {
		return rows;
	}
	stmt.BindInt64(1, parentId);
	while (stmt.Step() == Statement::StepResult::Row) {
		rows.push_back(RowFromStatement(stmt));
	}
	return rows;
}

std::vector<TaskRow> TaskStore::GetTasksByState(SessionId sessionId, TaskState state) {
	std::lock_guard<std::mutex> lock(mutex_);
	std::vector<TaskRow> rows;

	Statement stmt;
	Result r = db_.Prepare(
		"SELECT id, session_id, parent_id, type, state, depth, retry_count, spec_json, result_json "
		"FROM Task WHERE session_id=?1 AND state=?2 ORDER BY id;",
		&stmt);
	if (!r) {
		return rows;
	}
	stmt.BindInt64(1, sessionId);
	stmt.BindText(2, ToString(state));
	while (stmt.Step() == Statement::StepResult::Row) {
		rows.push_back(RowFromStatement(stmt));
	}
	return rows;
}

TaskRow TaskStore::RowFromStatement(Statement& stmt) {
	TaskRow row;
	row.id = stmt.ColumnInt64(0);
	row.sessionId = stmt.ColumnInt64(1);
	row.parentId = stmt.ColumnIsNull(2) ? kInvalidId : stmt.ColumnInt64(2);
	row.type = stmt.ColumnText(3);
	row.state = ParseTaskState(stmt.ColumnText(4));
	row.depth = static_cast<int>(stmt.ColumnInt64(5));
	row.retryCount = static_cast<int>(stmt.ColumnInt64(6));

	Json spec = Json::parse(stmt.ColumnText(7), nullptr, false);
	row.spec = spec.is_discarded() ? Json::object() : spec;

	if (stmt.ColumnIsNull(8)) {
		row.result = Json::object();
	} else {
		Json result = Json::parse(stmt.ColumnText(8), nullptr, false);
		row.result = result.is_discarded() ? Json::object() : result;
	}

	return row;
}

// ---------------------------------
// Evidence
// ---------------------------------
EvidenceId TaskStore::AddEvidence(const Evidence& evidence) {
	std::lock_guard<std::mutex> lock(mutex_);
	Transaction tx(db_);

	Statement stmt;
	Result r = db_.Prepare(
		"INSERT INTO Evidence(task_id, source_type, source_uri, session, frame, claim, payload_json, confidence) "
		"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8);",
		&stmt);
	if (!r) {
		return kInvalidId;
	}
	stmt.BindInt64(1, evidence.taskId);
	stmt.BindText(2, evidence.provenance.sourceType);
	stmt.BindText(3, evidence.provenance.sourceUri);
	stmt.BindText(4, evidence.provenance.session);
	stmt.BindInt64(5, evidence.provenance.frame);
	stmt.BindText(6, evidence.claim);
	stmt.BindText(7, evidence.payload.dump());
	stmt.BindDouble(8, evidence.confidence);
	if (stmt.Step() == Statement::StepResult::Error) {
		return kInvalidId;
	}

	EvidenceId id = db_.LastInsertRowId();
	if (!tx.Commit()) {
		return kInvalidId;
	}
	return id;
}

Evidence TaskStore::EvidenceFromStatement(Statement& stmt) {
	Evidence e;
	e.id = stmt.ColumnInt64(0);
	e.taskId = stmt.ColumnInt64(1);
	e.provenance.sourceType = stmt.ColumnText(2);
	e.provenance.sourceUri = stmt.ColumnText(3);
	e.provenance.session = stmt.ColumnText(4);
	e.provenance.frame = stmt.ColumnInt64(5);
	e.claim = stmt.ColumnText(6);

	Json payload = Json::parse(stmt.ColumnText(7), nullptr, false);
	e.payload = payload.is_discarded() ? Json::object() : payload;

	e.confidence = stmt.ColumnDouble(8);
	return e;
}

std::optional<Evidence> TaskStore::GetEvidence(EvidenceId evidenceId) {
	std::lock_guard<std::mutex> lock(mutex_);

	Statement stmt;
	Result r = db_.Prepare(
		"SELECT id, task_id, source_type, source_uri, session, frame, claim, payload_json, confidence "
		"FROM Evidence WHERE id=?1;",
		&stmt);
	if (!r) {
		return std::nullopt;
	}
	stmt.BindInt64(1, evidenceId);
	if (stmt.Step() != Statement::StepResult::Row) {
		return std::nullopt;
	}
	return EvidenceFromStatement(stmt);
}

std::vector<Evidence> TaskStore::GetEvidenceForTask(TaskId taskId) {
	std::lock_guard<std::mutex> lock(mutex_);
	std::vector<Evidence> rows;

	Statement stmt;
	Result r = db_.Prepare(
		"SELECT id, task_id, source_type, source_uri, session, frame, claim, payload_json, confidence "
		"FROM Evidence WHERE task_id=?1 ORDER BY id;",
		&stmt);
	if (!r) {
		return rows;
	}
	stmt.BindInt64(1, taskId);
	while (stmt.Step() == Statement::StepResult::Row) {
		rows.push_back(EvidenceFromStatement(stmt));
	}
	return rows;
}

std::vector<Evidence> TaskStore::GetEvidenceForSession(SessionId sessionId) {
	std::lock_guard<std::mutex> lock(mutex_);
	std::vector<Evidence> rows;

	Statement stmt;
	Result r = db_.Prepare(
		"SELECT Evidence.id, Evidence.task_id, Evidence.source_type, Evidence.source_uri, Evidence.session, "
		"Evidence.frame, Evidence.claim, Evidence.payload_json, Evidence.confidence "
		"FROM Evidence JOIN Task ON Evidence.task_id = Task.id "
		"WHERE Task.session_id=?1 ORDER BY Evidence.id;",
		&stmt);
	if (!r) {
		return rows;
	}
	stmt.BindInt64(1, sessionId);
	while (stmt.Step() == Statement::StepResult::Row) {
		rows.push_back(EvidenceFromStatement(stmt));
	}
	return rows;
}

// ---------------------------------
// Logic
// ---------------------------------
LogicNodeId TaskStore::AddLogicNode(TaskId taskId, const std::string& hypothesis, double confidence) {
	std::lock_guard<std::mutex> lock(mutex_);
	Transaction tx(db_);

	Statement stmt;
	Result r = db_.Prepare(
		"INSERT INTO LogicNode(task_id, hypothesis, confidence, status) VALUES(?1, ?2, ?3, ?4);", &stmt);
	if (!r) {
		return kInvalidId;
	}
	stmt.BindInt64(1, taskId);
	stmt.BindText(2, hypothesis);
	stmt.BindDouble(3, confidence);
	stmt.BindText(4, "Proposed");
	if (stmt.Step() == Statement::StepResult::Error) {
		return kInvalidId;
	}

	LogicNodeId id = db_.LastInsertRowId();
	if (!tx.Commit()) {
		return kInvalidId;
	}
	return id;
}

Result TaskStore::UpdateLogicNode(LogicNodeId nodeId, double confidence, const std::string& status) {
	std::lock_guard<std::mutex> lock(mutex_);
	Transaction tx(db_);

	Statement stmt;
	Result r = db_.Prepare("UPDATE LogicNode SET confidence=?1, status=?2 WHERE id=?3;", &stmt);
	if (!r) {
		return r;
	}
	stmt.BindDouble(1, confidence);
	stmt.BindText(2, status);
	stmt.BindInt64(3, nodeId);
	if (stmt.Step() == Statement::StepResult::Error) {
		return Result::Fail(sqlite3_errmsg(db_.Handle()));
	}
	if (sqlite3_changes(db_.Handle()) == 0) {
		return Result::Fail("UpdateLogicNode: logic node id=" + std::to_string(nodeId) + " が存在しない");
	}

	return tx.Commit();
}

Result TaskStore::AddLogicEdge(std::int64_t fromId, std::int64_t toId, const std::string& relation) {
	std::lock_guard<std::mutex> lock(mutex_);
	Transaction tx(db_);

	Statement stmt;
	Result r = db_.Prepare(
		"INSERT OR IGNORE INTO LogicEdge(from_id, to_id, relation) VALUES(?1, ?2, ?3);", &stmt);
	if (!r) {
		return r;
	}
	stmt.BindInt64(1, fromId);
	stmt.BindInt64(2, toId);
	stmt.BindText(3, relation);
	if (stmt.Step() == Statement::StepResult::Error) {
		return Result::Fail(sqlite3_errmsg(db_.Handle()));
	}

	return tx.Commit();
}

// ---------------------------------
// Command
// ---------------------------------
Result TaskStore::RecordCommand(TaskId taskId, const std::string& issuer, const std::string& tool,
                                 const Json& args, const std::string& validationStatus,
                                 const std::string& executionStatus, const Json& result) {
	std::lock_guard<std::mutex> lock(mutex_);
	Transaction tx(db_);

	Statement stmt;
	Result r = db_.Prepare(
		"INSERT INTO Command(task_id, issuer, tool, arguments_json, validation_status, execution_status, result_json) "
		"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7);",
		&stmt);
	if (!r) {
		return r;
	}
	if (taskId == kInvalidId) {
		stmt.BindNull(1);
	} else {
		stmt.BindInt64(1, taskId);
	}
	stmt.BindText(2, issuer);
	stmt.BindText(3, tool);
	stmt.BindText(4, args.dump());
	stmt.BindText(5, validationStatus);
	stmt.BindText(6, executionStatus);
	stmt.BindText(7, result.dump());
	if (stmt.Step() == Statement::StepResult::Error) {
		return Result::Fail(sqlite3_errmsg(db_.Handle()));
	}

	return tx.Commit();
}

// ---------------------------------
// Summary
// ---------------------------------
Json TaskStore::GetSessionSummary(SessionId sessionId) {
	std::lock_guard<std::mutex> lock(mutex_);

	Json summary = Json::object();
	Json tasksByState = Json::object();

	Statement taskStmt;
	Result r = db_.Prepare("SELECT state, COUNT(*) FROM Task WHERE session_id=?1 GROUP BY state;", &taskStmt);
	if (r) {
		taskStmt.BindInt64(1, sessionId);
		while (taskStmt.Step() == Statement::StepResult::Row) {
			tasksByState[taskStmt.ColumnText(0)] = taskStmt.ColumnInt64(1);
		}
	}
	summary["tasksByState"] = tasksByState;

	std::int64_t evidenceCount = 0;
	Statement evidenceStmt;
	r = db_.Prepare(
		"SELECT COUNT(*) FROM Evidence JOIN Task ON Evidence.task_id = Task.id WHERE Task.session_id=?1;",
		&evidenceStmt);
	if (r) {
		evidenceStmt.BindInt64(1, sessionId);
		if (evidenceStmt.Step() == Statement::StepResult::Row) {
			evidenceCount = evidenceStmt.ColumnInt64(0);
		}
	}
	summary["evidenceCount"] = evidenceCount;

	std::int64_t commandCount = 0;
	Statement commandStmt;
	r = db_.Prepare(
		"SELECT COUNT(*) FROM Command JOIN Task ON Command.task_id = Task.id WHERE Task.session_id=?1;",
		&commandStmt);
	if (r) {
		commandStmt.BindInt64(1, sessionId);
		if (commandStmt.Step() == Statement::StepResult::Row) {
			commandCount = commandStmt.ColumnInt64(0);
		}
	}
	summary["commandCount"] = commandCount;

	return summary;
}

} // namespace agentos
