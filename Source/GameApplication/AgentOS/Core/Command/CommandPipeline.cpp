// =======================================================================
//
// CommandPipeline.cpp
//
// =======================================================================
#include "CommandPipeline.h"

#include <typeinfo>

#include "CommandSchema.h"
#include "../Llm/PromptTemplates.h"

namespace agentos {

CommandPipeline::CommandPipeline(CapabilityRegistry* capabilityRegistry, CommandPipelineConfig config)
	: capabilityRegistry_(capabilityRegistry), config_(config) {}

void CommandPipeline::RegisterTool(std::shared_ptr<ICommandExecutor> tool) {
	if(!tool) return;
	std::lock_guard<std::mutex> lock(mutex_);
	tools_[tool->Descriptor().name] = std::move(tool);
}

void CommandPipeline::AddAuditSink(std::shared_ptr<IAuditSink> sink) {
	if(!sink) return;
	std::lock_guard<std::mutex> lock(mutex_);

	for(auto& existing : auditSinks_){
		if(existing && typeid(*existing) == typeid(*sink)){
			existing = std::move(sink);
			return;
		}
	}
	auditSinks_.push_back(std::move(sink));
}

bool CommandPipeline::HasAuditSinks() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return !auditSinks_.empty();
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
	for(auto& sink : sinksCopy){
		if(sink) sink->OnCommand(request, result);
	}
}

CommandResult CommandPipeline::Submit(CommandRequest request) {
	if(request.id == kInvalidId){
		request.id = nextCommandId_.fetch_add(1);
	}

	std::shared_ptr<ICommandExecutor> tool;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = tools_.find(request.tool);
		if(it != tools_.end()) tool = it->second;
	}
	if(!tool){
		CommandResult result = CommandResult::Fail(
			CommandStatus::ExecutionFailed,
			"unknown tool: " + request.tool
		);
		Audit(request, result);
		return result;
	}

	const ToolDescriptor& descriptor = tool->Descriptor();

	Result schemaResult = SchemaValidator::Validate(request.arguments, descriptor.argumentSchema);
	if(!schemaResult){
		CommandResult result = CommandResult::Fail(CommandStatus::SchemaRejected, schemaResult.error);
		Audit(request, result);
		return result;
	}

	// 会話上の「私は誰」「あなたは誰」をScene Entity探索へ変換することを、
	// DirectReplyのプロンプトだけでなく実行境界でも拒否する。
	if(request.issuer == "QuickPath" && prompts::CurrentRequestIsPersonalIdentityQuestion()){
		CommandResult result = CommandResult::Fail(
			CommandStatus::PreconditionRejected,
			"personal conversation request cannot be executed as an Engine Tool"
		);
		Audit(request, result);
		return result;
	}

	if(capabilityRegistry_){
		Result capabilityResult = capabilityRegistry_->Validate(
			request.capability,
			request.tool,
			descriptor.requiredPermission
		);
		if(!capabilityResult){
			CommandResult result = CommandResult::Fail(
				CommandStatus::CapabilityRejected,
				capabilityResult.error
			);
			Audit(request, result);
			return result;
		}
	}

	BudgetTracker* budgetTracker = nullptr;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		budgetTracker = budgetTracker_;
	}
	if(budgetTracker){
		Result budgetResult = budgetTracker->ConsumeToolCall();
		if(!budgetResult){
			CommandResult result = CommandResult::Fail(CommandStatus::BudgetRejected, budgetResult.error);
			Audit(request, result);
			return result;
		}
	}

	if(static_cast<std::uint8_t>(descriptor.requiredPermission) >=
	   static_cast<std::uint8_t>(config_.approvalRequiredAtOrAbove)){
		std::function<bool(const CommandRequest&)> handler;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			handler = approvalHandler_;
		}
		const bool approved = handler ? handler(request) : false;
		if(!approved){
			CommandResult result = CommandResult::Fail(
				CommandStatus::AwaitingApproval,
				"awaiting human approval"
			);
			Audit(request, result);
			return result;
		}
	}

	Result preconditionResult = tool->CheckPrecondition(request.arguments);
	if(!preconditionResult){
		CommandResult result = CommandResult::Fail(
			CommandStatus::PreconditionRejected,
			preconditionResult.error
		);
		Audit(request, result);
		return result;
	}

	if(request.dryRun){
		Json payload = Json::object();
		payload["dryRun"] = true;
		CommandResult result = CommandResult::Ok(payload);
		Audit(request, result);
		return result;
	}

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
	for(const auto& [name, tool] : tools_){
		(void)name;
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
