// =======================================================================
//
// SynthesisAgent.cpp
//
// =======================================================================
#include "SynthesisAgent.h"

#include <initializer_list>
#include <sstream>
#include <string>

namespace agentos {

namespace {

bool CriticPassed(const Json& stopInfo) {
	return stopInfo.is_object() && stopInfo.value("reason", std::string()) == "critic passed";
}

bool ContainsAny(const std::string& text, std::initializer_list<const char*> needles) {
	for (const char* needle : needles) {
		if (needle != nullptr && text.find(needle) != std::string::npos) return true;
	}
	return false;
}

bool ContainsFalseCompletionClaim(const std::string& report) {
	return ContainsAny(report, {
		"調査は完了", "調査完了", "確認されました", "確認できました",
		"存在しないことが確認", "存在しないと確定", "問題は解決", "解決しました"
	});
}

std::string BuildFallbackReport(
	const Json& builtEvidence,
	const Json& rankedHypotheses,
	const Json& stopInfo,
	bool incomplete) {
	std::ostringstream oss;
	oss << (incomplete ? "# 自動調査は未完了です\n\n" : "# 調査レポート\n\n");
	if (incomplete) {
		oss << "Criticの通過条件を満たさなかったため、以下は確定回答ではなく部分結果です。\n\n";
	}

	oss << "## 取得できたEvidence\n";
	bool hasEvidence = false;
	if (builtEvidence.is_object() && builtEvidence.contains("evidences") &&
	    builtEvidence.at("evidences").is_array()) {
		for (const Json& evidence : builtEvidence.at("evidences")) {
			if (!evidence.is_object()) continue;
			const Json payload = evidence.value("payload", Json::object());
			const bool failed = payload.is_object() &&
				(payload.value("failure", false) || payload.value("unsatisfied", false));
			oss << "- " << (failed ? "[未達成] " : "")
				<< evidence.value("claim", std::string("(claimなし)")) << "\n";
			hasEvidence = true;
		}
	}
	if (!hasEvidence) oss << "- 利用可能なEvidenceは得られませんでした。\n";

	oss << "\n## 現在の仮説\n";
	if (rankedHypotheses.is_object() && rankedHypotheses.contains("hypotheses") &&
	    rankedHypotheses.at("hypotheses").is_array() && !rankedHypotheses.at("hypotheses").empty()) {
		const Json& top = rankedHypotheses.at("hypotheses")[0];
		oss << "- " << top.value("text", std::string("(不明)")) << "\n";
		oss << "- confidence: " << top.value("confidence", 0.0) << "\n";
	} else {
		oss << "- 有効な仮説はありません。\n";
	}

	oss << "\n## 停止理由\n";
	oss << (stopInfo.is_object()
		? stopInfo.value("reason", std::string("不明"))
		: std::string("不明"));
	oss << "\n";
	return oss.str();
}

void PersistFinalResponse(AgentContext& ctx, const std::string& report) {
	if (ctx.store != nullptr && ctx.sessionId != kInvalidId && !report.empty()) {
		(void)ctx.store->SetConversationResponse(ctx.sessionId, report);
	}
}

} // namespace

Result SynthesisAgent::Run(
	AgentContext& ctx,
	const Json& builtEvidence,
	const Json& rankedHypotheses,
	const Json& stopInfo,
	std::string* reportOut) {
	if (reportOut == nullptr) return Result::Fail("SynthesisAgent: reportOut is null");

	const bool passed = CriticPassed(stopInfo);
	const PromptPair prompt = prompts::Synthesize(builtEvidence, rankedHypotheses, stopInfo);
	Json raw;
	Result callResult = CallLlmJson(ctx, prompt, &raw);
	if (callResult && raw.is_object() && raw.contains("report") && raw.at("report").is_string() &&
	    !raw.at("report").get<std::string>().empty()) {
		const std::string generated = raw.at("report").get<std::string>();
		if (passed || !ContainsFalseCompletionClaim(generated)) {
			*reportOut = passed
				? generated
				: "自動調査は未完了です。以下は部分結果です。\n\n" + generated;
			PersistFinalResponse(ctx, *reportOut);
			return Result::Ok();
		}
	}

	*reportOut = BuildFallbackReport(builtEvidence, rankedHypotheses, stopInfo, !passed);
	PersistFinalResponse(ctx, *reportOut);
	return Result::Ok();
}

} // namespace agentos
