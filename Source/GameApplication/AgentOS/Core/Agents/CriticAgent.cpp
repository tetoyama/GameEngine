// =======================================================================
//
// CriticAgent.cpp
//
// =======================================================================
#include "CriticAgent.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <regex>
#include <string>
#include <unordered_set>
#include <vector>

namespace agentos {

namespace critic_internal {

namespace {

// ---------------------------------
// goal identifier抽出用の共有ヘルパ
// ---------------------------------

const std::unordered_set<std::string>& AsciiStoplist() {
	// Entity/Component/Scene/System/Toolはツール名・スキーマ語彙として
	// あらゆる要求文に自然に登場する一般語のため、目的識別子としては扱わない。
	static const std::unordered_set<std::string> kStoplist = {
		"entity", "component", "scene", "system", "tool", "the", "and",
	};
	return kStoplist;
}

const std::unordered_set<std::string>& KatakanaStoplist() {
	// 上記ASCII stoplistのカタカナ表記版。「シーン」等はSceneSnapshot系の
	// 要求文に常に登場するため、これを目的識別子として扱うと
	// AgentOSSceneSnapshotFastPathSmokeTest等の正当なSnapshot要求まで
	// 「識別子未カバー」と誤判定してしまう。
	static const std::unordered_set<std::string> kStoplist = {
		"シーン", "コンポーネント", "エンティティ", "システム", "ツール",
	};
	return kStoplist;
}

std::string ToLowerAscii(const std::string& s) {
	std::string out = s;
	std::transform(out.begin(), out.end(), out.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return out;
}

bool IsAsciiOnly(const std::string& s) {
	for (unsigned char c : s) {
		if (c > 127) return false;
	}
	return true;
}

// UTF-8の3バイト目までを見て、Katakana Unicode block（U+30A0-U+30FF）の
// 3バイトシーケンスかどうかを判定する（E3 82 A0-BF / E3 83 80-BF）。
bool IsKatakanaTriple(unsigned char b0, unsigned char b1, unsigned char b2) {
	if (b0 != 0xE3) return false;
	if (b1 == 0x82 && b2 >= 0xA0 && b2 <= 0xBF) return true;
	if (b1 == 0x83 && b2 >= 0x80 && b2 <= 0xBF) return true;
	return false;
}

} // namespace

std::vector<std::string> ExtractGoalIdentifiers(const std::string& text) {
	std::vector<std::string> result;
	std::unordered_set<std::string> seenKeys;

	auto tryAdd = [&](const std::string& identifier) {
		if (identifier.empty()) return;
		const std::string key = IsAsciiOnly(identifier) ? ToLowerAscii(identifier) : identifier;
		if (seenKeys.count(key) != 0) return;
		seenKeys.insert(key);
		result.push_back(identifier);
	};

	// 1) 引用トークン（例: Entity 'Player' → "Player"）。
	static const std::regex quotedRe(R"(['"]([^'"]{1,64})['"])");
	for (auto it = std::sregex_iterator(text.begin(), text.end(), quotedRe);
	     it != std::sregex_iterator(); ++it) {
		const std::string token = (*it)[1].str();
		if (!token.empty()) tryAdd(token);
	}

	// 2) ASCII識別子（3文字以上、stoplist除外、大小無視で重複排除）。
	static const std::regex asciiRe(R"([A-Za-z_][A-Za-z0-9_]{2,})");
	for (auto it = std::sregex_iterator(text.begin(), text.end(), asciiRe);
	     it != std::sregex_iterator(); ++it) {
		const std::string token = it->str();
		if (AsciiStoplist().count(ToLowerAscii(token)) != 0) continue;
		tryAdd(token);
	}

	// 3) カタカナ連続（3文字以上、UTF-8バイト範囲スキャン、stoplist除外）。
	std::size_t i = 0;
	while (i + 2 < text.size()) {
		const unsigned char b0 = static_cast<unsigned char>(text[i]);
		const unsigned char b1 = static_cast<unsigned char>(text[i + 1]);
		const unsigned char b2 = static_cast<unsigned char>(text[i + 2]);
		if (!IsKatakanaTriple(b0, b1, b2)) {
			// UTF-8可変長エンコーディングに沿って1文字分だけ進める。
			if ((b0 & 0x80) == 0) i += 1;
			else if ((b0 & 0xE0) == 0xC0) i += 2;
			else if ((b0 & 0xF0) == 0xE0) i += 3;
			else if ((b0 & 0xF8) == 0xF0) i += 4;
			else i += 1;
			continue;
		}
		const std::size_t start = i;
		std::size_t count = 0;
		while (i + 2 < text.size() &&
		       IsKatakanaTriple(
		           static_cast<unsigned char>(text[i]),
		           static_cast<unsigned char>(text[i + 1]),
		           static_cast<unsigned char>(text[i + 2]))) {
			i += 3;
			++count;
		}
		if (count >= 3) {
			const std::string token = text.substr(start, i - start);
			if (KatakanaStoplist().count(token) == 0) tryAdd(token);
		}
	}

	return result;
}

} // namespace critic_internal

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

// ---------------------------------
// ゲート#8: goal identifiers uncovered
// ---------------------------------

std::string SerializeEvidenceForGoalCheck(const Json& builtEvidence) {
	std::string out;
	if (!builtEvidence.is_object() || !builtEvidence.contains("evidences") ||
	    !builtEvidence.at("evidences").is_array()) {
		return out;
	}
	for (const Json& evidence : builtEvidence.at("evidences")) {
		if (!evidence.is_object()) continue;
		if (evidence.contains("claim") && evidence.at("claim").is_string()) {
			out += evidence.at("claim").get<std::string>();
			out += "\n";
		}
		if (evidence.contains("payload")) {
			out += evidence.at("payload").dump();
			out += "\n";
		}
	}
	return out;
}

bool IsAsciiOnlyIdentifier(const std::string& s) {
	for (unsigned char c : s) {
		if (c > 127) return false;
	}
	return true;
}

std::string ToLowerAsciiIdentifier(const std::string& s) {
	std::string out = s;
	std::transform(out.begin(), out.end(), out.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return out;
}

bool EvidenceCoversIdentifier(
	const std::string& evidenceTextLower,
	const std::string& evidenceTextRaw,
	const std::string& identifier) {
	if (IsAsciiOnlyIdentifier(identifier)) {
		return evidenceTextLower.find(ToLowerAsciiIdentifier(identifier)) != std::string::npos;
	}
	return evidenceTextRaw.find(identifier) != std::string::npos;
}

std::vector<std::string> FindMissingGoalIdentifiers(
	const std::vector<std::string>& identifiers,
	const Json& builtEvidence) {
	std::vector<std::string> missing;
	if (identifiers.empty()) return missing;

	const std::string evidenceTextRaw = SerializeEvidenceForGoalCheck(builtEvidence);
	std::string evidenceTextLower = evidenceTextRaw;
	std::transform(evidenceTextLower.begin(), evidenceTextLower.end(), evidenceTextLower.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	for (const std::string& identifier : identifiers) {
		if (!EvidenceCoversIdentifier(evidenceTextLower, evidenceTextRaw, identifier)) {
			missing.push_back(identifier);
		}
	}
	return missing;
}

std::string JoinIdentifiers(const std::vector<std::string>& identifiers) {
	std::string joined;
	for (std::size_t i = 0; i < identifiers.size(); ++i) {
		if (i != 0) joined += ", ";
		joined += identifiers[i];
	}
	return joined;
}

// resolvedRequestから抽出した対象識別子が、収集済みEvidence（claim+payload）に
// 一切現れない場合はhard failとして扱う（戻り値false）。additionalTasksが
// まだ空であれば、不足識別子を調べ直すための修復Taskを1件合成し、Repairループ
// が実際に到達できるようにする。IsCompleteSceneSnapshotの決定的バイパスからも
// 必ず呼び出すこと（Snapshotとして内部完結していても目的未達を見逃さないため）。
bool ApplyGoalIdentifierGate(const Json& builtEvidence, CriticVerdict* out) {
	const std::string resolvedRequest = prompts::CurrentResolvedRequest();
	const std::vector<std::string> identifiers = critic_internal::ExtractGoalIdentifiers(resolvedRequest);
	if (identifiers.empty()) return true;

	const std::vector<std::string> missing = FindMissingGoalIdentifiers(identifiers, builtEvidence);
	if (missing.empty()) return true;

	const std::string joined = JoinIdentifiers(missing);
	AddFailureOnce(out, "goal identifiers not covered by evidence: " + joined);

	if (!out->additionalTasks.is_array() || out->additionalTasks.empty()) {
		out->additionalTasks = Json::array({
			Json::object({
				{"type", "RuntimeObservation"},
				{"description", "不足識別子 " + joined +
					" に関するEvidenceを取得する（ResolveEntity/DescribeEntity/ReadComponent等）"},
			}),
		});
	}
	return false;
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
		// ゲート#8はこの決定的バイパスでも必ず適用する。Snapshotとして
		// 内部完結していても、resolvedRequestが具体的な対象識別子
		// （例: Entity 'Player' のJumpForce）を含み、それがEvidence中に
		// 一切現れないなら目的未達であり、passにしてはならない
		// （実機失敗事例: 「調査未完了」を自認する報告がcritic passedになっていた）。
		out->pass = ApplyGoalIdentifierGate(builtEvidence, out);
		return Result::Ok();
	}

	const PromptPair prompt = prompts::Critique(rankedHypotheses, builtEvidence);
	Json raw;
	Result callResult = CallLlmJson(ctx, prompt, &raw);
	bool requestPatchApplied = false;
	// goalSatisfied==falseはLLMが目的未達を明言した場合のみ設定される（advisoryではなく
	// hard failとして扱う: 「達成した」という誤った自己申告を信用するより、「未達だ」
	// という申告を信用する方が安全側に倒れるため）。逆にgoalSatisfied==trueは
	// passの十分条件にはしない。判定は必ずプログラム側の決定的ゲート
	// （coverage/failedEvidenceCount/ゲート#8等）で行う。
	bool goalSatisfiedHardFail = false;
	if (callResult) {
		out->llmScores = raw.value("scores", Json::object());
		if (raw.contains("failures") && raw.at("failures").is_array()) {
			for (const Json& failure : raw.at("failures")) {
				if (failure.is_string()) out->failures.push_back(failure.get<std::string>());
			}
		}

		if (raw.contains("goalSatisfied") && raw.at("goalSatisfied").is_boolean()) {
			out->llmScores["goalSatisfied"] = raw.at("goalSatisfied");
			if (raw.at("goalSatisfied").get<bool>() == false) {
				goalSatisfiedHardFail = true;
			}
		}
		if (raw.contains("unmetAspects") && raw.at("unmetAspects").is_array()) {
			out->llmScores["unmetAspects"] = raw.at("unmetAspects");
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
		AddFailureOnce(
			out,
			"programmatic hard fail: failed tool or command-validation evidence exists "
			"(including unsatisfied Tool results)");
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
	if (goalSatisfiedHardFail) {
		AddFailureOnce(out, "programmatic hard fail: critic LLM reported goalSatisfied=false");
		hardFail = true;
	}
	// ゲート#8: resolvedRequestの目的識別子がEvidenceに一切現れないなら、
	// 他の全ゲートを通過していても目的未達としてhard failにする。
	if (!ApplyGoalIdentifierGate(builtEvidence, out)) {
		hardFail = true;
	}

	out->pass = !hardFail && out->programmaticScore >= 0.55;
	return Result::Ok();
}

} // namespace agentos
