// =======================================================================
//
// CriticAgent.cpp
//
// =======================================================================
#include "CriticAgent.h"

#include <algorithm>
#include <cstdint>
#include <string>

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

bool EvidenceLooksFailed(const Json& evidence) {
	if (!evidence.is_object()) return false;
	if (evidence.contains("payload") && evidence.at("payload").is_object()) {
		const Json& payload = evidence.at("payload");
		if (payload.value("failure", false) || payload.value("unsatisfied", false)) return true;
		if (payload.contains("error") && payload.at("error").is_string() &&
		    !payload.at("error").get<std::string>().empty()) return true;
	}
	if (evidence.contains("provenance") && evidence.at("provenance").is_object()) {
		const std::string sourceType = evidence.at("provenance").value("sourceType", std::string());
		return sourceType == "ToolError" || sourceType == "ToolResultError" ||
			sourceType == "ToolUnsatisfied" || sourceType == "DependencyUnsatisfied" ||
			sourceType == "CommandValidationError";
	}
	return false;
}

std::size_t CountFailedEvidenceDefensively(const Json& builtEvidence) {
	if (!builtEvidence.is_object() || !builtEvidence.contains("evidences") ||
	    !builtEvidence.at("evidences").is_array()) return 0;
	std::size_t count = 0;
	for (const Json& evidence : builtEvidence.at("evidences")) {
		if (EvidenceLooksFailed(evidence)) ++count;
	}
	return count;
}

bool IsSceneSnapshotSource(const std::string& sourceType) {
	return sourceType == "Tool:ListEntities" || sourceType == "Tool:ListSystems" ||
		sourceType == "Tool:DescribeEntity";
}

bool IsCompleteSceneSnapshot(const Json& builtEvidence) {
	if (!builtEvidence.is_object() || builtEvidence.value("coverage", 0.0) < 1.0 ||
	    builtEvidence.value("failedEvidenceCount", CountFailedEvidenceDefensively(builtEvidence)) != 0 ||
	    !builtEvidence.contains("evidences") || !builtEvidence.at("evidences").is_array() ||
	    builtEvidence.at("evidences").empty()) {
		return false;
	}

	bool hasEntities = false;
	bool hasSystems = false;
	for (const Json& evidence : builtEvidence.at("evidences")) {
		if (!evidence.is_object() || !evidence.contains("provenance") ||
		    !evidence.at("provenance").is_object()) return false;
		const std::string sourceType = evidence.at("provenance").value("sourceType", std::string());
		if (!IsSceneSnapshotSource(sourceType)) return false;
		hasEntities = hasEntities || sourceType == "Tool:ListEntities";
		hasSystems = hasSystems || sourceType == "Tool:ListSystems";
	}
	return hasEntities && hasSystems;
}

void AddFailureOnce(CriticVerdict* verdict, const std::string& failure) {
	if (std::find(verdict->failures.begin(), verdict->failures.end(), failure) == verdict->failures.end()) {
		verdict->failures.push_back(failure);
	}
}

std::string NormalizeRepairType(const std::string& rawType, const std::string& toolName) {
	if (rawType == "RuntimeObservation" || rawType == "CodeSearch" || rawType == "Trace") {
		return rawType;
	}
	if (!toolName.empty()) return "RuntimeObservation";
	return {};
}

Json NormalizeRepairTasks(const Json& rawTasks) {
	Json normalized = Json::array();
	if (!rawTasks.is_array()) return normalized;

	for (const Json& task : rawTasks) {
		if (!task.is_object() || normalized.size() >= 2) break;
		const std::string tool = task.value("tool", std::string());
		const std::string type = NormalizeRepairType(task.value("type", std::string()), tool);
		if (type.empty()) continue;

		std::string description = task.value("description", std::string());
		if (description.empty()) description = "Criticが要求した追加調査";
		if (!tool.empty()) {
			const Json command = Json::object({
				{"tool", tool},
				{"arguments", task.value("arguments", Json::object())},
			});
			description += "\nREPAIR_COMMAND " + command.dump();
		}

		normalized.push_back(Json::object({
			{"type", type},
			{"description", description},
		}));
	}
	return normalized;
}

} // namespace

Result CriticAgent::Run(
	AgentContext& ctx,
	const Json& rankedHypotheses,
	const Json& builtEvidence,
	CriticVerdict* out) {
	if (out == nullptr) return Result::Fail("CriticAgent: out is null");
	*out = CriticVerdict{};

	if (IsCompleteSceneSnapshot(builtEvidence)) {
		out->llmScores = Json::object({
			{"evidenceCoverage", 1.0},
			{"contradictionHandling", 1.0},
			{"causalCompleteness", 1.0},
			{"testability", 1.0},
			{"route", "deterministic_scene_snapshot"},
		});
		out->programmaticScore = 1.0;
		out->pass = true;
		return Result::Ok();
	}

	const PromptPair prompt = prompts::Critique(rankedHypotheses, builtEvidence);
	Json raw;
	Result callResult = CallLlmJson(ctx, prompt, &raw);
	bool requestPatchApplied = false;
	if (callResult) {
		out->llmScores = raw.value("scores", Json::object());
		if (raw.contains("failures") && raw.at("failures").is_array()) {
			for (const Json& failure : raw.at("failures")) {
				if (failure.is_string()) out->failures.push_back(failure.get<std::string>());
			}
		}

		if (raw.contains("requestPatch") && raw.at("requestPatch").is_object() &&
		    !raw.at("requestPatch").empty()) {
			Json revised;
			Result patchResult = prompts::ApplyCurrentRequestPatch(raw.at("requestPatch"), &revised);
			if (patchResult) {
				requestPatchApplied = true;
				out->llmScores["requestPatchApplied"] = true;
				out->llmScores["activeRevision"] = revised.value("requestRevision", 0);
			} else {
				AddFailureOnce(out, "critic request patch rejected: " + patchResult.error);
			}
		}

		if (raw.contains("additionalTasksSuggested")) {
			out->additionalTasks = NormalizeRepairTasks(raw.at("additionalTasksSuggested"));
		}
	} else {
		out->failures.push_back("critic LLM call failed: " + callResult.error);
	}

	const double coverage = builtEvidence.is_object() ? builtEvidence.value("coverage", 0.0) : 0.0;
	const double topConfidence = TopHypothesisConfidence(rankedHypotheses);
	const std::size_t contradictionCount = ArraySize(builtEvidence, "contradictions");
	const std::size_t evidenceCount = ArraySize(builtEvidence, "evidences");
	const std::size_t tasksWithoutEvidence = ArraySize(builtEvidence, "tasksWithoutEvidence");
	const std::size_t failedEvidenceCount = builtEvidence.is_object()
		? builtEvidence.value("failedEvidenceCount", CountFailedEvidenceDefensively(builtEvidence))
		: CountFailedEvidenceDefensively(builtEvidence);
	const std::size_t usableEvidenceCount = builtEvidence.is_object()
		? builtEvidence.value(
			"usableEvidenceCount",
			evidenceCount >= failedEvidenceCount ? evidenceCount - failedEvidenceCount : std::size_t(0))
		: 0;

	const double contradictionTerm = 1.0 -
		(std::min)(1.0, static_cast<double>(contradictionCount) / 3.0);
	const double evidenceTerm = usableEvidenceCount >= 3 ? 1.0 : 0.0;
	out->programmaticScore =
		0.4 * coverage + 0.3 * topConfidence + 0.2 * contradictionTerm + 0.1 * evidenceTerm;

	bool hardFail = false;
	if (coverage < 1.0) {
		AddFailureOnce(out, "programmatic hard fail: required task coverage is incomplete");
		hardFail = true;
	}
	if (tasksWithoutEvidence > 0) {
		AddFailureOnce(out, "programmatic hard fail: one or more planned tasks produced no usable evidence");
		hardFail = true;
	}
	if (usableEvidenceCount == 0) {
		AddFailureOnce(out, "programmatic hard fail: no usable evidence was collected");
		hardFail = true;
	}
	if (failedEvidenceCount > 0) {
		AddFailureOnce(out, "programmatic hard fail: failed or unsatisfied Tool evidence exists");
		hardFail = true;
	}
	if (requestPatchApplied) {
		AddFailureOnce(out, "programmatic hard fail: request revision changed and must be re-observed");
		hardFail = true;
	}
	if (out->additionalTasks.is_array() && !out->additionalTasks.empty()) {
		AddFailureOnce(out, "programmatic hard fail: critic requested additional investigation");
		hardFail = true;
	}
	if (topConfidence < 0.4) {
		AddFailureOnce(out, "programmatic hard fail: top hypothesis confidence is below 0.4");
		hardFail = true;
	}

	out->pass = !hardFail && out->programmaticScore >= 0.55;
	return Result::Ok();
}

} // namespace agentos
