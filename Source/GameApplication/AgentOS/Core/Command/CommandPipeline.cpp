// =======================================================================
//
// CommandPipeline.cpp
//
// =======================================================================
#include "CommandPipeline.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <typeinfo>
#include <vector>

#include "CommandSchema.h"
#include "../Llm/PromptTemplates.h"

namespace agentos {

namespace {

// CHANGE 2: Tool別の既知argument alias補正 + Schemaベースの大文字小文字補正。
//
// argumentSchemaが分かっている場合（Tool解決後の2回目呼び出し）は、それに加えて
// 「Schema上の正式キーと大文字小文字だけが異なるキー」を機械的に補正する
// （例: "EntityName" -> "entityName"）。Tool固有テーブルに載っていない
// 未知Toolでも、Schemaさえあれば単純な大文字小文字揺れは自動で救済される。
void NormalizeKnownArgumentAliases(CommandRequest* request, const Json& argumentSchema = Json()) {
	if(request == nullptr || !request->arguments.is_object()) return;
	Json& arguments = request->arguments;
	auto moveStringAlias = [&arguments](const char* alias, const char* canonical) {
		if(!arguments.contains(alias) || !arguments.at(alias).is_string()) return;
		if(!arguments.contains(canonical)) arguments[canonical] = arguments.at(alias);
		arguments.erase(alias);
	};
	auto moveAnyAlias = [&arguments](const char* alias, const char* canonical) {
		if(!arguments.contains(alias)) return;
		if(!arguments.contains(canonical)) arguments[canonical] = arguments.at(alias);
		arguments.erase(alias);
	};

	if(request->tool == "ReadComponent" || request->tool == "StartWriteTrace") {
		moveStringAlias("entityId", "entityName");
		moveStringAlias("name", "entityName");
		moveStringAlias("componentName", "component");
		moveStringAlias("componentType", "component");
	} else if(request->tool == "DescribeEntity") {
		moveStringAlias("name", "entityName");
	} else if(request->tool == "FindEntityByName") {
		moveStringAlias("entityName", "name");
	} else if(request->tool == "ListEntities") {
		moveAnyAlias("max", "maxCount");
		moveAnyAlias("count", "maxCount");
		moveAnyAlias("limit", "maxCount");
	} else if(request->tool == "FindWriters" || request->tool == "FindReaders") {
		moveStringAlias("componentName", "component");
		moveStringAlias("componentType", "component");
	}

	if (!argumentSchema.is_object()) return;

	auto lowerAscii = [](std::string value) {
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
			return static_cast<char>(std::tolower(ch));
		});
		return value;
	};

	std::vector<std::string> currentKeys;
	for (auto it = arguments.begin(); it != arguments.end(); ++it) currentKeys.push_back(it.key());
	for (const std::string& key : currentKeys) {
		if (argumentSchema.contains(key)) continue; // 既に正式キー
		const std::string lowerKey = lowerAscii(key);
		for (const auto& schemaItem : argumentSchema.items()) {
			if (schemaItem.key() != key && lowerAscii(schemaItem.key()) == lowerKey) {
				if (!arguments.contains(schemaItem.key())) {
					arguments[schemaItem.key()] = arguments.at(key);
				}
				arguments.erase(key);
				break;
			}
		}
	}
}

} // namespace

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
	NormalizeKnownArgumentAliases(&request);

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
	// Tool解決後、実Schemaを使った大文字小文字補正パスをもう一度通す
	// （Tool固有テーブルは1回目の呼び出しで既に適用済みなので冪等）。
	NormalizeKnownArgumentAliases(&request, descriptor.argumentSchema);

	Result schemaResult = SchemaValidator::Validate(request.arguments, descriptor.argumentSchema);
	if(!schemaResult){
		CommandResult result = CommandResult::Fail(CommandStatus::SchemaRejected, schemaResult.error);
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
