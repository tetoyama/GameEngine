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

// ---------------------------------
// 監査シンク
// Submit()の結果（成否問わず）を受け取る。SQLite永続化・UI表示等はこれを実装する。
// ---------------------------------
class IAuditSink {
public:
	virtual ~IAuditSink() = default;
	virtual void OnCommand(const CommandRequest& request, const CommandResult& result) = 0;
};

// ---------------------------------
// Pipeline設定
// ---------------------------------
struct CommandPipelineConfig {
	// このレベル以上を要求するToolはHuman Approval Gateを通す。
	PermissionLevel approvalRequiredAtOrAbove = PermissionLevel::Modify;
};

// ---------------------------------
// CommandPipeline
// スレッドセーフ。Tool登録・Submitともに複数スレッドから呼んでよい。
// ---------------------------------
class CommandPipeline {
public:
	explicit CommandPipeline(CapabilityRegistry* capabilityRegistry, CommandPipelineConfig config = {});

	// Descriptor().nameをキーに登録する。同名Toolの再登録は置き換える。
	void RegisterTool(std::shared_ptr<ICommandExecutor> tool);

	void AddAuditSink(std::shared_ptr<IAuditSink> sink);

	// trueを返せば承認、falseまたは未設定ならAwaitingApprovalのまま保留する。
	void SetApprovalHandler(std::function<bool(const CommandRequest&)> handler);

	// nullptrを渡せばBudget検査自体を無効化できる（省略可能）。
	void SetBudgetTracker(BudgetTracker* budgetTracker);

	CommandResult Submit(CommandRequest request);

	// 内蔵の監査ログ（AuditSinkとは別に常に記録される）を返す。
	std::vector<std::pair<CommandRequest, CommandResult>> GetAuditLog() const;

	// 登録済みTool一覧を {name, description, requiredPermission, argumentSchema} の配列で返す。
	// LLMへTool catalogを提示する用途。
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
