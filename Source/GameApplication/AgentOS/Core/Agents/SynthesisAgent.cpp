// =======================================================================
//
// SynthesisAgent.cpp
//
// =======================================================================
#include "SynthesisAgent.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_set>

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

bool EvidenceLooksFailed(const Json& evidence) {
	if (!evidence.is_object()) return true;
	const Json payload = evidence.value("payload", Json::object());
	return payload.is_object() &&
		(payload.value("failure", false) || payload.value("unsatisfied", false));
}

std::string SuccessfulEvidenceText(const Json& builtEvidence) {
	std::string text;
	if (!builtEvidence.is_object() || !builtEvidence.contains("evidences") ||
	    !builtEvidence.at("evidences").is_array()) return text;
	for (const Json& evidence : builtEvidence.at("evidences")) {
		if (EvidenceLooksFailed(evidence)) continue;
		text += evidence.value("claim", std::string());
		text += "\n";
		if (evidence.contains("payload")) text += evidence.at("payload").dump();
		text += "\n";
	}
	return text;
}

std::unordered_set<long long> AuthoritativeEntityCounts(const Json& builtEvidence) {
	std::unordered_set<long long> counts;
	if (!builtEvidence.is_object() || !builtEvidence.contains("evidences") ||
	    !builtEvidence.at("evidences").is_array()) return counts;
	for (const Json& evidence : builtEvidence.at("evidences")) {
		if (EvidenceLooksFailed(evidence) || !evidence.contains("provenance") ||
		    !evidence.at("provenance").is_object() ||
		    evidence.at("provenance").value("sourceType", std::string()) != "Tool:ListEntities") {
			continue;
		}
		const Json payload = evidence.value("payload", Json::object());
		for (const char* key : {"count", "entityCount", "total"}) {
			if (payload.contains(key) && payload.at(key).is_number_integer()) {
				counts.insert(payload.at(key).get<long long>());
			}
		}
		if (payload.contains("entities") && payload.at("entities").is_array()) {
			counts.insert(static_cast<long long>(payload.at("entities").size()));
		}
	}
	return counts;
}

bool EntityCountClaimsAreSupported(const std::string& report, const Json& builtEvidence) {
	const std::unordered_set<long long> counts = AuthoritativeEntityCounts(builtEvidence);
	if (counts.empty()) return true;
	static const std::regex countClaim(R"(([0-9]+)\s*(?:件|個|体|つ))");
	for (auto it = std::sregex_iterator(report.begin(), report.end(), countClaim);
	     it != std::sregex_iterator(); ++it) {
		const std::size_t matchPos = static_cast<std::size_t>(it->position());
		const std::size_t begin = matchPos > 64 ? matchPos - 64 : 0;
		const std::size_t end = (std::min)(
			report.size(), matchPos + static_cast<std::size_t>(it->length()) + 64);
		const std::string context = report.substr(begin, end - begin);
		if (context.find("Entity") == std::string::npos &&
		    context.find("Entities") == std::string::npos &&
		    context.find("エンティティ") == std::string::npos) {
			continue;
		}
		const long long value = std::stoll((*it)[1].str());
		if (counts.count(value) == 0) return false;
	}
	return true;
}

bool HasCapabilityFailureEvidence(const Json& builtEvidence) {
	if (!builtEvidence.is_object() || !builtEvidence.contains("evidences") ||
	    !builtEvidence.at("evidences").is_array()) return false;
	for (const Json& evidence : builtEvidence.at("evidences")) {
		if (!evidence.is_object() || !evidence.contains("provenance") ||
		    !evidence.at("provenance").is_object()) continue;
		const std::string sourceType =
			evidence.at("provenance").value("sourceType", std::string());
		if (sourceType == "CapabilityRejected" || sourceType == "AwaitingApproval") return true;
	}
	return false;
}

bool CapabilityClaimsAreSupported(const std::string& report, const Json& builtEvidence) {
	const bool claimsCapabilityFailure = ContainsAny(report, {
		"権限不足", "権限がない", "許可されていない", "利用禁止",
		"permission denied", "access denied"
	});
	return !claimsCapabilityFailure || HasCapabilityFailureEvidence(builtEvidence);
}

bool IsGenericHistoryIdentifier(const std::string& value) {
	static const std::unordered_set<std::string> generic = {
		"entity", "component", "system", "scene", "runtime", "current", "report",
		"agentos", "tool", "listentities", "listsystems", "describeentity",
	};
	std::string lower = value;
	std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return generic.count(lower) != 0;
}

bool ContainsAsciiToken(const std::string& text, const std::string& token) {
	std::size_t pos = 0;
	while ((pos = text.find(token, pos)) != std::string::npos) {
		auto word = [](unsigned char ch) { return std::isalnum(ch) != 0 || ch == '_'; };
		const bool left = pos == 0 || !word(static_cast<unsigned char>(text[pos - 1]));
		const std::size_t end = pos + token.size();
		const bool right = end >= text.size() || !word(static_cast<unsigned char>(text[end]));
		if (left && right) return true;
		pos = end;
	}
	return false;
}

bool HistoryClaimsAreSupported(const std::string& report, const Json& builtEvidence) {
	const std::string relation = prompts::CurrentTurnRelation();
	if (relation != "new" && relation != "refresh") return true;
	const std::string evidenceText = SuccessfulEvidenceText(builtEvidence);
	const Json identifiers = prompts::CurrentHistoryIdentifiers();
	if (!identifiers.is_array()) return true;
	for (const Json& value : identifiers) {
		if (!value.is_string()) continue;
		const std::string identifier = value.get<std::string>();
		if (identifier.size() < 4 || identifier.find_first_of(":/.-") != std::string::npos ||
		    IsGenericHistoryIdentifier(identifier)) continue;
		if (evidenceText.find(identifier) == std::string::npos &&
		    ContainsAsciiToken(report, identifier)) return false;
	}
	return true;
}

bool ReportIsEvidenceConsistent(const std::string& report, const Json& builtEvidence) {
	return EntityCountClaimsAreSupported(report, builtEvidence) &&
		CapabilityClaimsAreSupported(report, builtEvidence) &&
		HistoryClaimsAreSupported(report, builtEvidence);
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
			if (!payload.empty()) {
				oss << "  - payload: " << prompts::Truncate(payload.dump(), 1600) << "\n";
			}
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
		const bool completionSafe = passed || !ContainsFalseCompletionClaim(generated);
		if (completionSafe && ReportIsEvidenceConsistent(generated, builtEvidence)) {
			*reportOut = passed
				? generated
				: "自動調査は未完了です。以下は部分結果です。\n\n" + generated;
			PersistFinalResponse(ctx, *reportOut);
			return Result::Ok();
		}
		if (completionSafe) {
			const Json synthesisStop = Json::object({
				{"reason", "generated report contradicted current evidence"},
			});
			*reportOut = "# 最終回答のEvidence整合性検証に失敗しました\n\n" +
				BuildFallbackReport(builtEvidence, rankedHypotheses, synthesisStop, true);
			PersistFinalResponse(ctx, *reportOut);
			return Result::Fail("SynthesisAgent: generated report contradicted current evidence");
		}
	}

	*reportOut = BuildFallbackReport(builtEvidence, rankedHypotheses, stopInfo, !passed);
	PersistFinalResponse(ctx, *reportOut);
	return Result::Ok();
}

} // namespace agentos
