// =======================================================================
//
// AgentOSStructuredConversationContextSmokeTest.cpp
//
// Raw assistant本文をPromptへ再注入せず、構造化Thread Stateだけを参照する。
// =======================================================================
#include "AgentOS/Core/Agents/AgentContext.h"
#include "AgentOS/Core/Agents/IntakeAgent.h"
#include "AgentOS/Core/Llm/MockLlmBackend.h"
#include "AgentOS/Core/Store/TaskStore.h"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>

using namespace agentos;

namespace {

void RemoveDb(const std::string& path) {
	std::remove(path.c_str());
	std::remove((path + "-wal").c_str());
	std::remove((path + "-shm").c_str());
	std::remove((path + "-journal").c_str());
}

SessionId AddTurn(
	TaskStore& store,
	const std::string& user,
	const std::string& assistant,
	const Json& state) {
	const SessionId session = store.CreateSession(Json::object({{"userRequest", user}}));
	assert(session != kInvalidId);
	assert(store.SetConversationResponse(session, assistant));
	assert(store.SetConversationThreadState(session, state));
	assert(store.UpdateSessionState(session, "Completed"));
	return session;
}

void GreetingSkipsHistoryAndIntakeLlm() {
	const std::string dbPath = "/tmp/agentos_structured_greeting.db";
	RemoveDb(dbPath);
	TaskStore store;
	assert(store.Open(dbPath));
	AddTurn(
		store,
		"プレイヤーのジャンプ力を教えて",
		"RAW_ASSISTANT_SECRET: 権限制限により調査を続行する",
		Json::object({
			{"goal", "ジャンプ速度を取得する"},
			{"resolvedRequest", "PlatformerPlayerのジャンプ速度を取得する"},
			{"turnRelation", "new"},
			{"requestType", "investigation"},
			{"targets", Json::object({{"entityName", "PlatformerPlayer"}})},
		}));
	const SessionId current = store.CreateSession(Json::object({{"userRequest", "こんにちは"}}));

	MockLlmBackend llm;
	AgentContext ctx;
	ctx.llm = &llm;
	ctx.store = &store;
	ctx.sessionId = current;
	Json intake;
	assert(IntakeAgent::Run(ctx, "こんにちは", &intake));
	assert(llm.GetCalls().empty());
	assert(intake.value("turnRelation", std::string()) == "new");
	assert(intake.value("simpleConversation", false));
	assert(intake.at("conversationContext").at("threadStates").empty());
	assert(intake.at("conversationContext").at("recentTurns").empty());
	RemoveDb(dbPath);
}

void ContinuationUsesOnlyStructuredState() {
	const std::string dbPath = "/tmp/agentos_structured_continue.db";
	RemoveDb(dbPath);
	TaskStore store;
	assert(store.Open(dbPath));
	AddTurn(
		store,
		"PlatformerPlayerのジャンプ力を教えて",
		"RAW_ASSISTANT_SECRET: " + std::string(12000, 'X'),
		Json::object({
			{"goal", "ジャンプ速度を取得する"},
			{"resolvedRequest", "PlatformerPlayerのPlatformerCharacterControllerからジャンプ速度を取得する"},
			{"turnRelation", "new"},
			{"requestType", "investigation"},
			{"targets", Json::object({
				{"kind", "field"},
				{"entityName", "PlatformerPlayer"},
				{"concept", "jump velocity"},
			})},
			{"unresolved", Json::array({"Field名"})},
		}));
	const SessionId current = store.CreateSession(Json::object({{"userRequest", "続けて"}}));

	MockLlmBackend llm;
	llm.EnqueueResponse(
		"```json\n"
		"{\"goal\":\"前の調査を続ける\","
		"\"resolvedRequest\":\"PlatformerPlayerのPlatformerCharacterControllerからジャンプ速度を取得する\","
		"\"turnRelation\":\"continue\",\"referencedSessionIds\":[],"
		"\"symptoms\":[],\"constraints\":[],\"requiredCapabilities\":[],"
		"\"unresolvedReferences\":[\"Field名\"],\"targetKind\":\"field\","
		"\"targetConcept\":\"jump velocity\",\"resolvedEntityName\":\"PlatformerPlayer\","
		"\"requestType\":\"investigation\"}\n```" );

	AgentContext ctx;
	ctx.llm = &llm;
	ctx.store = &store;
	ctx.sessionId = current;
	Json intake;
	assert(IntakeAgent::Run(ctx, "続けて", &intake));
	const auto calls = llm.GetCalls();
	assert(calls.size() == 1);
	const std::string& prompt = calls.front().second;
	assert(prompt.find("RAW_ASSISTANT_SECRET") == std::string::npos);
	assert(prompt.find(std::string(1000, 'X')) == std::string::npos);
	assert(prompt.find("PlatformerCharacterController") != std::string::npos);
	assert(intake.at("conversationContext").at("threadStates").size() == 1);
	RemoveDb(dbPath);
}

void RefreshDoesNotReceivePreviousState() {
	const std::string dbPath = "/tmp/agentos_structured_refresh.db";
	RemoveDb(dbPath);
	TaskStore store;
	assert(store.Open(dbPath));
	AddTurn(
		store,
		"以前の質問",
		"RAW_ASSISTANT_SECRET",
		Json::object({
			{"goal", "古い調査"},
			{"resolvedRequest", "STALE_PLATFORMER_STATE"},
			{"turnRelation", "new"},
			{"requestType", "investigation"},
		}));
	const SessionId current = store.CreateSession(Json::object({{"userRequest", "今のシーンの状況を教えて"}}));

	MockLlmBackend llm;
	llm.EnqueueResponse(
		"```json\n"
		"{\"goal\":\"現在のシーンを確認する\",\"resolvedRequest\":\"現在のシーンを再観測する\","
		"\"turnRelation\":\"refer\",\"referencedSessionIds\":[1],\"symptoms\":[],"
		"\"constraints\":[],\"requiredCapabilities\":[\"ListEntities\"],"
		"\"unresolvedReferences\":[],\"targetKind\":\"concept\","
		"\"targetConcept\":\"current scene\",\"resolvedEntityName\":null,"
		"\"requestType\":\"investigation\"}\n```" );

	AgentContext ctx;
	ctx.llm = &llm;
	ctx.store = &store;
	ctx.sessionId = current;
	Json intake;
	assert(IntakeAgent::Run(ctx, "今のシーンの状況を教えて", &intake));
	const auto calls = llm.GetCalls();
	assert(calls.size() == 1);
	assert(calls.front().second.find("STALE_PLATFORMER_STATE") == std::string::npos);
	assert(calls.front().second.find("RAW_ASSISTANT_SECRET") == std::string::npos);
	assert(intake.value("turnRelation", std::string()) == "refresh");
	assert(intake.at("conversationContext").at("threadStates").empty());
	RemoveDb(dbPath);
}

} // namespace

int main() {
	std::cout << "=== AgentOS Structured Conversation Context Smoke Test ===\n";
	GreetingSkipsHistoryAndIntakeLlm();
	ContinuationUsesOnlyStructuredState();
	RefreshDoesNotReceivePreviousState();
	prompts::ClearCurrentConversationRequestContext();
	std::cout << "=== ALL PASSED ===\n";
	return 0;
}
