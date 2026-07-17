// =======================================================================
//
// SynthesisAgent.cpp
//
// =======================================================================
#include "SynthesisAgent.h"

#include <sstream>

namespace agentos {

namespace {

std::string BuildFallbackReport(const Json& builtEvidence, const Json& rankedHypotheses, const Json& stopInfo) {
	std::ostringstream oss;
	oss << "# 調査レポート（フォールバック生成）\n\n";
	oss << "LLMによる要約が得られなかったため、確定Evidenceと仮説から機械的に組み立てました。\n\n";

	oss << "## 最有力仮説\n";
	if (rankedHypotheses.is_object() && rankedHypotheses.contains("hypotheses") &&
	    rankedHypotheses.at("hypotheses").is_array() && !rankedHypotheses.at("hypotheses").empty()) {
		const Json& top = rankedHypotheses.at("hypotheses")[0];
		oss << "- " << top.value("text", std::string("(不明)")) << "\n";
		oss << "- confidence: " << top.value("confidence", 0.0) << "\n\n";
	} else {
		oss << "有効な仮説はありませんでした。\n\n";
	}

	oss << "## 確認済みEvidence\n";
	if (builtEvidence.is_object() && builtEvidence.contains("evidences") &&
	    builtEvidence.at("evidences").is_array()) {
		for (const auto& e : builtEvidence.at("evidences")) {
			oss << "- " << e.value("claim", std::string()) << "\n";
		}
	}

	oss << "\n## 停止理由\n";
	if (stopInfo.is_object() && stopInfo.contains("reason")) {
		oss << stopInfo.value("reason", std::string("不明"));
	} else {
		oss << "不明";
	}
	oss << "\n";
	return oss.str();
}

void PersistFinalResponse(AgentContext& ctx, const std::string& report) {
	if (ctx.store != nullptr && ctx.sessionId != kInvalidId && !report.empty()) {
		(void)ctx.store->SetConversationResponse(ctx.sessionId, report);
	}
}

} // namespace

Result SynthesisAgent::Run(AgentContext& ctx, const Json& builtEvidence, const Json& rankedHypotheses,
                            const Json& stopInfo, std::string* reportOut) {
	if (reportOut == nullptr) {
		return Result::Fail("SynthesisAgent: reportOut is null");
	}

	const PromptPair prompt = prompts::Synthesize(builtEvidence, rankedHypotheses, stopInfo);
	Json raw;
	Result callResult = CallLlmJson(ctx, prompt, &raw);
	if (callResult && raw.is_object() && raw.contains("report") && raw.at("report").is_string() &&
	    !raw.at("report").get<std::string>().empty()) {
		*reportOut = raw.at("report").get<std::string>();
		PersistFinalResponse(ctx, *reportOut);
		return Result::Ok();
	}

	*reportOut = BuildFallbackReport(builtEvidence, rankedHypotheses, stopInfo);
	PersistFinalResponse(ctx, *reportOut);
	return Result::Ok();
}

} // namespace agentos
