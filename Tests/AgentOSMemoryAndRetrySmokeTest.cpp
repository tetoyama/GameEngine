// =======================================================================
//
// AgentOSMemoryAndRetrySmokeTest.cpp
//
// 2つの既知欠陥に対する回帰テスト:
// (1) Intake LLMが過去turn由来の失敗記述をsymptoms/constraintsへ混入させる
//     Memory汚染 -> Grounding filterでmemoryDerivedNotesへ隔離されること。
// (2) SchemaRejectedで即座に失敗記録・打ち切りされていたRetrievalWorkerが、
//     正しいargumentSchemaを渡した1回だけの自己修復リトライで復帰すること。
//
// =======================================================================
#include "AgentOS/Core/Agents/AgentContext.h"
#include "AgentOS/Core/Agents/IntakeAgent.h"
#include "AgentOS/Core/Agents/RetrievalWorker.h"
#include "AgentOS/Core/Budget/Budget.h"
#include "AgentOS/Core/Command/CapabilitySet.h"
#include "AgentOS/Core/Command/CommandPipeline.h"
#include "AgentOS/Core/Command/CommandTypes.h"
#include "AgentOS/Core/Evidence/Evidence.h"
#include "AgentOS/Core/Llm/MockLlmBackend.h"
#include "AgentOS/Core/Llm/PromptTemplates.h"
#include "AgentOS/Core/Store/TaskStore.h"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace agentos;

namespace {

const char* kDbPath = "/tmp/agentos_memory_and_retry_test.db";

void RemoveDb() {
	std::remove(kDbPath);
	std::remove((std::string(kDbPath) + "-wal").c_str());
	std::remove((std::string(kDbPath) + "-shm").c_str());
	std::remove((std::string(kDbPath) + "-journal").c_str());
}

// ---------------------------------
// Fake Tools
// ---------------------------------

class FakeStartWriteTraceTool final : public ICommandExecutor {
public:
	FakeStartWriteTraceTool() {
		descriptor_.name = "StartWriteTrace";
		descriptor_.description = "test write trace";
		descriptor_.requiredPermission = PermissionLevel::Observe;
		descriptor_.argumentSchema = Json::object({
			{"entityName", Json::object({{"type", "string"}, {"required", true}})},
			{"component", Json::object({{"type", "string"}, {"required", true}})},
		});
	}
	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }
	CommandResult Execute(const Json& arguments) override { return CommandResult::Ok(arguments); }
private:
	ToolDescriptor descriptor_;
};

class FakeDescribeEntityTool final : public ICommandExecutor {
public:
	FakeDescribeEntityTool() {
		descriptor_.name = "DescribeEntity";
		descriptor_.description = "test describe entity";
		descriptor_.requiredPermission = PermissionLevel::Read;
		descriptor_.argumentSchema = Json::object({
			{"entityName", Json::object({{"type", "string"}, {"required", true}})},
		});
	}
	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }
	CommandResult Execute(const Json& arguments) override { return CommandResult::Ok(arguments); }
private:
	ToolDescriptor descriptor_;
};

class FakeListEntitiesTool final : public ICommandExecutor {
public:
	FakeListEntitiesTool() {
		descriptor_.name = "ListEntities";
		descriptor_.description = "test list entities";
		descriptor_.requiredPermission = PermissionLevel::Read;
		descriptor_.argumentSchema = Json::object({
			{"maxCount", Json::object({{"type", "integer"}, {"required", false}})},
		});
	}
	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }
	CommandResult Execute(const Json& arguments) override { return CommandResult::Ok(arguments); }
private:
	ToolDescriptor descriptor_;
};

class FakeReadComponentTool final : public ICommandExecutor {
public:
	FakeReadComponentTool() {
		descriptor_.name = "ReadComponent";
		descriptor_.description = "test read component";
		descriptor_.requiredPermission = PermissionLevel::Read;
		descriptor_.argumentSchema = Json::object({
			{"entityName", Json::object({{"type", "string"}, {"required", true}})},
			{"component", Json::object({{"type", "string"}, {"required", true}})},
		});
	}
	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }
	CommandResult Execute(const Json& arguments) override {
		++executeCount;
		return CommandResult::Ok(arguments);
	}
	int executeCount = 0;
private:
	ToolDescriptor descriptor_;
};

// ---------------------------------
// CHANGE 1: Intake Memory Grounding filter
// ---------------------------------

void TestIntakeMemoryGroundingFilter() {
	MockLlmBackend llm;
	llm.AddRule(
		"Intake担当",
		"```json\n"
		"{\"goal\":\"Playerのジャンプ力を確認する\","
		"\"resolvedRequest\":\"Playerのジャンプ力を確認する\","
		"\"turnRelation\":\"new\",\"referencedSessionIds\":[],"
		"\"symptoms\":[\"Playerのコンポーネント構成が未確認\","
		"\"ReadComponent ツール実行失敗（不明なフィールド componentName）\"],"
		"\"constraints\":[\"ツール利用に許可エラーが発生しており調査が難航している\"],"
		"\"requiredCapabilities\":[\"ReadComponent\"],"
		"\"unresolvedReferences\":[],\"requestType\":\"investigation\"}\n"
		"```");

	AgentContext ctx;
	ctx.llm = &llm;

	Json intake;
	assert(IntakeAgent::Run(ctx, "Playerのジャンプ力を教えて", &intake));

	// (a) 現在turnに根拠がある記述はsymptomsに残る。
	bool foundGrounded = false;
	for (const Json& value : intake.at("symptoms")) {
		if (value.get<std::string>() == "Playerのコンポーネント構成が未確認") foundGrounded = true;
	}
	assert(foundGrounded);

	// (b) 現在turnに根拠がない失敗語彙混じりの記述はsymptomsから排除される。
	for (const Json& value : intake.at("symptoms")) {
		assert(value.get<std::string>().find("ツール実行失敗") == std::string::npos);
	}
	for (const Json& value : intake.at("constraints")) {
		assert(value.get<std::string>().find("許可エラー") == std::string::npos);
	}

	// 排除された記述はmemoryDerivedNotesへ隔離され、デバッグ用に見える状態を保つ。
	assert(intake.contains("memoryDerivedNotes"));
	bool foundInMemoryNotes = false;
	bool foundConstraintInMemoryNotes = false;
	for (const Json& value : intake.at("memoryDerivedNotes")) {
		const std::string text = value.get<std::string>();
		if (text.find("ReadComponent ツール実行失敗") != std::string::npos) foundInMemoryNotes = true;
		if (text.find("許可エラー") != std::string::npos) foundConstraintInMemoryNotes = true;
	}
	assert(foundInMemoryNotes);
	assert(foundConstraintInMemoryNotes);

	prompts::ClearCurrentConversationRequestContext();
	std::puts("  - intake memory grounding filter isolates ungrounded failure vocabulary: OK");
}

// ---------------------------------
// CHANGE 2: Argument alias extension
// ---------------------------------

void TestStartWriteTraceGetsReadComponentStyleAliases() {
	CapabilityRegistry registry;
	CommandPipeline pipeline(&registry);
	pipeline.RegisterTool(std::make_shared<FakeStartWriteTraceTool>());
	CapabilityToken token = registry.IssueToken("Tester", {"StartWriteTrace"}, PermissionLevel::Observe);

	CommandRequest request;
	request.issuer = "Tester";
	request.tool = "StartWriteTrace";
	request.arguments = Json::object({
		{"entityId", "Boss"},
		{"componentName", "Velocity"},
	});
	request.capability = token;

	const CommandResult result = pipeline.Submit(request);
	assert(result.IsOk());
	assert(result.payload.at("entityName") == "Boss");
	assert(result.payload.at("component") == "Velocity");
	assert(!result.payload.contains("entityId"));
	assert(!result.payload.contains("componentName"));
	std::puts("  - StartWriteTrace gets ReadComponent-style aliases: OK");
}

void TestGenericCaseInsensitiveKeyFix() {
	CapabilityRegistry registry;
	CommandPipeline pipeline(&registry);
	pipeline.RegisterTool(std::make_shared<FakeDescribeEntityTool>());
	CapabilityToken token = registry.IssueToken("Tester", {"DescribeEntity"}, PermissionLevel::Read);

	CommandRequest request;
	request.issuer = "Tester";
	request.tool = "DescribeEntity";
	request.arguments = Json::object({{"EntityName", "Boss"}}); // 大文字小文字だけ違う
	request.capability = token;

	const CommandResult result = pipeline.Submit(request);
	assert(result.IsOk());
	assert(result.payload.at("entityName") == "Boss");
	assert(!result.payload.contains("EntityName"));
	std::puts("  - generic case-insensitive schema key fix: OK");
}

void TestListEntitiesCountAliases() {
	CapabilityRegistry registry;
	CommandPipeline pipeline(&registry);
	pipeline.RegisterTool(std::make_shared<FakeListEntitiesTool>());
	CapabilityToken token = registry.IssueToken("Tester", {"ListEntities"}, PermissionLevel::Read);

	CommandRequest request;
	request.issuer = "Tester";
	request.tool = "ListEntities";
	request.arguments = Json::object({{"max", 10}});
	request.capability = token;

	const CommandResult result = pipeline.Submit(request);
	assert(result.IsOk());
	assert(result.payload.at("maxCount") == 10);
	assert(!result.payload.contains("max"));
	std::puts("  - ListEntities max/count/limit alias to maxCount: OK");
}

// ---------------------------------
// CHANGE 3: RetrievalWorker schema-error retry
// ---------------------------------

void TestSchemaRejectedSelfHealsWithOneCorrectiveRetry() {
	RemoveDb();
	TaskStore store;
	assert(store.Open(kDbPath));
	const SessionId session = store.CreateSession(Json::object({{"goal", "test"}}));
	const TaskId root = store.CreateTask(session, kInvalidId, "Intake", Json::object(), 0);

	auto tool = std::make_shared<FakeReadComponentTool>();
	CommandPipeline pipeline(nullptr);
	pipeline.RegisterTool(tool);

	const Json taskSpec = Json::object({
		{"taskId", "T1"},
		{"type", "RuntimeObservation"},
		{"allowedTools", Json::array({"ReadComponent"})},
	});
	const TaskId task = store.CreateTask(session, root, "RuntimeObservation", taskSpec, 1);
	assert(store.UpdateTaskState(task, TaskState::Running));

	MockLlmBackend llm;
	// GenerateQueriesの提案: componentNameはPipelineのTool別alias表で救済されるため、
	// SchemaRejectedを再現するにはalias表に無い誤フィールド名を使う。
	llm.AddRule(
		"を最大5件提案するWorker担当",
		"```json\n"
		"{\"commands\":[{\"tool\":\"ReadComponent\","
		"\"arguments\":{\"entityName\":\"Boss\",\"componentField\":\"Velocity\"}}]}\n"
		"```");
	// SchemaRejectedからの自己修復リトライ: role文言中の「スキーマ違反」をキーにする。
	llm.AddRule(
		"スキーマ違反",
		"```json\n"
		"{\"commands\":[{\"tool\":\"ReadComponent\","
		"\"arguments\":{\"entityName\":\"Boss\",\"component\":\"Velocity\"}}]}\n"
		"```");

	Budget budget;
	budget.maxLlmCalls = 6;
	budget.maxLlmChars = 200000;
	BudgetTracker tracker(budget);
	AgentContext ctx;
	ctx.llm = &llm;
	ctx.pipeline = &pipeline;
	ctx.store = &store;
	ctx.budget = &tracker;
	ctx.sessionId = session;

	std::vector<Evidence> evidence;
	Json summary;
	const Result result = RetrievalWorker::Run(ctx, task, taskSpec, &evidence, &summary);
	assert(result);
	assert(tool->executeCount == 1);

	int correctiveCalls = 0;
	for (const auto& call : llm.GetCalls()) {
		if ((call.first + call.second).find("スキーマ違反") != std::string::npos) ++correctiveCalls;
	}
	assert(correctiveCalls == 1);

	bool foundRetryEvidence = false;
	bool foundSuccessEvidence = false;
	for (const Evidence& e : evidence) {
		if (e.provenance.sourceType == "SchemaRepair" &&
		    e.claim.find("schema違反をリトライで修正した: ReadComponent") != std::string::npos &&
		    e.confidence == 1.0) {
			foundRetryEvidence = true;
		}
		if (e.provenance.sourceType == "Tool:ReadComponent") foundSuccessEvidence = true;
	}
	assert(foundRetryEvidence);
	assert(foundSuccessEvidence);
	assert(summary.value("executed", 0) == 1);
	assert(summary.value("failed", 0) == 0);

	RemoveDb();
	std::puts("  - SchemaRejected self-heals with exactly one corrective retry: OK");
}

void TestExplicitCommandsAreNotEligibleForCorrectiveRetry() {
	RemoveDb();
	TaskStore store;
	assert(store.Open(kDbPath));
	const SessionId session = store.CreateSession(Json::object({{"goal", "test"}}));
	const TaskId root = store.CreateTask(session, kInvalidId, "Intake", Json::object(), 0);

	auto tool = std::make_shared<FakeReadComponentTool>();
	CommandPipeline pipeline(nullptr);
	pipeline.RegisterTool(tool);

	// commandsが明示指定されている場合はGenerateQueries経由ではないため、
	// SchemaRejectedでも自己修復リトライの対象にならず、従来どおり即break/失敗する。
	const Json taskSpec = Json::object({
		{"taskId", "T2"},
		{"type", "RuntimeObservation"},
		{"allowedTools", Json::array({"ReadComponent"})},
		{"commands", Json::array({
			Json::object({{"tool", "ReadComponent"},
			               {"arguments", Json::object({{"entityName", "Boss"}, {"componentField", "Velocity"}})}}),
		})},
	});
	const TaskId task = store.CreateTask(session, root, "RuntimeObservation", taskSpec, 1);
	assert(store.UpdateTaskState(task, TaskState::Running));

	MockLlmBackend llm; // 呼ばれないはず
	AgentContext ctx;
	ctx.llm = &llm;
	ctx.pipeline = &pipeline;
	ctx.store = &store;
	ctx.sessionId = session;

	std::vector<Evidence> evidence;
	const Result result = RetrievalWorker::Run(ctx, task, taskSpec, &evidence, nullptr);
	assert(!result);
	assert(tool->executeCount == 0);
	assert(llm.GetCalls().empty());
	assert(evidence.size() == 1);
	assert(evidence[0].provenance.sourceType == "ToolError");

	RemoveDb();
	std::puts("  - explicit commands remain ineligible for corrective retry: OK");
}

} // namespace

int main() {
	std::cout << "=== AgentOS Memory And Retry Smoke Test ===\n";
	TestIntakeMemoryGroundingFilter();
	TestStartWriteTraceGetsReadComponentStyleAliases();
	TestGenericCaseInsensitiveKeyFix();
	TestListEntitiesCountAliases();
	TestSchemaRejectedSelfHealsWithOneCorrectiveRetry();
	TestExplicitCommandsAreNotEligibleForCorrectiveRetry();
	RemoveDb();
	std::cout << "=== ALL PASSED ===\n";
	return 0;
}
