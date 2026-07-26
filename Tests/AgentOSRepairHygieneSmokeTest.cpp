// =======================================================================
//
// AgentOSRepairHygieneSmokeTest.cpp
//
// 修復ループが「自分の出した不備」で自壊しないことを検証する。
//
// 実機で観測した故障（transcript_20260726_015802）:
//   ラウンド0は coverage 1.0 / 失敗0 で通る状態だった。
//   しかしCriticが追加調査を提案したためゲート#6でrepairへ入り、
//   その提案の引数に内部項目 requestRevision が混入していたため
//   Toolがスキーマ拒否 → 失敗Evidenceとして記録 →
//   coverage 1.0→0.333、失敗0→2 と悪化し「未完了」で終わった。
//   つまり修復が、通っていた状態を破壊した。
//
//   引数の不備は「計画の失敗」であって「調べても無かった」ではない。
//   世界について何も言っていないものを観測結果として扱ってはいけない。
//
// あわせて、最上位仮説の合否をconfidenceスカラーではなく
// 構造（根拠の有無・不足の申告・矛盾）で見ることを検証する。
// 実機では内容が正しい仮説にモデルが0.38を振り、閾値0.4に0.02足りず落ちた。
//
// MockLlmBackendのAddRuleは systemPrompt+userPrompt の部分文字列一致。
// 各段のsystemPrompt冒頭にある役割記述（"Intake担当"等）を目印にする
// （AgentOSVerticalSliceSmokeTestの注記と同じ方針）。
//
// =======================================================================
#include "AgentOS/Core/Agents/AgentContext.h"
#include "AgentOS/Core/Agents/CriticAgent.h"
#include "AgentOS/Core/Command/CapabilitySet.h"
#include "AgentOS/Core/Command/CommandPipeline.h"
#include "AgentOS/Core/Command/CommandSchema.h"
#include "AgentOS/Core/Llm/MockLlmBackend.h"
#include "AgentOS/Core/Llm/PromptTemplates.h"
#include "AgentOS/Core/Orchestrator/Orchestrator.h"
#include "AgentOS/Core/Store/TaskStore.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

using namespace agentos;

namespace {

// ---------------------------------
// 検索Tool（queryのみ必須）
// ---------------------------------
class SearchTool final : public ICommandExecutor {
public:
	SearchTool()
		: m_descriptor{
			"CodeSearch", "コードを検索する", PermissionLevel::Read,
			Json::object({
				{"query", Json::object({{"type", "string"}, {"required", true}})},
			})} {}

	const ToolDescriptor& Descriptor() const override { return m_descriptor; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }

	CommandResult Execute(const Json& args) override {
		++executions;
		lastQuery = args.value("query", std::string());
		return CommandResult::Ok(Json::object({
			{"claim", "コードを取得した: " + lastQuery},
			{"value", "Result SqliteDb::Prepare(...) { ... }"},
		}));
	}

	int executions = 0;
	std::string lastQuery;

private:
	ToolDescriptor m_descriptor;
};

void SetupCommonRules(MockLlmBackend& llm) {
	llm.AddRule("Intake担当",
		"```json\n{\"goal\": \"SqliteDb::Prepare の実装を提示する\", "
		"\"symptoms\": [], \"constraints\": [], \"requiredCapabilities\": [\"CodeSearch\"]}\n```");

	llm.AddRule("Planner担当",
		"```json\n{\"tasks\": [{\"taskId\": \"T1\", \"type\": \"CodeSearch\", "
		"\"description\": \"実装コードを検索する\", \"dependencies\": [], "
		"\"allowedTools\": [\"CodeSearch\"], \"searchHints\": [\"SqliteDb\"]}]}\n```");

	llm.AddRule("\"taskId\": \"T1\"",
		"```json\n{\"commands\": [{\"tool\": \"CodeSearch\", "
		"\"arguments\": {\"query\": \"SqliteDb Prepare\"}}]}\n```");

	llm.AddRule("Reasoning担当",
		"```json\n{\"hypotheses\": [{\"description\": "
		"\"SqliteDb::Prepare は sqlite3_prepare_v2 で事前コンパイルする\", "
		"\"rubricBase\": 0.9, \"supports\": [1], \"contradicts\": [], "
		"\"missingEvidence\": []}]}\n```");

	llm.AddRule("Synthesis担当",
		"```json\n{\"report\": \"SqliteDb::Prepare の実装を提示しました。\"}\n```");
}

// -----------------------------------------------------------------------
// 引数の混入がスキーマ検証で捕まること
// -----------------------------------------------------------------------
void TestContaminatedArgumentIsDetectable() {
	const Json schema = Json::object({
		{"query", Json::object({{"type", "string"}, {"required", true}})},
		{"file", Json::object({{"type", "string"}, {"required", false}})},
	});

	// 実機でCriticが出した形。queryは妥当だがrequestRevisionが混入している。
	const Json contaminated = Json::object({
		{"query", "SqliteDb Statement mutex lock guard"},
		{"requestRevision", 0}, // Evidenceの内部項目。Toolの引数ではない
	});
	const Result bad = SchemaValidator::Validate(contaminated, schema);
	assert(!bad);
	assert(bad.error.find("requestRevision") != std::string::npos);

	// 創作されたフィールド名（正しくは file）も同様に弾かれる
	const Json invented = Json::object({
		{"query", "Statement definition"},
		{"targetFile", "SqliteDb.h"},
	});
	assert(!SchemaValidator::Validate(invented, schema));

	// 混入を取り除けば通る
	assert(SchemaValidator::Validate(
		Json::object({{"query", "SqliteDb Statement mutex lock guard"}}), schema));

	std::printf("  [ok] contaminated arguments are detectable before execution\n");
}

// -----------------------------------------------------------------------
// 混入は取り除かれ、修復は走り、Evidenceは汚れないこと
// -----------------------------------------------------------------------
void TestContaminatedRepairIsCleanedNotPoisoned() {
	const std::string dbPath = "/tmp/agentos_repair_hygiene1.db";
	std::remove(dbPath.c_str());

	TaskStore store;
	assert(store.Open(dbPath));

	CapabilityRegistry registry;
	CommandPipeline pipeline(&registry);
	auto tool = std::make_shared<SearchTool>();
	pipeline.RegisterTool(tool);

	MockLlmBackend llm;
	SetupCommonRules(llm);

	// 実機と同じ形。引数に内部項目 requestRevision が混入している。
	llm.AddRule("Critic担当",
		"```json\n{\"scores\": {\"evidenceCoverage\": 0.9, \"contradictionHandling\": 1.0, "
		"\"causalCompleteness\": 0.8, \"testability\": 0.7}, "
		"\"failures\": [], \"goalSatisfied\": true, \"unmetAspects\": [], "
		"\"additionalTasksSuggested\": [{\"type\": \"CodeSearch\", "
		"\"description\": \"スレッドセーフ性を確認する\", \"tool\": \"CodeSearch\", "
		"\"arguments\": {\"query\": \"mutex lock guard\", \"requestRevision\": 0}}], "
		"\"obsoleteTasks\": []}\n```");

	OrchestratorConfig config;
	config.maxRepairRounds = 1;
	Orchestrator orchestrator(&llm, &pipeline, &store, &registry, config);

	const OrchestratorResult result = orchestrator.RunSession("SqliteDb::Prepareの実装を見せて");

	// 混入した項目だけが落とされ、修復Taskは実行される。
	// queryそのものは妥当だったので、提案ごと捨てるのは惜しい。
	assert(tool->executions == 2);
	assert(tool->lastQuery == "mutex lock guard");

	// 本題: 失敗Evidenceが1件も無いこと。
	// 実機ではスキーマ拒否が失敗Evidenceとして記録されていた。
	assert(result.builtEvidence.contains("failedEvidenceCount"));
	assert(result.builtEvidence.at("failedEvidenceCount").get<std::size_t>() == 0);

	// カバレッジが劣化していないこと（実機では 1.0 → 0.333 になった）
	assert(result.builtEvidence.at("coverage").get<double>() == 1.0);
	assert(result.builtEvidence.at("tasksWithoutEvidence").empty());

	std::remove(dbPath.c_str());
	std::printf("  [ok] contaminated arguments are cleaned; repair runs and evidence stays clean\n");
}

// -----------------------------------------------------------------------
// 直せない提案はTask化されないこと
// -----------------------------------------------------------------------
void TestUnsalvageableRepairIsNotCreated() {
	const std::string dbPath = "/tmp/agentos_repair_hygiene2.db";
	std::remove(dbPath.c_str());

	TaskStore store;
	assert(store.Open(dbPath));

	CapabilityRegistry registry;
	CommandPipeline pipeline(&registry);
	auto tool = std::make_shared<SearchTool>();
	pipeline.RegisterTool(tool);

	MockLlmBackend llm;
	SetupCommonRules(llm);

	// 存在しないTool名。引数を掃除しても直しようがない。
	llm.AddRule("Critic担当",
		"```json\n{\"scores\": {\"evidenceCoverage\": 0.9, \"contradictionHandling\": 1.0, "
		"\"causalCompleteness\": 0.8, \"testability\": 0.7}, "
		"\"failures\": [], \"goalSatisfied\": true, \"unmetAspects\": [], "
		"\"additionalTasksSuggested\": [{\"type\": \"CodeSearch\", "
		"\"description\": \"追加調査\", \"tool\": \"GrepTheUniverse\", "
		"\"arguments\": {\"pattern\": \"何か\"}}], "
		"\"obsoleteTasks\": []}\n```");

	OrchestratorConfig config;
	config.maxRepairRounds = 1;
	Orchestrator orchestrator(&llm, &pipeline, &store, &registry, config);

	const OrchestratorResult result = orchestrator.RunSession("実装を見せて");

	// 直せない提案は実行されない
	assert(tool->executions == 1);
	// そしてEvidenceは汚れない
	assert(result.builtEvidence.at("failedEvidenceCount").get<std::size_t>() == 0);
	assert(result.builtEvidence.at("coverage").get<double>() == 1.0);

	std::remove(dbPath.c_str());
	std::printf("  [ok] an unsalvageable repair task is never created\n");
}

// -----------------------------------------------------------------------
// 合否が自己申告スカラーではなく構造で決まること
// -----------------------------------------------------------------------
void TestVerdictUsesStructureNotSelfReportedScore() {
	// resolvedRequestはthread-localに保持されるため、直前のセッションの値が残る。
	// ゲート#8（目的識別子の被覆）が前テストの要求と突き合わせてしまうので、
	// ここでは明示的に初期化してから最上位仮説のゲートだけを見る。
	prompts::ClearCurrentConversationRequestContext();

	const Json evidence = Json::object({
		{"evidences", Json::array({Json::object({{"id", 1}, {"claim", "実装を取得した"}})})},
		{"contradictions", Json::array()},
		{"coverage", 1.0},
		{"tasksWithoutEvidence", Json::array()},
		{"usableEvidenceCount", 1},
		{"failedEvidenceCount", 0},
		{"activeRevision", 0},
	});

	MockLlmBackend llm;
	llm.AddRule("Critic担当",
		"```json\n{\"scores\": {\"evidenceCoverage\": 1.0, \"contradictionHandling\": 1.0, "
		"\"causalCompleteness\": 1.0, \"testability\": 1.0}, "
		"\"failures\": [], \"goalSatisfied\": true, \"unmetAspects\": [], "
		"\"additionalTasksSuggested\": [], \"obsoleteTasks\": []}\n```");

	AgentContext ctx;
	ctx.llm = &llm;

	const auto makeHypothesis = [](double confidence, const Json& supports,
	                               const Json& missing, const Json& contradicts) {
		return Json::object({{"hypotheses", Json::array({Json::object({
			{"id", 1},
			{"text", "SqliteDb::Prepare は sqlite3_prepare_v2 で事前コンパイルする"},
			{"confidence", confidence},
			{"supports", supports},
			{"contradicts", contradicts},
			{"missingEvidence", missing},
		})})}});
	};

	// 実機で落ちた形。内容は正しく根拠もあるが、モデルが0.38を振った。
	{
		CriticVerdict verdict;
		assert(CriticAgent::Run(
			ctx, makeHypothesis(0.38, Json::array({1}), Json::array(), Json::array()),
			evidence, &verdict));
		for (const std::string& f : verdict.failures) {
			assert(f.find("confidence") == std::string::npos);
		}
		assert(verdict.pass); // 0.38でも構造が揃っていれば通す
	}

	// 根拠が無い仮説は、点数が高くても落とす
	{
		CriticVerdict verdict;
		assert(CriticAgent::Run(
			ctx, makeHypothesis(0.99, Json::array(), Json::array(), Json::array()),
			evidence, &verdict));
		assert(!verdict.pass);
	}

	// 本人が「足りない」と言っているなら、点数が高くても落とす
	{
		CriticVerdict verdict;
		assert(CriticAgent::Run(
			ctx, makeHypothesis(0.95, Json::array({1}),
			                    Json::array({"ロックの適用箇所"}), Json::array()),
			evidence, &verdict));
		assert(!verdict.pass);
	}

	// 矛盾を抱えているなら落とす
	{
		CriticVerdict verdict;
		assert(CriticAgent::Run(
			ctx, makeHypothesis(0.95, Json::array({1}), Json::array(), Json::array({2})),
			evidence, &verdict));
		assert(!verdict.pass);
	}

	// 仮説が1件も無いなら落とす
	{
		CriticVerdict verdict;
		assert(CriticAgent::Run(
			ctx, Json::object({{"hypotheses", Json::array()}}), evidence, &verdict));
		assert(!verdict.pass);
	}

	std::printf("  [ok] verdict follows structure (support/missing/contradiction), not the score\n");
}

} // namespace

int main() {
	std::printf("==== AgentOSRepairHygieneSmokeTest ====\n");

	TestContaminatedArgumentIsDetectable();
	TestContaminatedRepairIsCleanedNotPoisoned();
	TestUnsalvageableRepairIsNotCreated();
	TestVerdictUsesStructureNotSelfReportedScore();

	std::printf("==== AgentOSRepairHygieneSmokeTest: PASSED ====\n");
	return 0;
}
