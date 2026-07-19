// =======================================================================
//
// CommandPipeline.h
//
// LLM出力（Command提案）を実行に至るまで通す検証パイプライン本体（構想§3, §5）。
// 順序: Tool検索 → Schema検証 → Capability検証 → Budget消費 → Approval Gate →
//        Precondition → (DryRunならここで停止) → Execute → Audit
// すべての結果（拒否含む）をAuditSinkへ流す。
//
// =======================================================================
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../AgentOsTypes.h"
#include "../Json.h"
#include "../Budget/Budget.h"
#include "CapabilitySet.h"
#include "CommandTypes.h"

namespace agentos {

class IAuditSink {
public:
	virtual ~IAuditSink() = default;
	virtual void OnCommand(const CommandRequest& request, const CommandResult& result) = 0;
};

struct CommandPipelineConfig {
	PermissionLevel approvalRequiredAtOrAbove = PermissionLevel::Modify;
};

class CommandPipeline {
public:
	explicit CommandPipeline(CapabilityRegistry* capabilityRegistry, CommandPipelineConfig config = {});

	void RegisterTool(std::shared_ptr<ICommandExecutor> tool);

	// 同じ動的型のSinkは置換する。Sessionごとに同じSQLite Sinkが追加され続けて
	// Commandが二重・三重記録されることを防ぐ。
	void AddAuditSink(std::shared_ptr<IAuditSink> sink);
	bool HasAuditSinks() const;

	void SetApprovalHandler(std::function<bool(const CommandRequest&)> handler);
	void SetBudgetTracker(BudgetTracker* budgetTracker);

	CommandResult Submit(CommandRequest request);

	std::vector<std::pair<CommandRequest, CommandResult>> GetAuditLog() const;
	Json DescribeTools() const;

private:
	void Audit(const CommandRequest& request, const CommandResult& result);

	CapabilityRegistry* capabilityRegistry_ = nullptr;
	CommandPipelineConfig config_;
	BudgetTracker* budgetTracker_ = nullptr;
	std::function<bool(const CommandRequest&)> approvalHandler_;

	mutable std::mutex mutex_;
	std::unordered_map<ToolName, std::shared_ptr<ICommandExecutor>> tools_;
	std::vector<std::shared_ptr<IAuditSink>> auditSinks_;
	std::vector<std::pair<CommandRequest, CommandResult>> auditLog_;

	std::atomic<CommandId> nextCommandId_{1};
};

} // namespace agentos
