// =======================================================================
//
// CriticAgent.cpp
//
// =======================================================================
#include "CriticAgent.h"

#include <algorithm>
#include <cstdint>

namespace agentos {

namespace {

double TopHypothesisConfidence(const Json& rankedHypotheses) {
	if (!rankedHypotheses.is_object() || !rankedHypotheses.contains("hypotheses") ||
	    !rankedHypotheses.at("hypotheses").is_array() || rankedHypotheses.at("hypotheses").empty()) {
		return 0.0;
	}
	const Json& top = rankedHypotheses.at("hypotheses")[0];
	if (top.is_object() && top.contains("confidence") && top.at("confidence").is_number()) {
		return top.at("confidence").get<double>();
	}
	return 0.0;
}

std::size_t ArraySize(const Json& parent, const char* key) {
	if (parent.is_object() && parent.contains(key) && parent.at(key).is_array()) {
		return parent.at(key).size();
	}
	return 0;
}

} // namespace

Result CriticAgent::Run(AgentContext& ctx, const Json& rankedHypotheses, const Json& builtEvidence, CriticVerdict* out) {
	if (out == nullptr) {
		return Result::Fail("CriticAgent: out is null");
	}
	*out = CriticVerdict{};

	const PromptPair prompt = prompts::Critique(rankedHypotheses, builtEvidence);
	Json raw;
	Result callResult = CallLlmJson(ctx, prompt, &raw);
	if (callResult) {
		out->llmScores = raw.value("scores", Json::object());
		if (raw.contains("failures") && raw.at("failures").is_array()) {
			for (const auto& f : raw.at("failures")) {
				if (f.is_string()) {
					out->failures.push_back(f.get<std::string>());
				}
			}
		}
		if (raw.contains("additionalTasksSuggested") && raw.at("additionalTasksSuggested").is_array()) {
			out->additionalTasks = raw.at("additionalTasksSuggested");
		}
	} else {
		// LLMの所見はadvisoryに過ぎない。失敗しても採点自体はプログラムで続行する。
		out->failures.push_back("critic LLM call failed: " + callResult.error);
	}

	// --- プログラム採点（LLMを一切使わない） ---
	const double coverage = builtEvidence.is_object() ? builtEvidence.value("coverage", 0.0) : 0.0;
	const double topConfidence = TopHypothesisConfidence(rankedHypotheses);
	const std::size_t contradictionCount = ArraySize(builtEvidence, "contradictions");
	const std::size_t evidenceCount = ArraySize(builtEvidence, "evidences");

	const double contradictionTerm = 1.0 - std::min(1.0, static_cast<double>(contradictionCount) / 3.0);
	const double evidenceTerm = (evidenceCount >= 3) ? 1.0 : 0.0;

	out->programmaticScore =
		0.4 * coverage + 0.3 * topConfidence + 0.2 * contradictionTerm + 0.1 * evidenceTerm;
	out->pass = (out->programmaticScore >= 0.55) && (topConfidence >= 0.4);

	return Result::Ok();
}

} // namespace agentos
