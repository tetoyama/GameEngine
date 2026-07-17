// =======================================================================
//
// AgentOSConversationMemorySmokeTest.cpp
//
// 全Turn原文保持 / 累積要約 / 最近Turn原文 / 訂正解決 / 人物質問Tool遮断。
//
// =======================================================================
#include "AgentOS/Core/Agents/AgentContext.h"
#include "AgentOS/Core/Agents/IntakeAgent.h"
#include "AgentOS/Core/Budget/Budget.h"
#include "AgentOS/Core/Command/CommandPipeline.h"
#include "AgentOS/Core/Llm/MockLlmBackend.h"
#include "AgentOS/Core/Llm/PromptTemplates.h"
#include "AgentOS/Core/Store/TaskStore.h"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

using namespace agentos;

namespace {

const char* kDbPath = "/tmp/agentos_conversation_memory_test.db";

void RemoveDb() {
	std::remove(kDbPath);
	std::remove((std::string(kDbPath) + "-wal").c_str());
	std::remove((std::string(kDbPath) + "-shm").c_str());
	std::remove((std::string(kDbPath) + "-journal").c_str());
}

SessionId AddTurn(TaskStore& store, const std::string& user, const std::string& assistant) {
	const SessionId session = store.CreateSession(Json::object({{"userRequest", user}}));
	assert(session != kInvalidId);
	const TaskId task = store.CreateTask(session, kInvalidId, "FinalResponse", Json::object(), 0);
	assert(task != kInvalidId);
	assert(store.SetTaskResult(task, Json::object({{"reply", assistant}})));
	assert(store.UpdateTaskState(task, TaskState::Running));
	assert(store.UpdateTaskState(task, TaskState::Succeeded));
	assert(store.UpdateSessionState(session, "Completed"));
	return session;
}

class CountingTool final : public ICommandExecutor {
public:
	CountingTool() {
		descriptor_.name = "FindEntityByName";
		descriptor_.description = "test lookup";
		descriptor_.requiredPermission = PermissionLevel::Read;
		descriptor_.argumentSchema = Json::object({
			{"name", Json::object({{"type", "string"}, {"required", true}})},
		});
	}

	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }
	CommandResult Execute(const Json&) override {
		++executeCount;
		return CommandResult::Ok();
	}

	int executeCount = 0;

private:
	ToolDescriptor descriptor_;
};

void TestTurnPairsAndSummaryCursor() {
	RemoveDb();
	TaskStore store;
	assert(store.Open(kDbPath));

	const SessionId first = AddTurn(store, "シーン全体を報告して", "EntityとSystemを一覧しました。");
	const SessionId second = AddTurn(store, "Playerを詳しく見て", "PlayerのComponentを確認しました。");
	const SessionId current = store.CreateSession(Json::object({{"userRequest", "続けて"}}));
	assert(current != kInvalidId);

	Json context = store.GetConversationContext(current);
	assert(context.value("totalTurns", 0) == 2);
	assert(context.at("recentTurns").size() == 2);
	assert(context.at("recentTurns")[0].value("sessionId", kInvalidId) == first);
	assert(context.at("recentTurns")[0].value("user", std::string()) == "シーン全体を報告して");
	assert(context.at("recentTurns")[0].value("assistant", std::string()) == "EntityとSystemを一覧しました。");
	assert(context.at("recentTurns")[1].value("sessionId", kInvalidId) == second);

	assert(store.UpdateConversationSummary("最初のTurnではScene全体を報告した。", first));
	context = store.GetConversationContext(current);
	assert(context.value("totalTurns", 0) == 2); // 原文Turnは削除されない。
	assert(context.value("summary", std::string()) == "最初のTurnではScene全体を報告した。");
	assert(context.at("recentTurns").size() == 1);
	assert(context.at("recentTurns")[0].value("sessionId", kInvalidId) == second);
	assert(!store.UpdateConversationSummary("backwards", kInvalidId));

	std::puts("  - complete turn pairs and summary cursor: OK");
}

void TestCorrectionResolutionUsesHistory() {
	RemoveDb();
	TaskStore store;
	assert(store.Open(kDbPath));
	AddTurn(store, "現在のシーン全体を報告して", "Scene全体のEntityとSystemを報告しました。");
	const SessionId current = store.CreateSession(
		Json::object({{"userRequest", "そうじゃなくて、Playerだけ見て"}}));

	MockLlmBackend llm;
	llm.AddRule(
		"Intake担当",
		"```json\n"
		"{\"goal\":\"Player Entityだけを確認する\","
		"\"resolvedRequest\":\"Scene全体ではなくPlayer EntityだけのComponentと現在状態を確認する\","
		"\"turnRelation\":\"correct\",\"symptoms\":[],"
		"\"constraints\":[\"Scene全体は対象外\"],"
		"\"requiredCapabilities\":[\"DescribeEntity\"],"
		"\"unresolvedReferences\":[],\"requestType\":\"investigation\"}\n"
		"```");

	Budget budget;
	budget.maxLlmCalls = 4;
	budget.maxLlmChars = 200000;
	BudgetTracker tracker(budget);
	AgentContext ctx;
	ctx.llm = &llm;
	ctx.store = &store;
	ctx.budget = &tracker;
	ctx.sessionId = current;

	Json intake;
	assert(IntakeAgent::Run(ctx, "そうじゃなくて、Playerだけ見て", &intake));
	assert(intake.value("turnRelation", std::string()) == "correct");
	assert(intake.value("resolvedRequest", std::string()).find("Player Entityだけ") != std::string::npos);
	assert(prompts::CurrentResolvedRequest().find("Player Entityだけ") != std::string::npos);

	const auto calls = llm.GetCalls();
	assert(calls.size() == 1);
	assert(calls[0].second.find("現在のシーン全体を報告して") != std::string::npos);
	assert(calls[0].second.find("Scene全体のEntityとSystemを報告しました") != std::string::npos);
	assert(calls[0].second.find("そうじゃなくて、Playerだけ見て") != std::string::npos);

	std::puts("  - correction resolution uses complete history: OK");
}

void TestAutomaticCompressionKeepsRecentRawTurns() {
	RemoveDb();
	TaskStore store;
	assert(store.Open(kDbPath));

	SessionId fourth = kInvalidId;
	for (int i = 0; i < 10; ++i) {
		const SessionId session = AddTurn(
			store,
			"user turn " + std::to_string(i),
			"assistant final " + std::to_string(i));
		if (i == 3) fourth = session;
	}
	const SessionId current = store.CreateSession(Json::object({{"userRequest", "続けて"}}));

	MockLlmBackend llm;
	llm.AddRule(
		"Memory担当",
		"```json\n{\"summary\":\"turn 0〜3の目的と最終応答を累積要約した。\"}\n```");
	llm.AddRule(
		"Intake担当",
		"```json\n"
		"{\"goal\":\"直前作業を続ける\",\"resolvedRequest\":\"turn 9の応答を前提に作業を続ける\","
		"\"turnRelation\":\"continue\",\"symptoms\":[],\"constraints\":[],"
		"\"requiredCapabilities\":[],\"unresolvedReferences\":[],"
		"\"requestType\":\"conversation\"}\n"
		"```");

	Budget budget;
	budget.maxLlmCalls = 6;
	budget.maxLlmChars = 300000;
	BudgetTracker tracker(budget);
	AgentContext ctx;
	ctx.llm = &llm;
	ctx.store = &store;
	ctx.budget = &tracker;
	ctx.sessionId = current;

	Json intake;
	assert(IntakeAgent::Run(ctx, "続けて", &intake));
	const auto calls = llm.GetCalls();
	assert(calls.size() == 2); // Memory compression + Intake

	const Json context = store.GetConversationContext(current);
	assert(context.value("totalTurns", 0) == 10); // 全原文は残る。
	assert(context.value("summarizedThroughSessionId", kInvalidId) == fourth);
	assert(context.value("summary", std::string()).find("turn 0〜3") != std::string::npos);
	assert(context.at("recentTurns").size() == 6);
	assert(context.at("recentTurns")[0].value("user", std::string()) == "user turn 4");
	assert(context.at("recentTurns")[5].value("assistant", std::string()) == "assistant final 9");
	assert(calls[1].second.find("user turn 9") != std::string::npos);
	assert(calls[1].second.find("assistant final 9") != std::string::npos);

	std::puts("  - automatic compression retains recent raw turns: OK");
}

void TestPersonalIdentityCannotExecuteEngineTool() {
	prompts::SetCurrentConversationRequestContext(
		Json::object(),
		Json::object({
			{"resolvedRequest", "私は誰ですか"},
			{"requestType", "conversation"},
		}));

	auto tool = std::make_shared<CountingTool>();
	CommandPipeline pipeline(nullptr);
	pipeline.RegisterTool(tool);

	CommandRequest request;
	request.issuer = "QuickPath";
	request.tool = "FindEntityByName";
	request.arguments = Json::object({{"name", "わたし"}});
	const CommandResult result = pipeline.Submit(request);
	assert(result.status == CommandStatus::PreconditionRejected);
	assert(tool->executeCount == 0);
	prompts::ClearCurrentConversationRequestContext();

	std::puts("  - personal identity Engine Tool guard: OK");
}

} // namespace

int main() {
	std::cout << "=== AgentOS Conversation Memory Smoke Test ===\n";
	TestTurnPairsAndSummaryCursor();
	TestCorrectionResolutionUsesHistory();
	TestAutomaticCompressionKeepsRecentRawTurns();
	TestPersonalIdentityCannotExecuteEngineTool();
	RemoveDb();
	std::cout << "=== ALL PASSED ===\n";
	return 0;
}
