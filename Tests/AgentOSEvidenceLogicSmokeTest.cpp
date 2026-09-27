// =======================================================================
//
// AgentOSEvidenceLogicSmokeTest.cpp
//
// AgentOS Core（Evidence / EvidenceBuilder / LogicGraph / JsonExtractor /
// MockLlmBackend / PromptTemplates）の自己完結スモークテスト。
//
// =======================================================================
#include "AgentOS/Core/Evidence/Evidence.h"
#include "AgentOS/Core/Evidence/EvidenceBuilder.h"
#include "AgentOS/Core/Evidence/EvidencePromptCompressor.h"
#include "AgentOS/Core/Logic/LogicGraph.h"
#include "AgentOS/Core/Llm/JsonExtractor.h"
#include "AgentOS/Core/Llm/MockLlmBackend.h"
#include "AgentOS/Core/Llm/PromptTemplates.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

using namespace agentos;

namespace {

void TestEvidenceJsonRoundTrip() {
	Evidence e;
	e.id = 42;
	e.taskId = 7;
	e.claim = "PlayerHealth is zero at frame 120";
	e.payload = Json::object({{"target", "PlayerHealth"}, {"value", 0}});
	e.provenance.sourceType = "RuntimeTrace";
	e.provenance.sourceUri = "ReadComponent";
	e.provenance.session = "run_51";
	e.provenance.frame = 120;
	e.confidence = 0.9;

	const Json j = e.ToJson();
	const Evidence back = Evidence::FromJson(j);

	assert(back.id == e.id);
	assert(back.taskId == e.taskId);
	assert(back.claim == e.claim);
	assert(back.payload == e.payload);
	assert(back.provenance.sourceType == e.provenance.sourceType);
	assert(back.provenance.sourceUri == e.provenance.sourceUri);
	assert(back.provenance.session == e.provenance.session);
	assert(back.provenance.frame == e.provenance.frame);
	assert(std::abs(back.confidence - e.confidence) < 1e-9);

	// FromJsonはフィールド欠損に寛容であること。
	const Json partial = Json::object({{"claim", "only claim"}});
	const Evidence partialEvidence = Evidence::FromJson(partial);
	assert(partialEvidence.claim == "only claim");
	assert(partialEvidence.id == kInvalidId);
	assert(partialEvidence.provenance.frame == -1);

	// 型不一致（json例外）はデフォルト構築へフォールバックすること。
	const Json broken = Json::object({{"confidence", "not-a-number"}});
	const Evidence brokenEvidence = Evidence::FromJson(broken);
	assert(brokenEvidence.id == kInvalidId);
	assert(brokenEvidence.claim.empty());

	std::puts("  - Evidence JSON round-trip: OK");
}

void TestDedup() {
	EvidenceBuilder builder;

	Evidence e1;
	e1.id = 1;
	e1.taskId = 10;
	e1.claim = "duplicate claim";
	e1.payload = Json::object({{"k", 1}});
	builder.Add(e1);

	Evidence e2 = e1;
	e2.id = 2; // idが違ってもclaim+payloadが同一なら重複扱い
	builder.Add(e2);

	const EvidenceBuilder::BuiltEvidence built = builder.Build();
	assert(built.evidences.size() == 1);
	assert(built.evidences[0].id == 1); // 先勝ち

	std::puts("  - Dedup: OK");
}

void TestContradictionDetection() {
	EvidenceBuilder builder;

	// ルール1: 同一target・同一frameで値が食い違う → 矛盾
	Evidence a;
	a.id = 1;
	a.taskId = 1;
	a.claim = "posX read A";
	a.payload = Json::object({{"target", "posX"}, {"value", 5}});
	a.provenance.frame = 10;
	builder.Add(a);

	Evidence b;
	b.id = 2;
	b.taskId = 2;
	b.claim = "posX read B";
	b.payload = Json::object({{"target", "posX"}, {"value", 7}});
	b.provenance.frame = 10;
	builder.Add(b);

	// 異なるframeなら矛盾としない
	Evidence c;
	c.id = 3;
	c.taskId = 3;
	c.claim = "posX read C";
	c.payload = Json::object({{"target", "posX"}, {"value", 9}});
	c.provenance.frame = 11;
	builder.Add(c);

	// ルール2: 同一claimでpayload["value"]が食い違う → 矛盾
	Evidence d1;
	d1.id = 4;
	d1.taskId = 4;
	d1.claim = "same claim text";
	d1.payload = Json::object({{"value", 1}});
	builder.Add(d1);

	Evidence d2;
	d2.id = 5;
	d2.taskId = 5;
	d2.claim = "same claim text";
	d2.payload = Json::object({{"value", 2}});
	builder.Add(d2);

	const EvidenceBuilder::BuiltEvidence built = builder.Build();

	bool foundAB = false;
	bool foundAC = false;
	bool foundBC = false;
	bool foundD1D2 = false;
	for (const auto& contradiction : built.contradictions) {
		if (contradiction.a == 1 && contradiction.b == 2) foundAB = true;
		if (contradiction.a == 1 && contradiction.b == 3) foundAC = true;
		if (contradiction.a == 2 && contradiction.b == 3) foundBC = true;
		if (contradiction.a == 4 && contradiction.b == 5) foundD1D2 = true;
	}
	assert(foundAB);
	assert(!foundAC);
	assert(!foundBC);
	assert(foundD1D2);

	std::puts("  - Contradiction detection: OK");
}

void TestCoverage() {
	EvidenceBuilder builder;
	builder.MarkPlannedTask(1);
	builder.MarkPlannedTask(2);
	builder.MarkPlannedTask(3);

	Evidence e1;
	e1.id = 1;
	e1.taskId = 1;
	e1.claim = "task1 evidence";
	builder.Add(e1);

	Evidence e2;
	e2.id = 2;
	e2.taskId = 2;
	e2.claim = "task2 evidence";
	builder.Add(e2);
	// task3にはEvidenceが無い

	const EvidenceBuilder::BuiltEvidence built = builder.Build();
	assert(std::abs(built.coverage - (2.0 / 3.0)) < 1e-9);
	assert(built.tasksWithoutEvidence.size() == 1);
	assert(built.tasksWithoutEvidence[0] == 3);

	// 計画Taskが無ければcoverage=1.0
	EvidenceBuilder emptyPlanBuilder;
	emptyPlanBuilder.Add(e1);
	const EvidenceBuilder::BuiltEvidence builtNoPlan = emptyPlanBuilder.Build();
	assert(std::abs(builtNoPlan.coverage - 1.0) < 1e-9);
	assert(builtNoPlan.tasksWithoutEvidence.empty());

	// ToJsonが例外なく呼べること。
	const Json j = EvidenceBuilder::ToJson(built);
	assert(j.contains("coverage"));
	assert(j.contains("tasksWithoutEvidence"));

	std::puts("  - Coverage math: OK");
}

void TestLogicGraphConfidence() {
	LogicGraph graph;
	const LogicNodeId h1 = graph.AddHypothesis("hypothesis with no support", 0.8);
	assert(graph.ComputeConfidence(h1) == 0.0);

	graph.AddSupport(h1, 100);
	const double oneSupport = graph.ComputeConfidence(h1);
	assert(oneSupport > 0.0);

	graph.AddSupport(h1, 101);
	const double twoSupports = graph.ComputeConfidence(h1);
	assert(twoSupports > oneSupport);

	graph.AddContradiction(h1, 200);
	const double withContradiction = graph.ComputeConfidence(h1);
	assert(std::abs(withContradiction - twoSupports * 0.5) < 1e-9);

	// clamp at rubricBase: 大量の支持でもrubricBaseを超えない。
	const LogicNodeId h2 = graph.AddHypothesis("many supports", 0.5);
	for (int i = 0; i < 50; ++i) {
		graph.AddSupport(h2, 1000 + i);
	}
	const double manySupports = graph.ComputeConfidence(h2);
	assert(manySupports <= 0.5 + 1e-9);
	assert(manySupports > 0.499);

	graph.AddMissingEvidence(h1, "frame 130の再観測が必要");
	const Json j = graph.ToJson();
	assert(j.contains("hypotheses"));

	std::puts("  - LogicGraph confidence: OK");
}

void TestLogicGraphRankStable() {
	LogicGraph graph;
	const LogicNodeId low = graph.AddHypothesis("low confidence, no support", 0.9);
	const LogicNodeId tieA = graph.AddHypothesis("tie A", 0.9);
	const LogicNodeId tieB = graph.AddHypothesis("tie B", 0.9);
	const LogicNodeId high = graph.AddHypothesis("high confidence", 0.9);

	graph.AddSupport(high, 1);
	graph.AddSupport(high, 2);
	graph.AddSupport(high, 3);
	// low, tieA, tieBはsupportなし → confidence 0、追加順で安定ソートされるはず

	const std::vector<LogicGraph::RankedHypothesis> ranked = graph.Rank();
	assert(ranked.size() == 4);
	assert(ranked[0].id == high); // 唯一confidence>0
	assert(ranked[1].id == low);
	assert(ranked[2].id == tieA);
	assert(ranked[3].id == tieB);

	std::puts("  - LogicGraph rank stability: OK");
}

void TestJsonExtractorFenced() {
	const std::string text =
		"モデルの応答です。\n```json\n{\"a\": 1, \"b\": \"hello\"}\n```\nありがとうございました。";
	Json out;
	const Result r = JsonExtractor::Extract(text, &out);
	assert(r.ok);
	assert(out.at("a").get<int>() == 1);
	assert(out.at("b").get<std::string>() == "hello");
	std::puts("  - JsonExtractor fenced json: OK");
}

// 閉じ括弧が足りない出力を修復できること。
//
// 実機（transcript_20260727_042057）: Plannerが
//   {"tasks":[{"taskId":"1", ... ,"allowedTools":["Respond"],"searchHints":[]}]
// と最後の '}' だけを落とした出力を返し、パースに失敗して
// 「tasks array must not be empty」で計画失敗になった。
// stopReason=completed だったのでトークン切れではなく、モデルの取りこぼし。
void TestJsonExtractorRepairsUnterminated() {
	// 実機とほぼ同じ形。末尾の '}' が1つ足りない。
	{
		const std::string text =
			"```json\n"
			"{\"tasks\":[{\"taskId\":\"1\",\"description\":\"挨拶へ応答する\","
			"\"dependencies\":[],\"allowedTools\":[\"Respond\"],\"searchHints\":[]}]\n"
			"```";
		Json out;
		assert(JsonExtractor::Extract(text, &out).ok);
		assert(out.at("tasks").size() == 1);
		assert(out.at("tasks")[0].at("allowedTools")[0].get<std::string>() == "Respond");
	}

	// 末尾カンマと閉じ括弧欠落が重なっても直せること。
	{
		const std::string text = "{\"a\": [1, 2,]";
		Json out;
		assert(JsonExtractor::Extract(text, &out).ok);
		assert(out.at("a").size() == 2);
	}

	// 文字列の途中で切れている場合は修復しない（中身が壊れているため）。
	{
		const std::string text = "{\"a\": \"unterminated";
		Json out;
		assert(!JsonExtractor::Extract(text, &out).ok);
	}

	// 括弧の対応が食い違う場合も触らない。
	{
		const std::string text = "{\"a\": [1, 2}";
		Json out;
		assert(!JsonExtractor::Extract(text, &out).ok);
	}

	std::puts("  - JsonExtractor repairs unterminated braces: OK");
}

void TestJsonExtractorBare() {
	const std::string text = "  {\"c\": 3}  ";
	Json out;
	const Result r = JsonExtractor::Extract(text, &out);
	assert(r.ok);
	assert(out.at("c").get<int>() == 3);
	std::puts("  - JsonExtractor bare json: OK");
}

void TestJsonExtractorEmbeddedInProse() {
	const std::string text =
		"了解しました！以下のとおりです: {invalid json here} and then {\"f\": 6} でした。";
	Json out;
	const Result r = JsonExtractor::Extract(text, &out);
	assert(r.ok);
	assert(out.at("f").get<int>() == 6);
	std::puts("  - JsonExtractor embedded-in-prose (skip first '{'): OK");
}

void TestJsonExtractorTrailingCommaRepair() {
	const std::string text = "```json\n{\"d\": 4, \"arr\": [1, 2, ], }\n```";
	Json out;
	const Result r = JsonExtractor::Extract(text, &out);
	assert(r.ok);
	assert(out.at("d").get<int>() == 4);
	assert(out.at("arr").size() == 2);
	std::puts("  - JsonExtractor trailing-comma repair: OK");
}

void TestJsonExtractorThinkBlockIgnored() {
	const std::string text =
		"<think>ここは無視されるべき内容 {\"fake\": 1}</think>\n```json\n{\"e\": 5}\n```";
	Json out;
	const Result r = JsonExtractor::Extract(text, &out);
	assert(r.ok);
	assert(out.contains("e"));
	assert(!out.contains("fake"));
	assert(out.at("e").get<int>() == 5);
	std::puts("  - JsonExtractor <think> block ignored: OK");
}

void TestJsonExtractorGarbageFails() {
	const std::string text = "no json here at all, just prose.";
	Json out;
	const Result r = JsonExtractor::Extract(text, &out);
	assert(!r.ok);
	assert(!r.error.empty());
	std::puts("  - JsonExtractor garbage -> Fail: OK");
}

void TestMockLlmBackend() {
	MockLlmBackend llm;
	llm.AddRule("SPECIAL_MARKER", "RULE_RESPONSE");
	llm.EnqueueResponse("FIFO_1");
	llm.EnqueueResponse("FIFO_2");

	// ルール一致（システムプロンプト経由）
	const std::string r1 = llm.Generate("sys SPECIAL_MARKER", "user1");
	assert(r1 == "RULE_RESPONSE");

	// ルールは消費されない（何度でもマッチ）
	const std::string r1b = llm.Generate("sys SPECIAL_MARKER", "user2");
	assert(r1b == "RULE_RESPONSE");

	// ルール不一致 → FIFO消費
	const std::string r2 = llm.Generate("sys", "user3");
	assert(r2 == "FIFO_1");
	const std::string r3 = llm.Generate("sys", "user4");
	assert(r3 == "FIFO_2");

	// FIFO枯渇 → デフォルト "{}"
	const std::string r4 = llm.Generate("sys", "user5");
	assert(r4 == "{}");

	const std::vector<std::pair<std::string, std::string>> calls = llm.GetCalls();
	assert(calls.size() == 5);
	assert(calls[0].first == "sys SPECIAL_MARKER");
	assert(calls[0].second == "user1");
	assert(calls[4].second == "user5");

	std::puts("  - MockLlmBackend rule/FIFO/default: OK");
}

void TestPromptTemplates() {
	const PromptPair intakePair = prompts::Intake("カメラのジッターを直したい");
	assert(!intakePair.system.empty());
	assert(!intakePair.user.empty());
	assert(intakePair.user.find("カメラのジッターを直したい") != std::string::npos);
	// IntakeスキーマがrequestType（conversation/investigation分類）へ言及していること。
	assert(intakePair.system.find("requestType") != std::string::npos);
	assert(intakePair.system.find("conversation") != std::string::npos);
	assert(intakePair.system.find("investigation") != std::string::npos);

	const Json intakeResult = Json::object({{"goal", "fix jitter"}});
	// toolCatalogはDescribeTools()の形式（name/description/requiredPermission/argumentSchema）
	// を模したオブジェクト配列にする（CompactToolCatalogが解釈できる形）。
	const Json toolCatalog = Json::array({
		Json::object({{"name", "ReadComponent"}, {"description", "Componentを読む"},
			{"requiredPermission", "Read"}, {"argumentSchema", Json::object()}}),
		Json::object({{"name", "ListSystems"}, {"description", "System一覧を返す"},
			{"requiredPermission", "Observe"}, {"argumentSchema", Json::object()}})
	});
	const std::string compactCatalog = prompts::CompactToolCatalog(toolCatalog);

	const PromptPair planPair = prompts::Plan(intakeResult, toolCatalog, 5);
	assert(!planPair.system.empty());
	assert(!planPair.user.empty());
	assert(planPair.user.find(intakeResult.dump(2)) != std::string::npos);
	// dump(2)ではなくCompactToolCatalog()の圧縮表現が埋め込まれていること
	// （実機ログ: dump(2)で~1600 prompt tokensかかっていた問題への対処）。
	assert(planPair.user.find(compactCatalog) != std::string::npos);
	assert(planPair.user.find(toolCatalog.dump(2)) == std::string::npos);

	const Json taskSpec = Json::object({{"taskId", 1}, {"type", "CodeSearch"}});
	const PromptPair queryPair = prompts::GenerateQueries(taskSpec, toolCatalog);
	assert(!queryPair.system.empty());
	assert(!queryPair.user.empty());
	assert(queryPair.user.find(taskSpec.dump(2)) != std::string::npos);
	assert(queryPair.user.find(compactCatalog) != std::string::npos);

	const Json builtEvidenceJson = Json::object({{"evidences", Json::array()}});
	const PromptPair reasonPair = prompts::Reason(builtEvidenceJson);
	assert(!reasonPair.system.empty());
	assert(!reasonPair.user.empty());
	// Reasonは生Evidenceではなく圧縮版を埋め込む（EvidencePromptCompressor）。
	// 以前は dump(2) の整形済みJSONを期待していたが、圧縮の導入で
	// prompt側に整形済みJSONが現れなくなったため、期待値を実態へ合わせる。
	assert(reasonPair.user.find(
		evidence_prompt::CompressToString(builtEvidenceJson, 9800)) != std::string::npos);

	const Json hypotheses = Json::object({{"hypotheses", Json::array()}});
	const PromptPair critiquePair = prompts::Critique(hypotheses, builtEvidenceJson);
	assert(!critiquePair.system.empty());
	assert(!critiquePair.user.empty());
	assert(critiquePair.user.find(hypotheses.dump(2)) != std::string::npos);

	const Json stopInfo = Json::object({{"reason", "budget exhausted"}});
	const PromptPair synthesizePair = prompts::Synthesize(builtEvidenceJson, hypotheses, stopInfo);
	assert(!synthesizePair.system.empty());
	assert(!synthesizePair.user.empty());
	assert(synthesizePair.user.find(stopInfo.dump(2)) != std::string::npos);

	// Respond: 会話応答ツールのプロンプト。
	// 旧DirectReply（会話高速パス）は廃止した。会話も調査も同じパイプラインを通り、
	// 応答生成はツールとして計画される。
	const PromptPair respondPair = prompts::Respond("あなたは何ができますか？", builtEvidenceJson);
	assert(!respondPair.system.empty());
	assert(!respondPair.user.empty());
	assert(respondPair.system.find("応答担当") != std::string::npos);
	assert(respondPair.system.find("\"reply\"") != std::string::npos);
	assert(respondPair.user.find("あなたは何ができますか？") != std::string::npos);
	(void)compactCatalog;

	std::puts("  - PromptTemplates: OK");
}

void TestCompactToolCatalog() {
	// 小さな合成Tool catalog（DescribeTools()互換の形）でCompactToolCatalogの
	// 出力を検証する: 1Tool=1行、必須引数はそのまま、任意引数には"?"が付き、
	// 末尾に[Permission]が付くこと。
	Json catalog = Json::array();
	{
		Json tool = Json::object();
		tool["name"] = "DescribeEntity";
		tool["description"] = "指定Entityの状態を返す";
		tool["requiredPermission"] = "Read";
		Json schema = Json::object();
		schema["entityName"] = Json::object({{"type", "string"}, {"required", true}});
		schema["verbose"] = Json::object({{"type", "boolean"}, {"required", false}});
		tool["argumentSchema"] = schema;
		catalog.push_back(tool);
	}
	{
		// argumentSchema欠損Tool（引数なしTool）も扱えること。
		Json tool = Json::object();
		tool["name"] = "ListSystems";
		tool["description"] = "System一覧を返す";
		tool["requiredPermission"] = "Observe";
		catalog.push_back(tool);
	}

	const std::string compact = prompts::CompactToolCatalog(catalog);

	// 1行目: DescribeEntity。必須引数はそのまま、任意引数には"?"が付く。
	assert(compact.find("- DescribeEntity(entityName:string, verbose?:boolean) : "
	                     "指定Entityの状態を返す [Read]") != std::string::npos);
	// 2行目: argumentSchema欠損 → 括弧内は空。
	assert(compact.find("- ListSystems() : System一覧を返す [Observe]") != std::string::npos);

	// 入力順を保つこと（DescribeEntityがListSystemsより前に現れる）。
	assert(compact.find("DescribeEntity") < compact.find("ListSystems"));

	// dump(2)より大幅に短いこと（トークン圧縮が目的のため）。
	assert(compact.size() < catalog.dump(2).size());

	// 1Tool=1行であること: catalogは2Tool → '\n'はちょうど2個。
	const auto newlineCount = std::count(compact.begin(), compact.end(), '\n');
	assert(newlineCount == 2);

	std::puts("  - CompactToolCatalog: OK");
}

} // namespace

int main() {
	TestEvidenceJsonRoundTrip();
	TestDedup();
	TestContradictionDetection();
	TestCoverage();
	TestLogicGraphConfidence();
	TestLogicGraphRankStable();
	TestJsonExtractorFenced();
	TestJsonExtractorRepairsUnterminated();
	TestJsonExtractorBare();
	TestJsonExtractorEmbeddedInProse();
	TestJsonExtractorTrailingCommaRepair();
	TestJsonExtractorThinkBlockIgnored();
	TestJsonExtractorGarbageFails();
	TestMockLlmBackend();
	TestPromptTemplates();
	TestCompactToolCatalog();

	std::puts("AgentOSEvidenceLogicSmokeTest: OK");
	return 0;
}
