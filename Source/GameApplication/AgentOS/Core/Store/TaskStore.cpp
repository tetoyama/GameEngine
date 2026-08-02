// =======================================================================
//
// TaskStore.cpp
//
// =======================================================================
#include "TaskStore.h"

#include <cctype>
#include <string>
#include <vector>

namespace agentos {

namespace {

static TaskState ParseTaskState(const std::string& text) {
	if (text == "Pending")          return TaskState::Pending;
	if (text == "Running")          return TaskState::Running;
	if (text == "Succeeded")        return TaskState::Succeeded;
	if (text == "Failed")           return TaskState::Failed;
	if (text == "Cancelled")        return TaskState::Cancelled;
	if (text == "AwaitingApproval") return TaskState::AwaitingApproval;
	return TaskState::Pending;
}

std::string ExtractUserText(const Json& goal) {
	if (goal.is_object() && goal.contains("userRequest") &&
	    goal.at("userRequest").is_string()) {
		return goal.at("userRequest").get<std::string>();
	}
	return goal.dump();
}

std::string ExtractFinalResponse(const Json& result) {
	if (!result.is_object()) {
		return {};
	}
	if (result.contains("report") && result.at("report").is_string() &&
	    !result.at("report").get<std::string>().empty()) {
		return result.at("report").get<std::string>();
	}
	if (result.contains("reply") && result.at("reply").is_string() &&
	    !result.at("reply").get<std::string>().empty()) {
		return result.at("reply").get<std::string>();
	}
	return {};
}

} // namespace

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
		// このEvidenceが最終応答へ反映されたか（仮説のsupportsに挙がったか）。
		// 失敗Evidenceを次のセッションへ引き継ぐかの判定に使う。
		// 反映された失敗は「前回これは失敗した」という知識なので残し、
		// 反映されなかった失敗は同じ探索を繰り返させるだけなので捨てる。
		"  reflected INTEGER NOT NULL DEFAULT 0,"
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

		// user入力と最終assistant応答を1 Session = 1 Turnとして原文保存する。
		"CREATE TABLE IF NOT EXISTS ConversationTurn("
		"  session_id INTEGER PRIMARY KEY REFERENCES Session(id),"
		"  user_text TEXT NOT NULL,"
		"  assistant_text TEXT NOT NULL DEFAULT '',"
		"  created_at TEXT NOT NULL DEFAULT (datetime('now')),"
		"  updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
		");"

		// Promptへ渡すのはRaw assistant本文ではなく、Intakeが確定した構造化状態だけ。
		"CREATE TABLE IF NOT EXISTS ConversationThreadState("
		"  session_id INTEGER PRIMARY KEY REFERENCES Session(id),"
		"  state_json TEXT NOT NULL DEFAULT '{}',"
		"  created_at TEXT NOT NULL DEFAULT (datetime('now')),"
		"  updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
		");"

		// 古いTurnの累積要約は監査・移行互換として保持する。通常Promptへは渡さない。
		"CREATE TABLE IF NOT EXISTS ConversationMemory("
		"  id INTEGER PRIMARY KEY CHECK(id=1),"
		"  summary_text TEXT NOT NULL DEFAULT '',"
		"  summarized_through_session_id INTEGER NOT NULL DEFAULT 0,"
		"  updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
		");"
		"INSERT OR IGNORE INTO ConversationMemory(id, summary_text, summarized_through_session_id) "
		"VALUES(1, '', 0);"

		"CREATE INDEX IF NOT EXISTS idx_task_session_id ON Task(session_id);"
		"CREATE INDEX IF NOT EXISTS idx_task_parent_id ON Task(parent_id);"
		"CREATE INDEX IF NOT EXISTS idx_evidence_task_id ON Evidence(task_id);"
		"CREATE INDEX IF NOT EXISTS idx_command_task_id ON Command(task_id);"
		"CREATE INDEX IF NOT EXISTS idx_conversation_turn_session_id ON ConversationTurn(session_id);"
		"CREATE INDEX IF NOT EXISTS idx_conversation_thread_state_session_id "
		"ON ConversationThreadState(session_id);";

	Result schemaResult = db_.Exec(kSchemaSql);
	if (!schemaResult) return schemaResult;

	// 既存DBへの列追加。CREATE TABLE IF NOT EXISTS は既存テーブルを変更しないため、
	// 後から足した列はここで補う。既に在る場合のエラーは無視してよい。
	if (!HasColumn("Evidence", "reflected")) {
		(void)db_.Exec("ALTER TABLE Evidence ADD COLUMN reflected INTEGER NOT NULL DEFAULT 0;");
	}
	return Result::Ok();
}

bool TaskStore::HasColumn(const std::string& table, const std::string& column) {
	Statement stmt;
	if (!db_.Prepare("SELECT COUNT(*) FROM pragma_table_info(?1) WHERE name=?2;", &stmt)) {
		return false;
	}
	stmt.BindText(1, table);
	stmt.BindText(2, column);
	if (stmt.Step() != Statement::StepResult::Row) return false;
	return stmt.ColumnInt64(0) > 0;
}

Result TaskStore::MarkEvidenceReflected(const std::vector<EvidenceId>& evidenceIds) {
	if (evidenceIds.empty()) return Result::Ok();
	std::lock_guard<std::mutex> lock(mutex_);
	Transaction tx(db_);

	Statement stmt;
	Result r = db_.Prepare("UPDATE Evidence SET reflected=1 WHERE id=?1;", &stmt);
	if (!r) return r;
	for (const EvidenceId id : evidenceIds) {
		stmt.Reset();
		stmt.BindInt64(1, id);
		if (stmt.Step() == Statement::StepResult::Error) {
			return Result::Fail("MarkEvidenceReflected: update failed");
		}
	}
	return tx.Commit();
}

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

	const SessionId id = db_.LastInsertRowId();
	Statement turn;
	r = db_.Prepare(
		"INSERT INTO ConversationTurn(session_id, user_text, assistant_text) VALUES(?1, ?2, '');",
		&turn);
	if (!r) {
		return kInvalidId;
	}
	turn.BindInt64(1, id);
	turn.BindText(2, ExtractUserText(goal));
	if (turn.Step() == Statement::StepResult::Error) {
		return kInvalidId;
	}

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

Result TaskStore::SetConversationResponseLocked(
	SessionId sessionId,
	const std::string& assistantText) {

	if (assistantText.empty()) {
		return Result::Fail("SetConversationResponse: assistantText is empty");
	}

	Statement stmt;
	Result r = db_.Prepare(
		"UPDATE ConversationTurn SET assistant_text=?1, updated_at=datetime('now') WHERE session_id=?2;",
		&stmt);
	if (!r) {
		return r;
	}
	stmt.BindText(1, assistantText);
	stmt.BindInt64(2, sessionId);
	if (stmt.Step() == Statement::StepResult::Error) {
		return Result::Fail(sqlite3_errmsg(db_.Handle()));
	}
	if (sqlite3_changes(db_.Handle()) == 0) {
		return Result::Fail(
			"SetConversationResponse: session id=" + std::to_string(sessionId) + " が存在しない");
	}
	return Result::Ok();
}

Result TaskStore::SetConversationResponse(
	SessionId sessionId,
	const std::string& assistantText) {

	std::lock_guard<std::mutex> lock(mutex_);
	Transaction tx(db_);
	Result r = SetConversationResponseLocked(sessionId, assistantText);
	if (!r) {
		return r;
	}
	return tx.Commit();
}


Result TaskStore::SetConversationThreadState(SessionId sessionId, const Json& state) {
	if (!state.is_object() || state.empty()) {
		return Result::Fail("SetConversationThreadState: state must be a non-empty object");
	}

	std::lock_guard<std::mutex> lock(mutex_);
	Transaction tx(db_);
	Statement stmt;
	Result r = db_.Prepare(
		"INSERT INTO ConversationThreadState(session_id, state_json) VALUES(?1, ?2) "
		"ON CONFLICT(session_id) DO UPDATE SET state_json=excluded.state_json, "
		"updated_at=datetime('now');",
		&stmt);
	if (!r) return r;
	stmt.BindInt64(1, sessionId);
	stmt.BindText(2, state.dump());
	if (stmt.Step() == Statement::StepResult::Error) {
		return Result::Fail(sqlite3_errmsg(db_.Handle()));
	}
	return tx.Commit();
}

Json TaskStore::GetConversationContext(SessionId beforeSessionId) {
	std::lock_guard<std::mutex> lock(mutex_);

	Json context = Json::object();
	std::string summary;
	SessionId summarizedThrough = kInvalidId;

	Statement memory;
	Result r = db_.Prepare(
		"SELECT summary_text, summarized_through_session_id FROM ConversationMemory WHERE id=1;",
		&memory);
	if (r && memory.Step() == Statement::StepResult::Row) {
		summary = memory.ColumnText(0);
		summarizedThrough = memory.ColumnInt64(1);
	}

	std::int64_t totalTurns = 0;
	Statement count;
	r = db_.Prepare(
		"SELECT COUNT(*) FROM ConversationTurn "
		"WHERE session_id < ?1 AND assistant_text <> '';",
		&count);
	if (r) {
		count.BindInt64(1, beforeSessionId);
		if (count.Step() == Statement::StepResult::Row) {
			totalTurns = count.ColumnInt64(0);
		}
	}

	Json recentTurns = Json::array();
	Statement turns;
	r = db_.Prepare(
		"SELECT session_id, user_text, assistant_text FROM ConversationTurn "
		"WHERE session_id > ?1 AND session_id < ?2 AND assistant_text <> '' "
		"ORDER BY session_id;",
		&turns);
	if (r) {
		turns.BindInt64(1, summarizedThrough);
		turns.BindInt64(2, beforeSessionId);
		while (turns.Step() == Statement::StepResult::Row) {
			recentTurns.push_back(Json::object({
				{"sessionId", turns.ColumnInt64(0)},
				{"user", turns.ColumnText(1)},
				{"assistant", turns.ColumnText(2)},
			}));
		}
	}

	Json threadStates = Json::array();
	std::vector<Json> reversedStates;
	Statement states;
	r = db_.Prepare(
		"SELECT session_id, state_json FROM ConversationThreadState "
		"WHERE session_id < ?1 ORDER BY session_id DESC LIMIT 8;",
		&states);
	if (r) {
		states.BindInt64(1, beforeSessionId);
		while (states.Step() == Statement::StepResult::Row) {
			Json state = Json::parse(states.ColumnText(1), nullptr, false);
			if (!state.is_object() || state.is_discarded()) continue;
			state["sessionId"] = states.ColumnInt64(0);
			reversedStates.push_back(std::move(state));
		}
	}
	for (auto it = reversedStates.rbegin(); it != reversedStates.rend(); ++it) {
		threadStates.push_back(*it);
	}

	context["summary"] = summary;
	context["summarizedThroughSessionId"] = summarizedThrough;
	context["recentTurns"] = std::move(recentTurns);
	context["threadStates"] = std::move(threadStates);
	context["totalTurns"] = totalTurns;
	return context;
}

namespace {

bool WantsKind(const std::vector<std::string>& kinds, const char* kind) {
	if (kinds.empty()) return true; // 指定なしは全種別
	for (const std::string& k : kinds) {
		if (k == kind) return true;
	}
	return false;
}

std::string LowerAscii(const std::string& s) {
	std::string out = s;
	for (char& c : out) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return out;
}

// queryが空なら常に一致。ASCIIは大小無視、非ASCIIはそのまま部分一致。
bool Matches(const std::string& haystack, const std::string& loweredQuery) {
	if (loweredQuery.empty()) return true;
	return LowerAscii(haystack).find(loweredQuery) != std::string::npos;
}

} // namespace

Json TaskStore::SearchConversationHistory(
	SessionId beforeSessionId,
	const std::string& query,
	const std::vector<std::string>& kinds,
	int limitPerKind) {

	std::lock_guard<std::mutex> lock(mutex_);
	Json entries = Json::array();
	if (limitPerKind <= 0) limitPerKind = 5;
	const std::string lowered = LowerAscii(query);

	// --- 過去Evidence（観測） ---
	// 新しいセッションから遡る。同一セッション内はEvidence順。
	if (WantsKind(kinds, "evidence")) {
		Statement stmt;
		Result r = db_.Prepare(
			"SELECT Task.session_id, Evidence.source_type, Evidence.source_uri, "
			"Evidence.claim, Evidence.payload_json, Evidence.confidence, Evidence.reflected "
			"FROM Evidence JOIN Task ON Evidence.task_id = Task.id "
			"WHERE Task.session_id < ?1 ORDER BY Task.session_id DESC, Evidence.id DESC;",
			&stmt);
		if (r) {
			stmt.BindInt64(1, beforeSessionId);
			int taken = 0;
			while (taken < limitPerKind && stmt.Step() == Statement::StepResult::Row) {
				const std::string claim = stmt.ColumnText(3);
				const std::string payloadText = stmt.ColumnText(4);
				if (!Matches(claim, lowered) && !Matches(payloadText, lowered)) continue;
				Json payload = Json::parse(payloadText, nullptr, false);
				if (payload.is_discarded()) payload = Json::object();

				// 最終応答へ反映されなかった失敗Evidenceは引き継がない（宣言の注記参照）。
				// 判定基準はEvidenceBuilderのcoverage計算と同じものを使う。
				Evidence probe;
				probe.provenance.sourceType = stmt.ColumnText(1);
				probe.payload = payload;
				const bool reflected = stmt.ColumnInt64(6) != 0;
				if (!reflected && IsFailureEvidence(probe)) continue;

				entries.push_back(Json::object({
					{"kind", "evidence"},
					{"sessionId", stmt.ColumnInt64(0)},
					{"originSourceType", stmt.ColumnText(1)},
					{"originSourceUri", stmt.ColumnText(2)},
					{"claim", claim},
					{"payload", std::move(payload)},
					{"confidence", stmt.ColumnDouble(5)},
				}));
				++taken;
			}
		}
	}

	// --- ユーザ発話 / Agent応答 ---
	const bool wantUser = WantsKind(kinds, "userTurn");
	const bool wantAssistant = WantsKind(kinds, "assistantTurn");
	if (wantUser || wantAssistant) {
		Statement stmt;
		Result r = db_.Prepare(
			"SELECT session_id, user_text, assistant_text FROM ConversationTurn "
			"WHERE session_id < ?1 ORDER BY session_id DESC;",
			&stmt);
		if (r) {
			stmt.BindInt64(1, beforeSessionId);
			int takenUser = 0;
			int takenAssistant = 0;
			while ((takenUser < limitPerKind || takenAssistant < limitPerKind) &&
			       stmt.Step() == Statement::StepResult::Row) {
				const std::int64_t session = stmt.ColumnInt64(0);
				const std::string userText = stmt.ColumnText(1);
				const std::string assistantText = stmt.ColumnText(2);
				if (wantUser && takenUser < limitPerKind &&
				    !userText.empty() && Matches(userText, lowered)) {
					entries.push_back(Json::object({
						{"kind", "userTurn"}, {"sessionId", session}, {"text", userText},
					}));
					++takenUser;
				}
				if (wantAssistant && takenAssistant < limitPerKind &&
				    !assistantText.empty() && Matches(assistantText, lowered)) {
					entries.push_back(Json::object({
						{"kind", "assistantTurn"}, {"sessionId", session}, {"text", assistantText},
					}));
					++takenAssistant;
				}
			}
		}
	}

	// --- Thread State（Intakeが確定させた構造化要約） ---
	if (WantsKind(kinds, "threadState")) {
		Statement stmt;
		Result r = db_.Prepare(
			"SELECT session_id, state_json FROM ConversationThreadState "
			"WHERE session_id < ?1 ORDER BY session_id DESC;",
			&stmt);
		if (r) {
			stmt.BindInt64(1, beforeSessionId);
			int taken = 0;
			while (taken < limitPerKind && stmt.Step() == Statement::StepResult::Row) {
				const std::string stateText = stmt.ColumnText(1);
				if (!Matches(stateText, lowered)) continue;
				Json state = Json::parse(stateText, nullptr, false);
				if (!state.is_object() || state.is_discarded()) continue;
				entries.push_back(Json::object({
					{"kind", "threadState"},
					{"sessionId", stmt.ColumnInt64(0)},
					{"state", std::move(state)},
				}));
				++taken;
			}
		}
	}

	return entries;
}

Result TaskStore::UpdateConversationSummary(
	const std::string& summary,
	SessionId summarizedThroughSessionId) {

	if (summary.empty()) {
		return Result::Fail("UpdateConversationSummary: summary is empty");
	}

	std::lock_guard<std::mutex> lock(mutex_);
	Transaction tx(db_);

	Statement current;
	Result r = db_.Prepare(
		"SELECT summarized_through_session_id FROM ConversationMemory WHERE id=1;",
		&current);
	if (!r) {
		return r;
	}
	SessionId currentThrough = kInvalidId;
	if (current.Step() == Statement::StepResult::Row) {
		currentThrough = current.ColumnInt64(0);
	}
	if (summarizedThroughSessionId < currentThrough) {
		return Result::Fail("UpdateConversationSummary: summary cursor cannot move backwards");
	}

	Statement update;
	r = db_.Prepare(
		"UPDATE ConversationMemory SET summary_text=?1, summarized_through_session_id=?2, "
		"updated_at=datetime('now') WHERE id=1;",
		&update);
	if (!r) {
		return r;
	}
	update.BindText(1, summary);
	update.BindInt64(2, summarizedThroughSessionId);
	if (update.Step() == Statement::StepResult::Error) {
		return Result::Fail(sqlite3_errmsg(db_.Handle()));
	}
	return tx.Commit();
}

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

	const TaskId id = db_.LastInsertRowId();
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
	const TaskState currentState = ParseTaskState(select.ColumnText(0));

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

	SessionId sessionId = kInvalidId;
	Statement task;
	Result r = db_.Prepare("SELECT session_id FROM Task WHERE id=?1;", &task);
	if (!r) {
		return r;
	}
	task.BindInt64(1, taskId);
	if (task.Step() != Statement::StepResult::Row) {
		return Result::Fail("SetTaskResult: task id=" + std::to_string(taskId) + " が存在しない");
	}
	sessionId = task.ColumnInt64(0);

	Statement stmt;
	r = db_.Prepare("UPDATE Task SET result_json=?1, updated_at=datetime('now') WHERE id=?2;", &stmt);
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

	const std::string finalResponse = ExtractFinalResponse(result);
	if (!finalResponse.empty()) {
		r = SetConversationResponseLocked(sessionId, finalResponse);
		if (!r) {
			return r;
		}
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
		Json parsed = Json::parse(stmt.ColumnText(8), nullptr, false);
		row.result = parsed.is_discarded() ? Json::object() : parsed;
	}
	return row;
}

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

	const EvidenceId id = db_.LastInsertRowId();
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

	const LogicNodeId id = db_.LastInsertRowId();
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
