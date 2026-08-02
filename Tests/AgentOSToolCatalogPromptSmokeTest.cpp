// =======================================================================
//
// AgentOSToolCatalogPromptSmokeTest.cpp
//
// Plannerが受け取ったTool CatalogがPlanner PromptとCritic Promptの両方へ
// 同じ内容で渡され、セッション終了時に破棄されることを検証する。
//
// =======================================================================
#include "AgentOS/Core/Agents/PlannerAgent.h"
#include "AgentOS/Core/Llm/PromptTemplates.h"

#include <cassert>
#include <iostream>
#include <string>

using namespace agentos;

namespace {

Json BuildToolCatalog() {
	return Json::array({
		Json::object({
			{"name", "CodeSearch"},
			{"description", "コード定義を検索する"},
			{"requiredPermission", "Read"},
			{"argumentSchema", Json::object({
				{"query", Json::object({{"type", "string"}, {"required", true}})},
				{"file", Json::object({{"type", "string"}, {"required", false}})},
			})},
		}),
		Json::object({
			{"name", "FindCodeReferences"},
			{"description", "ソース全体から参照箇所を完全走査する"},
			{"requiredPermission", "Read"},
			{"argumentSchema", Json::object({
				{"query", Json::object({{"type", "string"}, {"required", true}})},
				{"topK", Json::object({{"type", "integer"}, {"required", false}})},
			})},
		}),
	});
}

void TestPlannerPromptContainsCatalog() {
	prompts::ClearCurrentConversationRequestContext();
	const Json catalog = BuildToolCatalog();
	const Json intake = Json::object({
		{"goal", "コードを調査する"},
		{"resolvedRequest", "対象コードを調査する"},
	});

	const PromptPair prompt = prompts::Plan(intake, catalog, 4);
	assert(prompt.user.find("Tool一覧") != std::string::npos);
	assert(prompt.user.find("CodeSearch(file?:string, query:string)") != std::string::npos ||
	       prompt.user.find("CodeSearch(query:string, file?:string)") != std::string::npos);
	assert(prompt.user.find("FindCodeReferences(query:string, topK?:integer)") != std::string::npos ||
	       prompt.user.find("FindCodeReferences(topK?:integer, query:string)") != std::string::npos);
	assert(prompts::CurrentToolCatalog() == catalog);
}

void TestCriticPromptUsesPlannerCatalog() {
	prompts::ClearCurrentConversationRequestContext();
	const Json catalog = BuildToolCatalog();

	// Deterministic code plannerはprompts::Planを呼ばないため、この経路でも
	// PlannerAgentが受け取ったCatalogを保存できることを確認する。
	AgentContext ctx;
	Json plan;
	const Json intake = Json::object({
		{"goal", "SqliteDb::Prepareの呼び出し元を調べる"},
		{"resolvedRequest", "SqliteDb::Prepareを呼び出している箇所をすべて探して"},
	});
	assert(PlannerAgent::Run(ctx, intake, catalog, &plan));
	assert(plan.value("route", std::string()) == "deterministic_code_investigation");
	assert(prompts::CurrentToolCatalog() == catalog);

	// WorkerはTask単位に絞ったCatalogを受け取るが、それでPlannerの完全な
	// Catalogを上書きしてはならない。Criticは全候補から修復Toolを選ぶ必要がある。
	const Json filteredCatalog = Json::array({catalog.at(0)});
	(void)prompts::GenerateQueries(Json::object(), filteredCatalog);
	assert(prompts::CurrentToolCatalog() == catalog);

	const Json hypotheses = Json::object({
		{"hypotheses", Json::array({Json::object({
			{"description", "追加の参照検索が必要"},
			{"supports", Json::array()},
			{"contradicts", Json::array()},
			{"missingEvidence", Json::array({"呼び出し元"})},
		})})},
	});
	const Json evidence = Json::object({
		{"evidences", Json::array()},
		{"coverage", 0.0},
	});

	const PromptPair critic = prompts::Critique(hypotheses, evidence);
	assert(critic.user.find("利用可能なTool一覧（この一覧外は提案禁止）") != std::string::npos);
	assert(critic.user.find("CodeSearch") != std::string::npos);
	assert(critic.user.find("FindCodeReferences") != std::string::npos);
	assert(critic.user.find("query:string") != std::string::npos);
	assert(critic.user.find("topK?:integer") != std::string::npos);
	assert(critic.system.find("Tool一覧に存在しないTool名を創作しない") != std::string::npos);
	assert(critic.user.find("GrepTheUniverse") == std::string::npos);
}

void TestClearDropsCatalog() {
	prompts::SetCurrentToolCatalog(BuildToolCatalog());
	assert(!prompts::CurrentToolCatalog().empty());
	prompts::ClearCurrentConversationRequestContext();
	assert(prompts::CurrentToolCatalog().empty());

	const PromptPair critic = prompts::Critique(Json::object(), Json::object());
	assert(critic.user.find("(利用可能なToolなし)") != std::string::npos);
}

} // namespace

int main() {
	std::cout << "=== AgentOS Tool Catalog Prompt Smoke Test ===\n";
	TestPlannerPromptContainsCatalog();
	TestCriticPromptUsesPlannerCatalog();
	TestClearDropsCatalog();
	std::cout << "=== ALL PASSED ===\n";
	return 0;
}
