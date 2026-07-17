// =======================================================================
//
// ReasoningAgent.cpp
//
// =======================================================================
#include "ReasoningAgent.h"

#include <algorithm>
#include <string>
#include <unordered_set>

namespace agentos {

namespace {

double Clamp01(double v) {
	if (v < 0.0) return 0.0;
	if (v > 1.0) return 1.0;
	return v;
}

} // namespace

Result ReasoningAgent::Run(AgentContext& ctx, const Json& builtEvidenceJson, LogicGraph* graphOut, Json* rawOut) {
	if (graphOut == nullptr) {
		return Result::Fail("ReasoningAgent: graphOut is null");
	}

	const PromptPair prompt = prompts::Reason(builtEvidenceJson);
	Json raw;
	Result callResult = CallLlmJson(ctx, prompt, &raw);
	if (!callResult) {
		return callResult;
	}
	if (rawOut != nullptr) {
		*rawOut = raw;
	}

	// LLM出力が参照してよいEvidence IDは、実際に統合Evidenceへ含まれるものだけ。
	// それ以外はLLMの捏造とみなし、黙って捨てる（構想§3の信頼境界）。
	std::unordered_set<std::int64_t> validEvidenceIds;
	if (builtEvidenceJson.is_object() && builtEvidenceJson.contains("evidences") &&
	    builtEvidenceJson.at("evidences").is_array()) {
		for (const auto& e : builtEvidenceJson.at("evidences")) {
			if (e.is_object() && e.contains("id") && e.at("id").is_number_integer()) {
				validEvidenceIds.insert(e.at("id").get<std::int64_t>());
			}
		}
	}

	if (!raw.is_object() || !raw.contains("hypotheses") || !raw.at("hypotheses").is_array()) {
		return Result::Ok(); // 仮説0件も正当な出力として扱う
	}

	for (const auto& h : raw.at("hypotheses")) {
		if (!h.is_object()) {
			continue;
		}
		const std::string description = h.value("description", std::string());
		if (description.empty()) {
			continue; // 説明の無い仮説は無視する
		}
		const double rubricBase = Clamp01(h.value("rubricBase", 0.0));

		const LogicNodeId nodeId = graphOut->AddHypothesis(description, rubricBase);

		if (h.contains("supports") && h.at("supports").is_array()) {
			for (const auto& s : h.at("supports")) {
				if (!s.is_number_integer()) {
					continue;
				}
				const std::int64_t evidenceId = s.get<std::int64_t>();
				if (validEvidenceIds.count(evidenceId) != 0) {
					graphOut->AddSupport(nodeId, evidenceId);
				}
			}
		}
		if (h.contains("contradicts") && h.at("contradicts").is_array()) {
			for (const auto& c : h.at("contradicts")) {
				if (!c.is_number_integer()) {
					continue;
				}
				const std::int64_t evidenceId = c.get<std::int64_t>();
				if (validEvidenceIds.count(evidenceId) != 0) {
					graphOut->AddContradiction(nodeId, evidenceId);
				}
			}
		}
		if (h.contains("missingEvidence") && h.at("missingEvidence").is_array()) {
			for (const auto& m : h.at("missingEvidence")) {
				if (m.is_string()) {
					graphOut->AddMissingEvidence(nodeId, m.get<std::string>());
				}
			}
		}
	}

	return Result::Ok();
}

} // namespace agentos
