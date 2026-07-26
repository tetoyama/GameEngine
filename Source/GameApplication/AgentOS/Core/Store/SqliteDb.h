// =======================================================================
//
// SqliteDb.h
//
// sqlite3への薄いRAIIラッパ。AgentOS Core全体のSQLiteアクセスはこの層を経由する。
// 例外は投げない。エラーはResultとして返す（構想§8）。
//
// =======================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../AgentOsTypes.h"

#include "../../../Backends/sqlite/sqlite3.h"

namespace agentos {

// ---------------------------------
// Statement
// sqlite3_stmtのRAIIラッパ。Move-only。
// ---------------------------------
class Statement {
public:
	enum class StepResult {
		Row,
		Done,
		Error,
	};

	Statement() = default;
	~Statement();

	Statement(const Statement&) = delete;
	Statement& operator=(const Statement&) = delete;

	Statement(Statement&& other) noexcept;
	Statement& operator=(Statement&& other) noexcept;

	bool IsValid() const noexcept { return stmt_ != nullptr; }

	// 1-based index
	Result BindInt64(int index, std::int64_t value);
	Result BindDouble(int index, double value);
	Result BindText(int index, const std::string& value);
	Result BindNull(int index);

	// バイナリ列。埋め込みベクトル（float配列）の格納に使う。
	// data はSQLite側へコピーされるため、呼び出し後に解放してよい。
	Result BindBlob(int index, const void* data, std::size_t bytes);

	StepResult Step();
	Result Reset();

	// column getters（0-based index）。NULLは各型の既定値を返す。
	std::int64_t ColumnInt64(int index) const;
	double ColumnDouble(int index) const;
	std::string ColumnText(int index) const;
	bool ColumnIsNull(int index) const;

	// バイナリ列を取り出す。NULLや型不一致では空を返す。
	std::vector<std::uint8_t> ColumnBlob(int index) const;

private:
	friend class SqliteDb;

	explicit Statement(sqlite3_stmt* stmt, sqlite3* db);
	void Destroy();

	sqlite3_stmt* stmt_ = nullptr;
	sqlite3* db_ = nullptr; // エラーメッセージ取得用（所有はしない）
};

// ---------------------------------
// SqliteDb
// sqlite3*のRAIIラッパ。
// ---------------------------------
class SqliteDb {
public:
	SqliteDb() = default;
	~SqliteDb();

	SqliteDb(const SqliteDb&) = delete;
	SqliteDb& operator=(const SqliteDb&) = delete;

	Result Open(const std::string& path);
	void Close();

	bool IsOpen() const noexcept { return db_ != nullptr; }

	Result Exec(const std::string& sql);

	Result Prepare(const std::string& sql, Statement* outStatement);

	std::int64_t LastInsertRowId() const;

	sqlite3* Handle() const noexcept { return db_; }

private:
	sqlite3* db_ = nullptr;
};

// ---------------------------------
// Transaction
// BEGIN IMMEDIATEでctor、dtorでCommitされていなければROLLBACK。
// ---------------------------------
class Transaction {
public:
	explicit Transaction(SqliteDb& db);
	~Transaction();

	Transaction(const Transaction&) = delete;
	Transaction& operator=(const Transaction&) = delete;

	Result Begin();
	Result Commit();

private:
	SqliteDb& db_;
	bool active_ = false;
	bool committed_ = false;
};

} // namespace agentos
