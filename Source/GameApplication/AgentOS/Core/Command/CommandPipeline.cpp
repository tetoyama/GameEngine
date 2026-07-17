// =======================================================================
//
// CommandPipeline.cpp
//
// =======================================================================
#include "CommandPipeline.h"

#include "CommandSchema.h"

namespace agentos {

CommandPipeline::CommandPipeline(CapabilityRegistry* capabilityRegistry, CommandPipelineConfig config)
	: capabilityRegistry_(capabilityRegistry), config_(config) {}

void CommandPipeline::RegisterTool(std::shared_ptr<ICommandExecutor> tool) {
	std::lock_guard<std::mutex> lock(mutex_);
	tools_[tool->Descriptor().name] = std::move(tool);
}

void CommandPipeline::AddAuditSink(std::shared_ptr<IAuditSink> sink) {
	std::lock_guard<std::mutex> lock(mutex_);
	auditSinks_.push_back(std::move(sink));
}

void CommandPipeline::SetApprovalHandler(std::function<bool(const CommandRequest&)> handler) {
	std::lock_guard<std::mutex> lock(mutex_);
	approvalHandler_ = std::move(handler);
}

void CommandPipeline::SetBudgetTracker(BudgetTracker* budgetTracker) {
	std::lock_guard<std::mutex> lock(mutex_);
	budgetTracker_ = budgetTracker;
}

void CommandPipeline::Audit(const CommandRequest& request, const CommandResult& result) {
	std::vector<std::shared_ptr<IAuditSink>> sinksCopy;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auditLog_.emplace_back(request, result);
		sinksCopy = auditSinks_;
	}
	for (auto& sink : sinksCopy) {
		sink->OnCommand(request, result);
	}
}

CommandResult CommandPipeline::Submit(CommandRequest request) {
	if (request.id == kInvalidId) {
		request.id = nextCommandId_.fetch_add(1);
	}

	// --- Tool検索 ---
	std::shared_ptr<ICommandExecutor> tool;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = tools_.find(request.tool);
		if (it != tools_.end()) {
			tool = it->second;
		}
	}
	if (!tool) {
		CommandResult result = CommandResult::Fail(CommandStatus::ExecutionFailed, "unknown tool: " + request.tool);
		Audit(request, result);
		return result;
	}

	const ToolDescriptor& descriptor = tool->Descriptor();

	// --- Schema検証 ---
	Result schemaResult = SchemaValidator::Validate(request.arguments, descriptor.argumentSchema);
	if (!schemaResult) {
		CommandResult result = CommandResult::Fail(CommandStatus::SchemaRejected, schemaResult.error);
		Audit(request, result);
		return result;
	}

	// --- Capability検証 ---
	if (capabilityRegistry_) {
		Result capabilityResult = capabilityRegistry_->Validate(request.capability, request.tool, descriptor.requiredPermission);
		if (!capabilityResult) {
			CommandResult result = CommandResult::Fail(CommandStatus::CapabilityRejected, capabilityResult.error);
			Audit(request, result);
			return result;
		}
	}

	// --- Budget消費（すべての検証を通過した後、実行前に消費する） ---
	BudgetTracker* budgetTracker = nullptr;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		budgetTracker = budgetTracker_;
	}
	if (budgetTracker) {
		Result budgetResult = budgetTracker->ConsumeToolCall();
		if (!budgetResult) {
			CommandResult result = CommandResult::Fail(CommandStatus::BudgetRejected, budgetResult.error);
			Audit(request, result);
			return result;
		}
	}

	// --- Approval Gate ---
	if (static_cast<std::uint8_t>(descriptor.requiredPermission) >=
	    static_cast<std::uint8_t>(config_.approvalRequiredAtOrAbove)) {
		std::function<bool(const CommandRequest&)> handler;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			handler = approvalHandler_;
		}
		const bool approved = handler ? handler(request) : false;
		if (!approved) {
			CommandResult result = CommandResult::Fail(CommandStatus::AwaitingApproval, "awaiting human approval");
			Audit(request, result);
			return result;
		}
	}

	// --- Precondition ---
	Result preconditionResult = tool->CheckPrecondition(request.arguments);
	if (!preconditionResult) {
		CommandResult result = CommandResult::Fail(CommandStatus::PreconditionRejected, preconditionResult.error);
		Audit(request, result);
		return result;
	}

	// --- Dry Run（Preconditionまでで停止） ---
	if (request.dryRun) {
		Json payload = Json::object();
		payload["dryRun"] = true;
		CommandResult result = CommandResult::Ok(payload);
		Audit(request, result);
		return result;
	}

	// --- Execute ---
	CommandResult result = tool->Execute(request.arguments);
	Audit(request, result);
	return result;
}

std::vector<std::pair<CommandRequest, CommandResult>> CommandPipeline::GetAuditLog() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return auditLog_;
}

Json CommandPipeline::DescribeTools() const {
	std::lock_guard<std::mutex> lock(mutex_);
	Json array = Json::array();
	for (const auto& [name, tool] : tools_) {
		const ToolDescriptor& descriptor = tool->Descriptor();
		Json entry = Json::object();
		entry["name"] = descriptor.name;
		entry["description"] = descriptor.description;
		entry["requiredPermission"] = ToString(descriptor.requiredPermission);
		entry["argumentSchema"] = descriptor.argumentSchema;
		array.push_back(std::move(entry));
	}
	return array;
}

} // namespace agentos
