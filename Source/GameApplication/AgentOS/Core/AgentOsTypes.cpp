// =======================================================================
//
// AgentOsTypes.cpp
//
// =======================================================================
#include "AgentOsTypes.h"

namespace agentos {

const char* ToString(PermissionLevel level) noexcept {
	switch (level) {
	case PermissionLevel::Read:       return "Read";
	case PermissionLevel::Observe:    return "Observe";
	case PermissionLevel::RunControl: return "RunControl";
	case PermissionLevel::Modify:     return "Modify";
	}
	return "Unknown";
}

const char* ToString(CommandStatus status) noexcept {
	switch (status) {
	case CommandStatus::Ok:                   return "Ok";
	case CommandStatus::SchemaRejected:       return "SchemaRejected";
	case CommandStatus::CapabilityRejected:   return "CapabilityRejected";
	case CommandStatus::BudgetRejected:       return "BudgetRejected";
	case CommandStatus::PreconditionRejected: return "PreconditionRejected";
	case CommandStatus::ExecutionFailed:      return "ExecutionFailed";
	case CommandStatus::PostconditionFailed:  return "PostconditionFailed";
	case CommandStatus::AwaitingApproval:     return "AwaitingApproval";
	}
	return "Unknown";
}

const char* ToString(TaskState state) noexcept {
	switch (state) {
	case TaskState::Pending:          return "Pending";
	case TaskState::Running:          return "Running";
	case TaskState::Succeeded:        return "Succeeded";
	case TaskState::Failed:           return "Failed";
	case TaskState::Cancelled:        return "Cancelled";
	case TaskState::AwaitingApproval: return "AwaitingApproval";
	}
	return "Unknown";
}

bool IsTerminal(TaskState state) noexcept {
	return state == TaskState::Succeeded ||
	       state == TaskState::Failed ||
	       state == TaskState::Cancelled;
}

// Pending → Running → (Succeeded | Failed | Cancelled | AwaitingApproval)
// AwaitingApproval → Running（承認） | Cancelled（却下）
// Pending → Cancelled（実行前キャンセル）
bool IsLegalTransition(TaskState from, TaskState to) noexcept {
	switch (from) {
	case TaskState::Pending:
		return to == TaskState::Running || to == TaskState::Cancelled;
	case TaskState::Running:
		return to == TaskState::Succeeded || to == TaskState::Failed ||
		       to == TaskState::Cancelled || to == TaskState::AwaitingApproval;
	case TaskState::AwaitingApproval:
		return to == TaskState::Running || to == TaskState::Cancelled;
	case TaskState::Succeeded:
	case TaskState::Failed:
	case TaskState::Cancelled:
		return false; // 終端状態からの遷移は禁止
	}
	return false;
}

} // namespace agentos
