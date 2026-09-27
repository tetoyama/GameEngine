// =======================================================================
//
// AgentOSRespondToolSmokeTest.cpp
//
// 会話応答をツールにしたことを検証する。
//
// 廃止した仕組み:
//   要求の種類（会話 / 調査）をIntakeのrequestTypeとキーワード一致で
//   先回りに分類し、会話と判定したら調査パイプラインを丸ごと飛ばす
//   3段のfast path（DirectReply / quick tool path / conversational fast path）。
//
// 廃止した理由（実機、transcript_20260727_0208xx）:
//   「私は誰ですか？」   → investigation と分類され、身元をSceneに探しに行き
//                          修復2ラウンド空振りで未完了になった。
//   「私の名前はTaroです」→ conversation と分類されたが、quick tool pathが
//                          ResolveEntity('Taro') を実行し「見つかりません」と答えた。
//   分類が外れるたびにキーワードを足す構造で、判定が2ファイル6関数に散っていた。
//
// いまの形:
//   会話応答も Respond という1つのツール。Plannerがツール一覧から普通に選ぶ。
//   完了かどうかはCriticがその出力を見て判定する。ツール一覧が正本である。
//
// =======================================================================
#include "AgentOS/Core/Agents/AgentContext.h"
#include "AgentOS/Core/Agents/CriticAgent.h"
#include "AgentOS/Core/Agents/PlannerAgent.h"
#include "AgentOS/Core/Command/CapabilitySet.h"
#include "AgentOS/Core/Command/CommandPipeline.h"
#include "AgentOS/Core/Conversation/RespondTool.h"
#include "AgentOS/Core/Json.h"
#include "AgentOS/Core/Llm/MockLlmBackend.h"
#include "AgentOS/Core/Llm/PromptTemplates.h"

#include <cassert>
#include <cstdio>
#include <string>

using namespace agentos;

namespace {

CommandRequest MakeRequest(const Json& arguments, const CapabilityToken& token) {
	CommandRequest request;
	request.issuer = token.owner;
	request.tool = RespondToolName();
	request.arguments = arguments;
	request.capability = token;
	return request;
}

// ---------------------------------
// 1) Respondがツールとして実行でき、応答本文をEvidenceに載せること
// ---------------------------------
void TestRespondProducesReplyEvidence() {
	MockLlmBackend llm;
	llm.AddRule("応答担当",
		"```json\n{\"reply\": \"私はこのエンジンの調査を手伝えます。\"}\n```");

	CapabilityRegistry registry;
	CommandPipeline pipeline(&registry);
	RegisterRespondTool(pipeline, [&]() -> ILlmBackend* { return &llm; });
	const CapabilityToken token = registry.IssueToken("test", {"*"}, PermissionLevel::Read);

	const CommandResult result = pipeline.Submit(
		MakeRequest(Json::object({{"instruction", "何ができるか説明する"}}), token));
	assert(result.status == CommandStatus::Ok);

	// claimはEvidenceの見出しになる。応答本文そのものを載せること。
	// 見出しに要約を置くと、Synthesisが本文ではなく要約を最終応答にしてしまう
	// （GetSymbolInfoで「場所だけ答える」故障を踏んでいる）。
	assert(result.payload.value("reply", std::string()).find("調査を手伝えます") != std::string::npos);
	assert(result.payload.value("claim", std::string()) == result.payload.value("reply", std::string()));

	// instructionは必須。
	assert(pipeline.Submit(MakeRequest(Json::object(), token)).status != CommandStatus::Ok);

	std::printf("  [ok] Respond executes as a tool and carries the reply itself\n");
}

// ---------------------------------
// 2) 応答生成に失敗したら失敗Evidenceにすること
// ---------------------------------
void TestRespondFailureIsNotSilent() {
	MockLlmBackend llm;
	llm.AddRule("応答担当", "```json\n{\"reply\": \"\"}\n```");

	CapabilityRegistry registry;
	CommandPipeline pipeline(&registry);
	RegisterRespondTool(pipeline, [&]() -> ILlmBackend* { return &llm; });
	const CapabilityToken token = registry.IssueToken("test", {"*"}, PermissionLevel::Read);

	// 空文字を成功として返すと、Criticが「応答した」と誤認する。
	const CommandResult result = pipeline.Submit(
		MakeRequest(Json::object({{"instruction", "説明する"}}), token));
	assert(result.status != CommandStatus::Ok);

	std::printf("  [ok] empty reply is recorded as a failure, not silently accepted\n");
}

// ---------------------------------
// 3) 観測要件は「使ったツール」で決まること
// ---------------------------------
Json EvidenceFrom(const char* sourceType, const char* claim) {
	return Json::object({
		{"id", 1},
		{"taskId", 1},
		{"confidence", 1.0},
		{"claim", claim},
		{"payload", Json::object({{"claim", claim}})},
		{"provenance", Json::object({
			{"sourceType", sourceType}, {"sourceUri", "x"},
			{"session", "s1"}, {"frame", -1},
		})},
	});
}

bool HasFailure(const CriticVerdict& verdict, const char* needle) {
	for (const auto& failure : verdict.failures) {
		if (failure.find(needle) != std::string::npos) return true;
	}
	return false;
}

void RunCritic(const Json& evidences, CriticVerdict* out) {
	prompts::ClearCurrentConversationRequestContext();
	prompts::SetCurrentConversationRequestContext(
		Json::object(),
		Json::object({
			{"currentUserInput", "あなたは何ができますか"},
			{"resolvedRequest", "AgentOSの能力を説明する"},
		}));

	const Json ranked = Json::object({{"hypotheses", Json::array({Json::object({
		{"id", 1}, {"text", "能力についての応答を提示した"}, {"confidence", 0.9},
		{"supports", Json::array({1})}, {"contradicts", Json::array()},
		{"missingEvidence", Json::array()},
	})})}});

	const Json built = Json::object({
		{"coverage", 1.0},
		{"tasksWithoutEvidence", Json::array()},
		{"failedEvidenceCount", 0},
		{"usableEvidenceCount", evidences.size()},
		{"contradictions", Json::array()},
		{"evidences", evidences},
	});

	AgentContext ctx; // llmはnullptr。LLM所見はadvisoryなので決定的ゲートは効く。
	CriticAgent::Run(ctx, ranked, built, out);
	prompts::ClearCurrentConversationRequestContext();
}

void TestObservationRequirementFollowsToolsUsed() {
	const char* kObservationNeeded = "supported only by conversation references";

	// 3a: Respondだけのプラン。観測ツールを使っていないので観測要件は適用しない。
	//     適用すると挨拶や「私は誰ですか？」が構造的に必ず未完了になる。
	{
		CriticVerdict verdict;
		RunCritic(Json::array({EvidenceFrom("Tool:Respond", "私はエンジンの調査を手伝えます。")}), &verdict);
		assert(!HasFailure(verdict, kObservationNeeded));
	}

	// 3b: 観測ツールが混ざったプラン。こちらはEngine/コードの事実を主張して
	//     いるので観測要件が効く。参照系だけで支えていれば落とす。
	{
		CriticVerdict verdict;
		RunCritic(Json::array({
			Json::object({
				{"id", 1}, {"taskId", 1}, {"confidence", 1.0},
				{"claim", "過去のやり取りから記録を取得した。"},
				{"payload", Json::object({
					{"count", 1}, {"observationCount", 0},
					{"entries", Json::array({Json::object({
						{"kind", "assistantTurn"}, {"role", "reference"},
					})})},
				})},
				{"provenance", Json::object({
					{"sourceType", "Tool:GetConversationHistory"}, {"sourceUri", "x"},
					{"session", "s1"}, {"frame", -1},
				})},
			}),
		}), &verdict);
		assert(HasFailure(verdict, kObservationNeeded));
	}

	std::printf("  [ok] observation requirement is decided by the tools used, not by request kind\n");
}

// ---------------------------------
// 4) 応答Taskだけの計画が通ること（Task種別廃止の回帰テスト）
// ---------------------------------
// 以前はTask種別が RuntimeObservation/CodeSearch/Trace/Analysis の4つで、
// すべて「調べる」の種類だった。応答を置く種別が無いため、Respondが
// ツール一覧にあってもPlannerが計画へ入れられず、実機では「こんにちは」に
// ListEntities と WriteTrace を並べた計画が出た（transcript_20260727_0332xx）。
void TestPlanWithOnlyRespondIsValid() {
	const Json toolCatalog = Json::array({
		Json::object({
			{"name", "Respond"}, {"description", "応答文を作る"},
			{"requiredPermission", "Read"},
			{"argumentSchema", Json::object({
				{"instruction", Json::object({{"type", "string"}, {"required", true}})},
			})},
		}),
	});

	MockLlmBackend llm;
	llm.AddRule("Planner担当",
		"```json\n"
		"{\"tasks\": [{\"taskId\": \"T1\", \"description\": \"挨拶へ応答する\", "
		"\"dependencies\": [], \"allowedTools\": [\"Respond\"], \"searchHints\": []}]}\n"
		"```");

	AgentContext ctx;
	ctx.llm = &llm;

	Json plan;
	const Result result = PlannerAgent::Run(
		ctx,
		Json::object({
			{"goal", "ユーザーの挨拶へ応答する"},
			{"resolvedRequest", "挨拶へ応答する"},
		}),
		toolCatalog,
		&plan);

	// typeが無くても計画として妥当であること。
	assert(result.ok);
	assert(plan.at("tasks").size() == 1);
	assert(!plan.at("tasks")[0].contains("type"));
	assert(plan.at("tasks")[0].at("allowedTools")[0].get<std::string>() == "Respond");

	std::printf("  [ok] a plan made only of Respond is valid (no task type)\n");
}

} // namespace

int main() {
	std::printf("=== AgentOS Respond Tool Smoke Test ===\n");

	TestRespondProducesReplyEvidence();
	TestRespondFailureIsNotSilent();
	TestObservationRequirementFollowsToolsUsed();
	TestPlanWithOnlyRespondIsValid();

	std::printf("=== ALL PASSED ===\n");
	return 0;
}
