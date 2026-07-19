// =======================================================================
//
// AgentOSContextIsolationSmokeTest.cpp
//
// new Turnで過去Topicを生成Contextから隔離し、DirectReply汚染を遮断する。
//
// =======================================================================
#include "AgentOS/Core/Agents/AgentContext.h"
#include "AgentOS/Core/Agents/IntakeAgent.h"
#include "AgentOS/Core/Budget/Budget.h"
#include "AgentOS/Core/Llm/MockLlmBackend.h"
#include "AgentOS/Core/Llm/PromptTemplates.h"
#include "AgentOS/Core/Store/TaskStore.h"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>

using namespace agentos;

namespace {

const char* kDbPath = "/tmp/agentos_context_isolation_test.db";

void RemoveDb() {
	std::remove(kDbPath);
	std::remove((std::string(kDbPath) + "-wal").c_str());
	std::remove((std::string(kDbPath) + "-shm").c_str());
	std::remove((std::string(kDbPath) + "-journal").c_str());
}

SessionId AddTurn(TaskStore& store, const std::string& user, const std::string& assistant) {
	const SessionId session = store.CreateSession(Json::object({{"userRequest", user}}));
	assert(session != kInvalidId);
	assert(store.SetConversationResponse(session, assistant));
	assert(store.UpdateSessionState(session, "Completed"));
	return session;
}

} // namespace

int main() {
	std::cout << "=== AgentOS Context Isolation Smoke Test ===\n";
	RemoveDb();

	TaskStore store;
	assert(store.Open(kDbPath));
	AddTurn(
		store,
		"Fieldになんのコンポーネントがあるの？",
		"FieldのComponent取得には失敗しました。追加調査が必要です。");
	const SessionId current = store.CreateSession(Json::object({{"userRequest", "こんにちはあなた"}}));
	assert(current != kInvalidId);

	MockLlmBackend llm;
	llm.AddRule(
		"Intake担当",
		"```json\n"
		"{\"goal\":\"挨拶へ応答する\",\"resolvedRequest\":\"ユーザーの挨拶へ応答する\","
		"\"turnRelation\":\"new\",\"referencedSessionIds\":[],\"symptoms\":[],"
		"\"constraints\":[\"Fieldを確認する\"],\"requiredCapabilities\":[\"ListEntities\"],"
		"\"unresolvedReferences\":[],\"requestType\":\"conversation\"}\n"
		"```");
	llm.AddRule(
		"DirectReply担当",
		"```json\n"
		"{\"reply\":\"Entity 'Field' にはComponentが登録されていません。\","
		"\"toolCall\":null,\"escalate\":false}\n"
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
	assert(IntakeAgent::Run(ctx, "こんにちはあなた", &intake));
	assert(intake.value("turnRelation", std::string()) == "new");
	assert(intake.value("simpleConversation", false));
	assert(intake.at("requiredCapabilities").empty());
	assert(intake.at("constraints").empty());
	assert(intake.at("conversationContext").at("recentTurns").empty());
	assert(intake.at("conversationContext").value("summary", std::string()).empty());

	const PromptPair direct = prompts::DirectReply("こんにちはあなた", "");
	Json reply;
	assert(CallLlmJson(ctx, direct, &reply));
	assert(reply.value("reply", std::string()).find("こんにちは") != std::string::npos);
	assert(reply.value("reply", std::string()).find("Field") == std::string::npos);
	assert(reply.contains("toolCall") && reply.at("toolCall").is_null());
	assert(!reply.value("escalate", true));

	prompts::ClearCurrentConversationRequestContext();
	RemoveDb();
	std::cout << "=== ALL PASSED ===\n";
	return 0;
}
