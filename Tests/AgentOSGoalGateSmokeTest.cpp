// =======================================================================
//
// AgentOSGoalGateSmokeTest.cpp
//
// 実機失敗事例（EVIDENCE）: 「Playerのジャンプ力を教えて」がresolvedRequest
// 「Entity 'Player' のコンポーネントを調査し、ジャンプ力の値を読み取る」へ
// 解決された後、PlannerAgentのTryBuildSceneSnapshotPlanが症状/制約に混入した
// 「Scene」「状態」等の汚染語で誤って決定的Scene Snapshot経路へ捕捉し、
// CriticAgentはSnapshot Evidenceが内部的に完結していることだけを理由に
// pass=trueを返していた（confidence 0.97）。実際にはJumpForceが一度も
// 調査されておらず、最終報告自体が「調査未完了」と認めていたにもかかわらず
// Repairが一切発火しなかった。
//
// 本テストは2つの修正を検証する:
//   1) PlannerAgent: ルーティング判定テキストをresolvedRequestのみに限定し、
//      具体的な対象（Entity/属性）を名指しした要求はScene Snapshot経路から
//      除外する（specificTargetRequest）。
//   2) CriticAgent: resolvedRequestから抽出した目的識別子がEvidence
//      （claim+payload）に一切現れない場合はhard failにするゲート#8を追加
//      する。IsCompleteSceneSnapshotの決定的バイパスにも必ず適用される。
//
// エンジン規約どおり自己完結main()+assert方式。
//
// =======================================================================
#include "AgentOS/Core/Agents/AgentContext.h"
#include "AgentOS/Core/Agents/CriticAgent.h"
#include "AgentOS/Core/Agents/PlannerAgent.h"
#include "AgentOS/Core/Json.h"
#include "AgentOS/Core/Llm/MockLlmBackend.h"
#include "AgentOS/Core/Llm/PromptTemplates.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace agentos;

namespace {

// ---------------------------------
// 共通ヘルパ: 「Scene Snapshot完結」形のBuiltEvidenceを組み立てる。
// AgentOSSceneSnapshotFastPathSmokeTest.cppのFakeTool群と同じ内容
// （Player/MainCamera、TransformSystem/RenderSystem）を模す。
// ---------------------------------
Json BuildSnapshotCompleteEvidence() {
	return Json::object({
		{"coverage", 1.0},
		{"failedEvidenceCount", 0},
		{"evidences", Json::array({
			Json::object({
				{"id", 1},
				{"taskId", 1},
				{"claim", "生存Entityは2件"},
				{"payload", Json::object({
					{"entities", Json::array({
						Json::object({{"name", "Player"}, {"id", 1}}),
						Json::object({{"name", "MainCamera"}, {"id", 2}}),
					})},
				})},
				{"provenance", Json::object({
					{"sourceType", "Tool:ListEntities"}, {"sourceUri", "ListEntities"},
					{"session", "s1"}, {"frame", -1},
				})},
				{"confidence", 1.0},
			}),
			Json::object({
				{"id", 2},
				{"taskId", 2},
				{"claim", "登録SystemはTransformSystemとRenderSystem"},
				{"payload", Json::object({
					{"systems", Json::array({"TransformSystem", "RenderSystem"})},
				})},
				{"provenance", Json::object({
					{"sourceType", "Tool:ListSystems"}, {"sourceUri", "ListSystems"},
					{"session", "s1"}, {"frame", -1},
				})},
				{"confidence", 1.0},
			}),
		})},
	});
}

void SetResolvedRequest(const std::string& resolvedRequest) {
	prompts::ClearCurrentConversationRequestContext();
	prompts::SetCurrentConversationRequestContext(
		Json::object(),
		Json::object({{"resolvedRequest", resolvedRequest}}));
}

// =======================================================================
// 1) ExtractGoalIdentifiers 単体テスト
// =======================================================================
void TestExtractGoalIdentifiers() {
	// 引用トークン
	{
		const auto ids = critic_internal::ExtractGoalIdentifiers("Entity 'Player' を調べる");
		bool found = false;
		for (const auto& id : ids) if (id == "Player") found = true;
		assert(found);
	}
	// 二重引用符も対応
	{
		const auto ids = critic_internal::ExtractGoalIdentifiers("Entity \"Boss\" を調べる");
		bool found = false;
		for (const auto& id : ids) if (id == "Boss") found = true;
		assert(found);
	}
	// ASCII識別子（3文字以上）
	{
		const auto ids = critic_internal::ExtractGoalIdentifiers("JumpForceを読み取る");
		bool found = false;
		for (const auto& id : ids) if (id == "JumpForce") found = true;
		assert(found);
	}
	// ASCII stoplist（Entity/Component/Scene/System/Tool等は除外）
	{
		const auto ids = critic_internal::ExtractGoalIdentifiers(
			"Entity Component Scene System Tool the and");
		assert(ids.empty());
	}
	// カタカナ連続（3文字以上）: "ジャンプ" (力は漢字なので連続が切れる)
	{
		const auto ids = critic_internal::ExtractGoalIdentifiers("ジャンプ力を読み取る");
		bool found = false;
		for (const auto& id : ids) if (id == "ジャンプ") found = true;
		assert(found);
	}
	// カタカナstoplist: 「シーン」はSceneSnapshot系要求文の一般語のため除外
	{
		const auto ids = critic_internal::ExtractGoalIdentifiers("現在のシーンの一覧");
		for (const auto& id : ids) assert(id != "シーン");
	}
	// 2文字以下のカタカナ連続は識別子にしない
	{
		const auto ids = critic_internal::ExtractGoalIdentifiers("アイの設定を教えて");
		for (const auto& id : ids) assert(id != "アイ");
	}
	// 重複排除（大小無視でASCII）
	{
		const auto ids = critic_internal::ExtractGoalIdentifiers("Player player PLAYER");
		int count = 0;
		for (const auto& id : ids) {
			(void)id;
			++count;
		}
		assert(count == 1);
	}

	std::cout << "  - ExtractGoalIdentifiers (quoted/ASCII/katakana/stoplist): OK\n";
}

// =======================================================================
// 2) CriticAgent ゲート#8: Snapshot完結でも目的未達ならFAIL
// =======================================================================
void TestCriticFailsOnUncoveredGoalIdentifier() {
	SetResolvedRequest("Entity 'Player' の ジャンプ力 (JumpForce) を読み取る");

	AgentContext ctx; // llmはnullptrのまま。SnapshotバイパスはLLM呼び出しをしない。
	const Json rankedHypotheses = Json::object({{"hypotheses", Json::array()}});
	const Json builtEvidence = BuildSnapshotCompleteEvidence();

	CriticVerdict verdict;
	const Result result = CriticAgent::Run(ctx, rankedHypotheses, builtEvidence, &verdict);
	assert(result.ok);

	// JumpForce/ジャンプはEvidence（claim/payload）に一切現れないため、
	// Snapshotとして内部完結していてもpass=falseでなければならない。
	assert(!verdict.pass);

	bool sawGoalFailure = false;
	for (const auto& failure : verdict.failures) {
		if (failure.find("goal identifiers not covered") != std::string::npos) sawGoalFailure = true;
	}
	assert(sawGoalFailure);

	// Repairループが実際に到達できるよう、additionalTaskが合成されていること。
	assert(verdict.additionalTasks.is_array());
	assert(!verdict.additionalTasks.empty());
	const Json& task = verdict.additionalTasks[0];
	assert(task.value("type", std::string()) == "RuntimeObservation");
	assert(task.value("description", std::string()).find("不足識別子") != std::string::npos);

	prompts::ClearCurrentConversationRequestContext();
	std::cout << "  - CriticAgent gate#8 fails on uncovered goal identifier (JumpForce): OK\n";
}

// =======================================================================
// 3) CriticAgent ゲート#8: 目的識別子がstoplistのみ、または実際にカバー
//    されている場合はPASSすること。
// =======================================================================
void TestCriticPassesWhenGoalIdentifiersCoveredOrGeneric() {
	const Json builtEvidence = BuildSnapshotCompleteEvidence();
	AgentContext ctx;
	const Json rankedHypotheses = Json::object({{"hypotheses", Json::array()}});

	// 3a: 「Entity」「シーン」はどちらもstoplist語のため、識別子が空になり
	//     ゲート#8は無評価でpassする（AgentOSSceneSnapshotFastPathSmokeTest
	//     が使う実際のgoal文とほぼ同型）。
	{
		SetResolvedRequest("現在のシーンのEntity一覧");
		CriticVerdict verdict;
		const Result result = CriticAgent::Run(ctx, rankedHypotheses, builtEvidence, &verdict);
		assert(result.ok);
		assert(verdict.pass);
		prompts::ClearCurrentConversationRequestContext();
	}

	// 3b: stoplist対象外の識別子（Player）が実際にEvidenceでカバーされて
	//     いる場合もpassする（空リストの空判定に頼らない実質的な検証）。
	{
		SetResolvedRequest("現在のシーンのPlayerを含むEntity一覧を取得する");
		CriticVerdict verdict;
		const Result result = CriticAgent::Run(ctx, rankedHypotheses, builtEvidence, &verdict);
		assert(result.ok);
		assert(verdict.pass);
		prompts::ClearCurrentConversationRequestContext();
	}

	std::cout << "  - CriticAgent gate#8 passes on stoplisted/covered identifiers: OK\n";
}

// =======================================================================
// 4) PlannerAgent: 具体的対象要求はScene Snapshot経路から除外される。
// =======================================================================
void TestPlannerExcludesSpecificTargetFromSnapshotRoute() {
	const Json toolCatalog = Json::array({
		Json::object({
			{"name", "ListEntities"}, {"description", "Entity一覧"},
			{"requiredPermission", "Read"}, {"argumentSchema", Json::object()},
		}),
		Json::object({
			{"name", "ListSystems"}, {"description", "System一覧"},
			{"requiredPermission", "Read"}, {"argumentSchema", Json::object()},
		}),
	});

	// EVIDENCE再現: resolvedRequestが特定Entity('Player')の特定属性を
	// 名指ししており、symptoms/constraintsには過去セッションの汚染語
	// （シーン/状態）が混入している。旧実装ではsymptoms経由でsceneRequest/
	// snapshotRequestが誤ってtrueになり、決定的Scene Snapshot経路へ捕捉
	// されていた。
	const Json intake = Json::object({
		{"goal", "Playerのジャンプ力を調べる"},
		{"resolvedRequest", "Entity 'Player' のジャンプ力を読み取る"},
		{"symptoms", Json::array({
			"過去の調査でシーン全体の状態確認が必要だった",
			"シーンの状態がまだ不明という報告が残っている",
		})},
		{"constraints", Json::array()},
	});

	MockLlmBackend llm;
	llm.AddRule("Planner担当",
		"```json\n"
		"{\"tasks\": [{\"taskId\": \"T1\", \"type\": \"RuntimeObservation\", "
		"\"description\": \"Player Entityの詳細を調べる\", \"dependencies\": [], "
		"\"allowedTools\": [\"ListEntities\"], \"searchHints\": [\"Player\"]}]}\n"
		"```");

	AgentContext ctx;
	ctx.llm = &llm;

	Json plan;
	const Result result = PlannerAgent::Run(ctx, intake, toolCatalog, &plan);
	assert(result.ok);

	// 決定的Scene Snapshot経路が発火していないこと（route未設定）。
	assert(plan.value("route", std::string()).empty());
	// 実際にLLM Planner（"Planner担当"）が呼ばれたこと（Fast Pathを迂回して
	// いないことの直接的な証拠）。
	bool sawPlannerCall = false;
	for (const auto& call : llm.GetCalls()) {
		if (call.first.find("Planner担当") != std::string::npos) sawPlannerCall = true;
	}
	assert(sawPlannerCall);

	std::cout << "  - PlannerAgent: specific-target request bypasses Scene Snapshot route: OK\n";
}

// =======================================================================
// 5) PlannerAgent: 汎用Scene Snapshot要求は引き続き決定的経路が発火する
//    こと（回帰確認）。
// =======================================================================
void TestPlannerStillFiresGenericSnapshotRoute() {
	const Json toolCatalog = Json::array({
		Json::object({
			{"name", "ListEntities"}, {"description", "Entity一覧"},
			{"requiredPermission", "Read"}, {"argumentSchema", Json::object()},
		}),
		Json::object({
			{"name", "ListSystems"}, {"description", "System一覧"},
			{"requiredPermission", "Read"}, {"argumentSchema", Json::object()},
		}),
	});

	const Json intake = Json::object({
		{"goal", "現在のシーンの状況を報告する"},
		{"resolvedRequest", "現在のシーンの状況を報告して"},
		{"symptoms", Json::array()},
		{"constraints", Json::array()},
	});

	// Plannerルールをあえて登録しない: 決定的経路が発火しなければ
	// デフォルト応答"{}"を消費してPlan検証に失敗し、テストが確実に落ちる。
	MockLlmBackend llm;
	AgentContext ctx;
	ctx.llm = &llm;

	Json plan;
	const Result result = PlannerAgent::Run(ctx, intake, toolCatalog, &plan);
	assert(result.ok);
	assert(plan.value("route", std::string()) == "deterministic_scene_snapshot");
	assert(llm.GetCalls().empty());

	std::cout << "  - PlannerAgent: generic scene snapshot request still deterministic: OK\n";
}

} // namespace

int main() {
	std::cout << "=== AgentOS Goal Gate Smoke Test ===\n";

	TestExtractGoalIdentifiers();
	TestCriticFailsOnUncoveredGoalIdentifier();
	TestCriticPassesWhenGoalIdentifiersCoveredOrGeneric();
	TestPlannerExcludesSpecificTargetFromSnapshotRoute();
	TestPlannerStillFiresGenericSnapshotRoute();

	std::cout << "=== ALL PASSED ===\n";
	return 0;
}
