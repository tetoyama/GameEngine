// =======================================================================
//
// AgentOsTypes.h
//
// AgentOS Core共通の型定義。
// Core層はエンジン・Windows APIに依存しない（Linux g++でもビルド可能）。
//
// =======================================================================
#pragma once

#include <cstdint>
#include <string>

namespace agentos {

// ---------------------------------
// ID型
// ---------------------------------
using CommandId   = std::int64_t;
using TaskId      = std::int64_t;
using EvidenceId  = std::int64_t;
using LogicNodeId = std::int64_t;
using SessionId   = std::int64_t;

using AgentId  = std::string;   // 例: "PlannerAgent", "RuntimeWorker#1"
using ToolName = std::string;   // 例: "ReadComponent"

inline constexpr std::int64_t kInvalidId = 0;

// ---------------------------------
// 権限レベル（低い順）
// Toolはそれぞれ要求レベルを宣言し、CapabilitySetが照合する。
// ---------------------------------
enum class PermissionLevel : std::uint8_t {
	Read       = 0,  // 静的情報・状態の読み取り
	Observe    = 1,  // Trace開始・Frame実行など観測系
	RunControl = 2,  // Scene Load / Play制御 / テスト実行
	Modify     = 3,  // Component書換・Entity生成・コード変更
};

const char* ToString(PermissionLevel level) noexcept;

// ---------------------------------
// Command検証・実行の結果ステータス
// ---------------------------------
enum class CommandStatus : std::uint8_t {
	Ok = 0,
	SchemaRejected,        // 引数スキーマ違反
	CapabilityRejected,    // 権限不足・Tool未許可
	BudgetRejected,        // Budget超過
	PreconditionRejected,  // 実行前条件を満たさない
	ExecutionFailed,       // 実行中の失敗
	PostconditionFailed,   // 実行後検証の失敗
	AwaitingApproval,      // Human Approval Gateで保留中
};

const char* ToString(CommandStatus status) noexcept;

// ---------------------------------
// Task状態機械
// 遷移はSupervisorのみが行う。
// Pending → Running → (Succeeded | Failed | Cancelled | AwaitingApproval)
// ---------------------------------
enum class TaskState : std::uint8_t {
	Pending = 0,
	Running,
	Succeeded,
	Failed,
	Cancelled,
	AwaitingApproval,
};

const char* ToString(TaskState state) noexcept;
bool IsTerminal(TaskState state) noexcept;
bool IsLegalTransition(TaskState from, TaskState to) noexcept;

// ---------------------------------
// 汎用Result
// ---------------------------------
struct Result {
	bool ok = true;
	std::string error;

	static Result Ok() { return {true, {}}; }
	static Result Fail(std::string message) { return {false, std::move(message)}; }
	explicit operator bool() const noexcept { return ok; }
};

} // namespace agentos
