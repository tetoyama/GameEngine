// =======================================================================
//
// ReasoningAgent.cpp
//
// =======================================================================
#include "ReasoningAgent.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_set>

namespace agentos {

namespace {

double Clamp01(double v) {
	if (v < 0.0) return 0.0;
	if (v > 1.0) return 1.0;
	return v;
}

bool IsCompleteSceneSnapshot(const Json& builtEvidenceJson, Json* evidenceIdsOut) {
	if (!builtEvidenceJson.is_object() || builtEvidenceJson.value("coverage", 0.0) < 1.0 ||
	    builtEvidenceJson.value("failedEvidenceCount", std::size_t(0)) != 0 ||
	    !builtEvidenceJson.contains("evidences") || !builtEvidenceJson.at("evidences").is_array()) {
		return false;
	}

	bool hasEntities = false;
	bool hasSystems = false;
	Json evidenceIds = Json::array();
	for (const Json& evidence : builtEvidenceJson.at("evidences")) {
		if (!evidence.is_object()) {
			continue;
		}
		if (evidence.contains("id") && evidence.at("id").is_number_integer()) {
			evidenceIds.push_back(evidence.at("id"));
		}
		if (evidence.contains("provenance") && evidence.at("provenance").is_object()) {
			const std::string sourceType = evidence.at("provenance").value("sourceType", std::string());
			hasEntities = hasEntities || sourceType == "Tool:ListEntities";
			hasSystems = hasSystems || sourceType == "Tool:ListSystems";
		}
	}

	if (!hasEntities || !hasSystems || evidenceIds.empty()) {
		return false;
	}
	if (evidenceIdsOut != nullptr) {
		*evidenceIdsOut = std::move(evidenceIds);
	}
	return true;
}

} // namespace

Result ReasoningAgent::Run(AgentContext& ctx, const Json& builtEvidenceJson, LogicGraph* graphOut, Json* rawOut) {
	if (graphOut == nullptr) {
		return Result::Fail("ReasoningAgent: graphOut is null");
	}

	Json raw;
	Json sceneEvidenceIds;
	if (IsCompleteSceneSnapshot(builtEvidenceJson, &sceneEvidenceIds)) {
		// Sceneの現在状態を列挙する要求は原因仮説を必要としない。取得済みEvidenceを
		// そのまま「完全なScene snapshot」としてLogicGraphへ載せ、Reasoning LLMを省略する。
		raw = Json::object({
			{"hypotheses", Json::array({Json::object({
				{"description", "現在のScene snapshotをEntity・Component・System観測から構成した"},
				{"rubricBase", 1.0},
				{"supports", sceneEvidenceIds},
				{"contradicts", Json::array()},
				{"missingEvidence", Json::array()},
			})})},
			{"route", "deterministic_scene_snapshot"},
		});
	} else {
		const PromptPair prompt = prompts::Reason(builtEvidenceJson);
		Result callResult = CallLlmJson(ctx, prompt, &raw);
		if (!callResult) {
			return callResult;
		}
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
		return Result::Ok();
	}

	for (const auto& h : raw.at("hypotheses")) {
		if (!h.is_object()) {
			continue;
		}
		const std::string description = h.value("description", std::string());
		if (description.empty()) {
			continue;
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
