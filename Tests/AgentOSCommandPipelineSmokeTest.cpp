// =======================================================================
//
// AgentOSCommandPipelineSmokeTest.cpp
//
// CommandPipeline / SchemaValidator / CapabilityRegistry / BudgetTracker /
// AgentOsTypes(IsLegalTransition)の結合スモークテスト。
// 自己完結main()+assert方式（テストフレームワーク非使用、エンジン規約準拠）。
//
// =======================================================================
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "AgentOS/Core/AgentOsTypes.h"
#include "AgentOS/Core/Json.h"
#include "AgentOS/Core/Budget/Budget.h"
#include "AgentOS/Core/Command/CapabilitySet.h"
#include "AgentOS/Core/Command/CommandPipeline.h"
#include "AgentOS/Core/Command/CommandSchema.h"
#include "AgentOS/Core/Command/CommandTypes.h"

using namespace agentos;

namespace {

// ---------------------------------
// Fakeツール: Read権限のEcho
// ---------------------------------
class FakeEchoTool : public ICommandExecutor {
public:
	FakeEchoTool() {
		descriptor_.name = "Echo";
		descriptor_.description = "引数のmessageをそのまま返すテスト用Tool";
		descriptor_.requiredPermission = PermissionLevel::Read;

		Json schema = Json::object();

		Json messageSpec = Json::object();
		messageSpec["type"] = "string";
		messageSpec["required"] = true;
		messageSpec["maxLength"] = 100;
		schema["message"] = messageSpec;

		Json countSpec = Json::object();
		countSpec["type"] = "integer";
		countSpec["required"] = false;
		countSpec["min"] = 1;
		countSpec["max"] = 10;
		schema["count"] = countSpec;

		Json modeSpec = Json::object();
		modeSpec["type"] = "string";
		modeSpec["required"] = false;
		modeSpec["enum"] = Json::array({"a", "b", "c"});
		schema["mode"] = modeSpec;

		descriptor_.argumentSchema = schema;
	}

	const ToolDescriptor& Descriptor() const override { return descriptor_; }

	Result CheckPrecondition(const Json&) override { return Result::Ok(); }

	CommandResult Execute(const Json& arguments) override {
		++executeCount;
		Json payload = Json::object();
		payload["echoed"] = arguments.value("message", std::string());
		return CommandResult::Ok(payload);
	}

	int executeCount = 0;

private:
	ToolDescriptor descriptor_;
};

// ---------------------------------
// Fakeツール: Modify権限（Approval Gate対象）
// ---------------------------------
class FakeModifyTool : public ICommandExecutor {
public:
	FakeModifyTool() {
		descriptor_.name = "ModifyThing";
		descriptor_.description = "値を書き換えるテスト用Tool（要Modify権限）";
		descriptor_.requiredPermission = PermissionLevel::Modify;

		Json schema = Json::object();
		Json valueSpec = Json::object();
		valueSpec["type"] = "integer";
		valueSpec["required"] = true;
		schema["value"] = valueSpec;
		descriptor_.argumentSchema = schema;
	}

	const ToolDescriptor& Descriptor() const override { return descriptor_; }

	Result CheckPrecondition(const Json&) override { return Result::Ok(); }

	CommandResult Execute(const Json& arguments) override {
		++executeCount;
		Json payload = Json::object();
		payload["applied"] = true;
		payload["value"] = arguments.value("value", 0);
		return CommandResult::Ok(payload);
	}

	int executeCount = 0;

private:
	ToolDescriptor descriptor_;
};

class RecordingAuditSink : public IAuditSink {
public:
	void OnCommand(const CommandRequest& request, const CommandResult& result) override {
		records.emplace_back(request, result);
	}
	std::vector<std::pair<CommandRequest, CommandResult>> records;
};

CommandRequest MakeRequest(const ToolName& tool, const Json& arguments, const CapabilityToken& token, bool dryRun = false) {
	CommandRequest request;
	request.issuer = token.owner;
	request.tool = tool;
	request.arguments = arguments;
	request.capability = token;
	request.dryRun = dryRun;
	return request;
}

void TestSchemaValidatorDirect() {
	Json schema = Json::object();
	Json field = Json::object();
	field["type"] = "integer";
	field["required"] = true;
	field["min"] = 0;
	field["max"] = 5;
	schema["n"] = field;

	Json ok = Json::object();
	ok["n"] = 3;
	assert(SchemaValidator::Validate(ok, schema).ok);

	Json outOfRange = Json::object();
	outOfRange["n"] = 6;
	assert(!SchemaValidator::Validate(outOfRange, schema).ok);

	Json missing = Json::object();
	assert(!SchemaValidator::Validate(missing, schema).ok);

	Json unknown = Json::object();
	unknown["n"] = 1;
	unknown["extra"] = true;
	assert(!SchemaValidator::Validate(unknown, schema).ok);
}

void TestBudgetTrackerDirect() {
	Budget budget;
	budget.maxRetries = 1;
	budget.maxModifiedFiles = 1;
	budget.maxDepth = 2;
	BudgetTracker tracker(budget);

	assert(tracker.ConsumeRetry().ok);
	assert(!tracker.ConsumeRetry().ok); // 上限超過

	assert(tracker.ConsumeModifiedFile().ok);
	assert(!tracker.ConsumeModifiedFile().ok);

	assert(tracker.CheckDepth(2).ok);
	assert(!tracker.CheckDepth(3).ok);

	tracker.AddElapsedMillis(100);
	double ratio = tracker.RemainingRatio();
	assert(ratio >= 0.0 && ratio <= 1.0);

	Json j = tracker.ToJson();
	assert(j.contains("toolCalls"));
	assert(j.contains("retries"));
}

void TestIsLegalTransitionEdgeCases() {
	assert(IsLegalTransition(TaskState::Pending, TaskState::Running));
	assert(IsLegalTransition(TaskState::Pending, TaskState::Cancelled));
	assert(!IsLegalTransition(TaskState::Pending, TaskState::Succeeded));
	assert(!IsLegalTransition(TaskState::Pending, TaskState::Failed));
	assert(!IsLegalTransition(TaskState::Pending, TaskState::AwaitingApproval));

	assert(IsLegalTransition(TaskState::Running, TaskState::Succeeded));
	assert(IsLegalTransition(TaskState::Running, TaskState::Failed));
	assert(IsLegalTransition(TaskState::Running, TaskState::Cancelled));
	assert(IsLegalTransition(TaskState::Running, TaskState::AwaitingApproval));
	assert(!IsLegalTransition(TaskState::Running, TaskState::Pending));

	assert(IsLegalTransition(TaskState::AwaitingApproval, TaskState::Running));
	assert(IsLegalTransition(TaskState::AwaitingApproval, TaskState::Cancelled));
	assert(!IsLegalTransition(TaskState::AwaitingApproval, TaskState::Succeeded));
	assert(!IsLegalTransition(TaskState::AwaitingApproval, TaskState::Failed));

	assert(!IsLegalTransition(TaskState::Succeeded, TaskState::Running));
	assert(!IsLegalTransition(TaskState::Failed, TaskState::Running));
	assert(!IsLegalTransition(TaskState::Cancelled, TaskState::Running));

	assert(IsTerminal(TaskState::Succeeded));
	assert(IsTerminal(TaskState::Failed));
	assert(IsTerminal(TaskState::Cancelled));
	assert(!IsTerminal(TaskState::Pending));
	assert(!IsTerminal(TaskState::Running));
	assert(!IsTerminal(TaskState::AwaitingApproval));
}

} // namespace

int main() {
	TestSchemaValidatorDirect();
	TestBudgetTrackerDirect();
	TestIsLegalTransitionEdgeCases();

	CapabilityRegistry registry;
	CommandPipeline pipeline(&registry);

	auto echoTool = std::make_shared<FakeEchoTool>();
	auto modifyTool = std::make_shared<FakeModifyTool>();
	pipeline.RegisterTool(echoTool);
	pipeline.RegisterTool(modifyTool);

	auto sink = std::make_shared<RecordingAuditSink>();
	pipeline.AddAuditSink(sink);

	CapabilityToken echoOnlyToken = registry.IssueToken("Agent1", {"Echo"}, PermissionLevel::Read);
	CapabilityToken allToolsModifyToken = registry.IssueToken("Agent1", {"*"}, PermissionLevel::Modify);
	CapabilityToken allToolsReadOnlyToken = registry.IssueToken("Agent1", {"*"}, PermissionLevel::Read);

	std::size_t expectedAuditCount = 0;
	auto assertAuditCount = [&]() {
		assert(pipeline.GetAuditLog().size() == expectedAuditCount);
		assert(sink->records.size() == expectedAuditCount);
	};

	// 1. ok path
	{
		Json args = Json::object();
		args["message"] = "hello";
		CommandResult result = pipeline.Submit(MakeRequest("Echo", args, echoOnlyToken));
		++expectedAuditCount;
		assert(result.IsOk());
		assert(result.payload["echoed"] == "hello");
		assertAuditCount();
	}

	// 2. unknown field rejected
	{
		Json args = Json::object();
		args["message"] = "hi";
		args["extra"] = 1;
		CommandResult result = pipeline.Submit(MakeRequest("Echo", args, echoOnlyToken));
		++expectedAuditCount;
		assert(result.status == CommandStatus::SchemaRejected);
		assertAuditCount();
	}

	// 3. missing required rejected
	{
		Json args = Json::object();
		CommandResult result = pipeline.Submit(MakeRequest("Echo", args, echoOnlyToken));
		++expectedAuditCount;
		assert(result.status == CommandStatus::SchemaRejected);
		assertAuditCount();
	}

	// 4. type mismatch rejected
	{
		Json args = Json::object();
		args["message"] = 123;
		CommandResult result = pipeline.Submit(MakeRequest("Echo", args, echoOnlyToken));
		++expectedAuditCount;
		assert(result.status == CommandStatus::SchemaRejected);
		assertAuditCount();
	}

	// 5. enum violation
	{
		Json args = Json::object();
		args["message"] = "hi";
		args["mode"] = "z";
		CommandResult result = pipeline.Submit(MakeRequest("Echo", args, echoOnlyToken));
		++expectedAuditCount;
		assert(result.status == CommandStatus::SchemaRejected);
		assertAuditCount();
	}

	// 6. unknown tool
	{
		Json args = Json::object();
		CommandResult result = pipeline.Submit(MakeRequest("NoSuchTool", args, echoOnlyToken));
		++expectedAuditCount;
		assert(result.status == CommandStatus::ExecutionFailed);
		assertAuditCount();
	}

	// 7a. capability rejection: tool not in allowlist
	{
		Json args = Json::object();
		args["value"] = 1;
		CommandResult result = pipeline.Submit(MakeRequest("ModifyThing", args, echoOnlyToken));
		++expectedAuditCount;
		assert(result.status == CommandStatus::CapabilityRejected);
		assertAuditCount();
	}

	// 7b. capability rejection: insufficient permission level
	{
		Json args = Json::object();
		args["value"] = 1;
		CommandResult result = pipeline.Submit(MakeRequest("ModifyThing", args, allToolsReadOnlyToken));
		++expectedAuditCount;
		assert(result.status == CommandStatus::CapabilityRejected);
		assertAuditCount();
	}

	// 7c. capability rejection: tampered token secret is rejected outright
	{
		CapabilityToken tampered = echoOnlyToken;
		tampered.secret = "not-a-real-secret";
		Json args = Json::object();
		args["message"] = "hi";
		CommandResult result = pipeline.Submit(MakeRequest("Echo", args, tampered));
		++expectedAuditCount;
		assert(result.status == CommandStatus::CapabilityRejected);
		assertAuditCount();
	}

	// 8. budget exhaustion after N calls (別Pipelineで隔離)
	{
		CapabilityRegistry budgetRegistry;
		CommandPipeline budgetPipeline(&budgetRegistry);
		budgetPipeline.RegisterTool(echoTool);

		Budget smallBudget;
		smallBudget.maxToolCalls = 2;
		BudgetTracker tracker(smallBudget);
		budgetPipeline.SetBudgetTracker(&tracker);

		CapabilityToken token = budgetRegistry.IssueToken("Agent2", {"Echo"}, PermissionLevel::Read);

		for (int i = 0; i < 2; ++i) {
			Json args = Json::object();
			args["message"] = "ok";
			CommandResult result = budgetPipeline.Submit(MakeRequest("Echo", args, token));
			assert(result.IsOk());
		}

		Json args = Json::object();
		args["message"] = "over";
		CommandResult result = budgetPipeline.Submit(MakeRequest("Echo", args, token));
		assert(result.status == CommandStatus::BudgetRejected);
		assert(budgetPipeline.GetAuditLog().size() == 3);
	}

	// 9. dry run does not execute
	{
		const int before = echoTool->executeCount;
		Json args = Json::object();
		args["message"] = "dry";
		CommandResult result = pipeline.Submit(MakeRequest("Echo", args, echoOnlyToken, /*dryRun=*/true));
		++expectedAuditCount;
		assert(result.IsOk());
		assert(result.payload["dryRun"] == true);
		assert(echoTool->executeCount == before); // 実行されていない
		assertAuditCount();
	}

	// 10a. approval gate: ハンドラ未設定 → AwaitingApproval
	{
		Json args = Json::object();
		args["value"] = 5;
		CommandResult result = pipeline.Submit(MakeRequest("ModifyThing", args, allToolsModifyToken));
		++expectedAuditCount;
		assert(result.status == CommandStatus::AwaitingApproval);
		assert(modifyTool->executeCount == 0);
		assertAuditCount();
	}

	// 10b. approval gate: ハンドラがfalse → AwaitingApproval（保留のまま）
	{
		pipeline.SetApprovalHandler([](const CommandRequest&) { return false; });
		Json args = Json::object();
		args["value"] = 6;
		CommandResult result = pipeline.Submit(MakeRequest("ModifyThing", args, allToolsModifyToken));
		++expectedAuditCount;
		assert(result.status == CommandStatus::AwaitingApproval);
		assert(modifyTool->executeCount == 0);
		assertAuditCount();
	}

	// 10c. approval gate: ハンドラがtrue → 実行される
	{
		pipeline.SetApprovalHandler([](const CommandRequest&) { return true; });
		Json args = Json::object();
		args["value"] = 7;
		CommandResult result = pipeline.Submit(MakeRequest("ModifyThing", args, allToolsModifyToken));
		++expectedAuditCount;
		assert(result.IsOk());
		assert(result.payload["value"] == 7);
		assert(modifyTool->executeCount == 1);
		assertAuditCount();
	}

	// DescribeTools: 登録済み2Toolが列挙される
	{
		Json described = pipeline.DescribeTools();
		assert(described.is_array());
		assert(described.size() == 2);
		for (const auto& entry : described) {
			assert(entry.contains("name"));
			assert(entry.contains("description"));
			assert(entry.contains("requiredPermission"));
			assert(entry.contains("argumentSchema"));
		}
	}

	std::cout << "AgentOSCommandPipelineSmokeTest: OK" << std::endl;
	return 0;
}
