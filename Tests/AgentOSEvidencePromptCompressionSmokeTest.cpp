// =======================================================================
//
// AgentOSEvidencePromptCompressionSmokeTest.cpp
//
// 巨大な初期Evidenceがあっても、後から得たRepair EvidenceがReason/Critic/
// SynthesisのPromptから押し出されないことを検証する。
//
// =======================================================================
#include "AgentOS/Core/Evidence/EvidencePromptCompressor.h"
#include "AgentOS/Core/Llm/PromptTemplates.h"
#include "AgentOS/Core/Orchestrator/EarlyStopping.h"
#include "AgentOS/Core/Orchestrator/Orchestrator.h"

#include <cassert>
#include <iostream>
#include <string>
#include <utility>

using namespace agentos;

namespace {

Json MakeEvidence(
	long long id,
	long long taskId,
	const std::string& claim,
	const std::string& sourceType,
	Json payload) {
	return Json::object({
		{"id", id},
		{"taskId", taskId},
		{"claim", claim},
		{"payload", std::move(payload)},
		{"provenance", Json::object({
			{"sourceType", sourceType},
			{"sourceUri", sourceType},
			{"session", "test"},
			{"frame", -1},
		})},
		{"confidence", 1.0},
	});
}

Json BuildLargeEvidence() {
	const std::string hugeBody(30000, 'A');
	return Json::object({
		{"coverage", 1.0},
		{"tasksWithoutEvidence", Json::array()},
		{"usableEvidenceCount", 3},
		{"failedEvidenceCount", 0},
		{"supersededEvidenceCount", 0},
		{"activeRevision", 0},
		{"retiredTasks", Json::array()},
		{"contradictions", Json::array()},
		{"evidences", Json::array({
			MakeEvidence(
				1, 10, "初期検索で巨大なコード断片を取得した", "Tool:CodeSearch",
				Json::object({{"body", hugeBody}, {"query", "initial query"}})),
			MakeEvidence(
				2, 11, "中間調査結果", "Tool:CodeSearch",
				Json::object({{"code", "intermediate result"}})),
			MakeEvidence(
				3, 12, "LATEST_REPAIR_EVIDENCEを取得した", "Tool:FindCodeReferences",
				Json::object({
					{"query", "latest repair query"},
					{"code", "LATEST_REPAIR_MARKER"},
					{"count", 2},
					{"complete_scan", true},
				})),
		})},
	});
}

void TestLatestEvidenceIsPrioritized() {
	const Json built = BuildLargeEvidence();
	const Json compressed = evidence_prompt::Compress(built, 5000);
	const std::string text = compressed.dump();

	assert(text.size() <= 5000);
	assert(compressed.value("coverage", 0.0) == 1.0);
	assert(compressed.value("failedEvidenceCount", 1) == 0);
	assert(compressed.contains("evidences"));
	assert(compressed.at("evidences").is_array());
	assert(!compressed.at("evidences").empty());
	assert(compressed.at("evidences").at(0).value("id", 0LL) == 3);
	assert(text.find("LATEST_REPAIR_EVIDENCE") != std::string::npos);
	assert(text.find("LATEST_REPAIR_MARKER") != std::string::npos);
	assert(text.find(std::string(2000, 'A')) == std::string::npos);

	assert(compressed.contains("recentEvidenceDetails"));
	assert(compressed.at("recentEvidenceDetails").is_array());
	assert(!compressed.at("recentEvidenceDetails").empty());
	assert(compressed.at("recentEvidenceDetails").at(0).value("id", 0LL) == 3);
}

void TestAllEvidencePromptsUseCompression() {
	prompts::ClearCurrentConversationRequestContext();
	prompts::SetCurrentConversationRequestContext(
		Json::object(),
		Json::object({
			{"goal", "大規模コード調査"},
			{"resolvedRequest", "複数段階でコード経路を追跡する"},
		}));
	const Json built = BuildLargeEvidence();
	const Json hypotheses = Json::object({
		{"hypotheses", Json::array({Json::object({
			{"description", "追加Evidenceを評価する"},
			{"supports", Json::array({3})},
			{"contradicts", Json::array()},
			{"missingEvidence", Json::array()},
		})})},
	});

	const PromptPair reason = prompts::Reason(built);
	const PromptPair critic = prompts::Critique(hypotheses, built);
	const PromptPair synthesis = prompts::Synthesize(
		built, hypotheses, Json::object({{"reason", "critic passed"}}));

	for(const PromptPair* prompt : {&reason, &critic, &synthesis}) {
		assert(prompt->user.find("LATEST_REPAIR_EVIDENCE") != std::string::npos);
		assert(prompt->user.find("LATEST_REPAIR_MARKER") != std::string::npos);
		assert(prompt->user.find(std::string(2000, 'A')) == std::string::npos);
	}
	prompts::ClearCurrentConversationRequestContext();
}

void TestLargeTaskDefaults() {
	const OrchestratorConfig config;
	assert(config.maxRepairRounds >= 100000);
	assert(config.budget.maxToolCalls >= 200);
	assert(config.budget.maxLlmCalls >= 100);
	assert(config.budget.maxLlmChars >= 1000000);
	assert(config.budget.maxMillis >= 3600000);
}

void TestProgressPreventsEarlyStop() {
	Budget budget;
	BudgetTracker tracker(budget);

	EarlyStopping progressing;
	for(int round = 0; round < 6; ++round) {
		progressing.RecordRound(1, 100 + round, 0, true);
		assert(!progressing.Evaluate(tracker).stop);
	}

	// Evidenceが増えず、同じ失敗だけが続く場合は仮説IDが変化していても止める。
	EarlyStopping stalled;
	stalled.RecordRound(0, 201, 0, true);
	assert(!stalled.Evaluate(tracker).stop);
	stalled.RecordRound(0, 202, 0, true);
	assert(!stalled.Evaluate(tracker).stop);
	stalled.RecordRound(0, 203, 0, true);
	const EarlyStopping::StopDecision decision = stalled.Evaluate(tracker);
	assert(decision.stop);
	assert(decision.reason.find("without new evidence") != std::string::npos);
}

} // namespace

int main() {
	std::cout << "=== AgentOS Evidence Prompt Compression Smoke Test ===\n";
	TestLatestEvidenceIsPrioritized();
	TestAllEvidencePromptsUseCompression();
	TestLargeTaskDefaults();
	TestProgressPreventsEarlyStop();
	std::cout << "=== ALL PASSED ===\n";
	return 0;
}
