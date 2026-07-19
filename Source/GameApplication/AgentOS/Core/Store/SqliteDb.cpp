// =======================================================================
//
// SqliteDb.cpp
//
// =======================================================================
#include "SqliteDb.h"

#include <utility>

namespace agentos {

// ---------------------------------
// Statement
// ---------------------------------
Statement::Statement(sqlite3_stmt* stmt, sqlite3* db)
	: stmt_(stmt), db_(db) {
}

Statement::~Statement() {
	Destroy();
}

Statement::Statement(Statement&& other) noexcept
	: stmt_(other.stmt_), db_(other.db_) {
	other.stmt_ = nullptr;
	other.db_ = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
	if (this != &other) {
		Destroy();
		stmt_ = other.stmt_;
		db_ = other.db_;
		other.stmt_ = nullptr;
		other.db_ = nullptr;
	}
	return *this;
}

void Statement::Destroy() {
	if (stmt_ != nullptr) {
		sqlite3_finalize(stmt_);
		stmt_ = nullptr;
	}
	db_ = nullptr;
}

Result Statement::BindInt64(int index, std::int64_t value) {
	if (stmt_ == nullptr) {
		return Result::Fail("Statement::BindInt64: 未初期化のStatement");
	}
	int rc = sqlite3_bind_int64(stmt_, index, static_cast<sqlite3_int64>(value));
	if (rc != SQLITE_OK) {
		return Result::Fail(sqlite3_errmsg(db_));
	}
	return Result::Ok();
}

Result Statement::BindDouble(int index, double value) {
	if (stmt_ == nullptr) {
		return Result::Fail("Statement::BindDouble: 未初期化のStatement");
	}
	int rc = sqlite3_bind_double(stmt_, index, value);
	if (rc != SQLITE_OK) {
		return Result::Fail(sqlite3_errmsg(db_));
	}
	return Result::Ok();
}

Result Statement::BindText(int index, const std::string& value) {
	if (stmt_ == nullptr) {
		return Result::Fail("Statement::BindText: 未初期化のStatement");
	}
	int rc = sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
	if (rc != SQLITE_OK) {
		return Result::Fail(sqlite3_errmsg(db_));
	}
	return Result::Ok();
}

Result Statement::BindNull(int index) {
	if (stmt_ == nullptr) {
		return Result::Fail("Statement::BindNull: 未初期化のStatement");
	}
	int rc = sqlite3_bind_null(stmt_, index);
	if (rc != SQLITE_OK) {
		return Result::Fail(sqlite3_errmsg(db_));
	}
	return Result::Ok();
}

Statement::StepResult Statement::Step() {
	if (stmt_ == nullptr) {
		return StepResult::Error;
	}
	int rc = sqlite3_step(stmt_);
	if (rc == SQLITE_ROW) {
		return StepResult::Row;
	}
	if (rc == SQLITE_DONE) {
		return StepResult::Done;
	}
	return StepResult::Error;
}

Result Statement::Reset() {
	if (stmt_ == nullptr) {
		return Result::Fail("Statement::Reset: 未初期化のStatement");
	}
	int rc = sqlite3_reset(stmt_);
	sqlite3_clear_bindings(stmt_);
	if (rc != SQLITE_OK) {
		return Result::Fail(sqlite3_errmsg(db_));
	}
	return Result::Ok();
}

std::int64_t Statement::ColumnInt64(int index) const {
	if (stmt_ == nullptr) {
		return 0;
	}
	return static_cast<std::int64_t>(sqlite3_column_int64(stmt_, index));
}

double Statement::ColumnDouble(int index) const {
	if (stmt_ == nullptr) {
		return 0.0;
	}
	return sqlite3_column_double(stmt_, index);
}

std::string Statement::ColumnText(int index) const {
	if (stmt_ == nullptr) {
		return {};
	}
	const unsigned char* text = sqlite3_column_text(stmt_, index);
	if (text == nullptr) {
		return {};
	}
	int bytes = sqlite3_column_bytes(stmt_, index);
	return std::string(reinterpret_cast<const char*>(text), static_cast<std::size_t>(bytes));
}

bool Statement::ColumnIsNull(int index) const {
	if (stmt_ == nullptr) {
		return true;
	}
	return sqlite3_column_type(stmt_, index) == SQLITE_NULL;
}

// ---------------------------------
// SqliteDb
// ---------------------------------
SqliteDb::~SqliteDb() {
	Close();
}

Result SqliteDb::Open(const std::string& path) {
	if (db_ != nullptr) {
		Close();
	}

	int rc = sqlite3_open_v2(
		path.c_str(),
		&db_,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
		nullptr);
	if (rc != SQLITE_OK) {
		std::string message = db_ != nullptr ? sqlite3_errmsg(db_) : "sqlite3_open_v2失敗";
		if (db_ != nullptr) {
			sqlite3_close(db_);
			db_ = nullptr;
		}
		return Result::Fail(message);
	}

	sqlite3_busy_timeout(db_, 5000);

	Result r = Exec("PRAGMA journal_mode=WAL;");
	if (!r) {
		Close();
		return r;
	}

	r = Exec("PRAGMA busy_timeout=5000;");
	if (!r) {
		Close();
		return r;
	}

	r = Exec("PRAGMA foreign_keys=ON;");
	if (!r) {
		Close();
		return r;
	}

	return Result::Ok();
}

void SqliteDb::Close() {
	if (db_ != nullptr) {
		sqlite3_close(db_);
		db_ = nullptr;
	}
}

Result SqliteDb::Exec(const std::string& sql) {
	if (db_ == nullptr) {
		return Result::Fail("SqliteDb::Exec: DB未オープン");
	}
	char* errorMessage = nullptr;
	int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errorMessage);
	if (rc != SQLITE_OK) {
		std::string message = errorMessage != nullptr ? errorMessage : "sqlite3_exec失敗";
		if (errorMessage != nullptr) {
			sqlite3_free(errorMessage);
		}
		return Result::Fail(message);
	}
	return Result::Ok();
}

Result SqliteDb::Prepare(const std::string& sql, Statement* outStatement) {
	if (db_ == nullptr) {
		return Result::Fail("SqliteDb::Prepare: DB未オープン");
	}
	if (outStatement == nullptr) {
		return Result::Fail("SqliteDb::Prepare: outStatementがnullptr");
	}
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		return Result::Fail(sqlite3_errmsg(db_));
	}
	*outStatement = Statement(stmt, db_);
	return Result::Ok();
}

std::int64_t SqliteDb::LastInsertRowId() const {
	if (db_ == nullptr) {
		return 0;
	}
	return static_cast<std::int64_t>(sqlite3_last_insert_rowid(db_));
}

// ---------------------------------
// Transaction
// ---------------------------------
Transaction::Transaction(SqliteDb& db) : db_(db) {
	Begin();
}

Transaction::~Transaction() {
	if (active_ && !committed_) {
		db_.Exec("ROLLBACK;");
	}
}

Result Transaction::Begin() {
	Result r = db_.Exec("BEGIN IMMEDIATE;");
	if (r) {
		active_ = true;
		committed_ = false;
	}
	return r;
}

Result Transaction::Commit() {
	if (!active_) {
		return Result::Fail("Transaction::Commit: トランザクション未開始");
	}
	Result r = db_.Exec("COMMIT;");
	if (r) {
		committed_ = true;
		active_ = false;
	}
	return r;
}

} // namespace agentos
