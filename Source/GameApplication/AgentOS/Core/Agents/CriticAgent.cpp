// =======================================================================
//
// CriticAgent.cpp
//
// =======================================================================
#include "CriticAgent.h"

#include "../Conversation/RespondTool.h"

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

std::vector<std::string> ExtractGoalIdentifiers(const std::string& text, bool includeKatakana) {
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
	if (!includeKatakana) return result;
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

// ---------------------------------
// 参照系Evidence
// ---------------------------------
// GetConversationHistoryは4種別を今回のEvidenceとして載せる。
//   evidence      : 過去のTool実行結果 ＝ 観測。断定の根拠になれる
//   userTurn      : ユーザの過去発話 ＝ 要求の記録であって観測ではない
//   threadState   : Intakeが確定させた構造化要約 ＝ 解釈
//   assistantTurn : 過去のAgent応答 ＝ 推測
//
// 後ろ3つは参照解決（「さっきの5件」が何を指すか）には要るが、
// これだけを根拠に断定すると「昔そう言ったから正しい」が成立してしまう。
// プロンプトで守らせず、ここで決定的に止める。
bool IsReferenceOnlyEvidence(const Json& evidence) {
	if (!evidence.is_object()) return false;
	if (!evidence.contains("payload") || !evidence.at("payload").is_object()) return false;
	const Json& payload = evidence.at("payload");
	if (!payload.contains("entries") || !payload.at("entries").is_array()) return false;

	// entriesを持つ履歴Evidenceのうち、観測が1件も含まれないものが参照系。
	if (payload.contains("observationCount") && payload.at("observationCount").is_number_unsigned()) {
		return payload.at("observationCount").get<std::size_t>() == 0;
	}
	for (const Json& entry : payload.at("entries")) {
		if (entry.is_object() && entry.value("role", std::string()) == "observation") return false;
	}
	return true;
}

// supportsに挙がったEvidenceのうち、1件でも観測があればtrue。
bool SupportsIncludeObservation(const Json& supports, const Json& builtEvidence) {
	if (!supports.is_array() || supports.empty()) return false;
	if (!builtEvidence.is_object() || !builtEvidence.contains("evidences") ||
	    !builtEvidence.at("evidences").is_array()) {
		return true; // Evidence一覧が読めないなら、この判定では落とさない
	}
	const Json& evidences = builtEvidence.at("evidences");
	for (const Json& supportId : supports) {
		if (!supportId.is_number_integer()) continue;
		const std::int64_t id = supportId.get<std::int64_t>();
		for (const Json& evidence : evidences) {
			if (!evidence.is_object()) continue;
			if (evidence.value("id", std::int64_t(-1)) != id) continue;
			if (!IsReferenceOnlyEvidence(evidence)) return true;
		}
	}
	return false;
}

// ---------------------------------
// 最上位仮説が構造的に成立しているか
// ---------------------------------
//
// confidenceは小規模モデルが自己申告するスカラーであり、根拠が無い。
// 実機で、内容が正しく supports もあり missingEvidence も contradicts も空の
// 仮説に対してモデルが0.38を振り、閾値0.4に0.02足りずhard failになった。
//
// このアーキテクチャは他方で、LLMの自己申告 goalSatisfied==true を
// 「passの十分条件にはしない」と明確に退けている。同じ自己申告である
// confidenceだけをhard gateとして無条件に信頼するのは一貫していない。
//
// そこでスカラーではなく構造で見る。
//   - 支持するEvidenceが存在する（根拠がある）
//   - 不足Evidenceが挙がっていない（本人が穴を認識していない）
//   - 矛盾するEvidenceを抱えていない
// いずれもモデルが数値を盛って回避できる類のものではない。
// confidenceはprogrammaticScoreの材料としては引き続き使う（連続量が要るため）。
struct TopHypothesisShape {
	bool exists = false;
	bool hasSupport = false;
	// supportsに観測（Engine Tool結果 / 過去のTool結果）が1件でも含まれるか。
	// 参照系Evidenceだけで支えられた仮説は「根拠あり」と認めない。
	bool hasObservationSupport = false;
	bool hasMissingEvidence = false;
	bool hasContradiction = false;
	double confidence = 0.0;

	// 構造的に自立している仮説か
	bool IsWellFormed() const {
		return exists && hasSupport && hasObservationSupport &&
			!hasMissingEvidence && !hasContradiction;
	}
};

TopHypothesisShape InspectTopHypothesis(const Json& rankedHypotheses, const Json& builtEvidence) {
	TopHypothesisShape shape;
	if (!rankedHypotheses.is_object() || !rankedHypotheses.contains("hypotheses") ||
	    !rankedHypotheses.at("hypotheses").is_array() || rankedHypotheses.at("hypotheses").empty()) {
		return shape;
	}
	const Json& top = rankedHypotheses.at("hypotheses")[0];
	if (!top.is_object()) return shape;

	shape.exists = true;
	if (top.contains("confidence") && top.at("confidence").is_number()) {
		shape.confidence = top.at("confidence").get<double>();
	}
	if (top.contains("supports") && top.at("supports").is_array()) {
		shape.hasSupport = !top.at("supports").empty();
		shape.hasObservationSupport =
			SupportsIncludeObservation(top.at("supports"), builtEvidence);
	}
	if (top.contains("missingEvidence") && top.at("missingEvidence").is_array()) {
		shape.hasMissingEvidence = !top.at("missingEvidence").empty();
	}
	if (top.contains("contradicts") && top.at("contradicts").is_array()) {
		shape.hasContradiction = !top.at("contradicts").empty();
	}
	return shape;
}

// Respond以外のツール由来のEvidenceが1件でもあるか。
// provenance.sourceTypeは RetrievalWorker が "Tool:<名前>" 形式で付ける。
bool EvidenceUsesNonRespondTool(const Json& builtEvidence) {
	if (!builtEvidence.is_object() || !builtEvidence.contains("evidences") ||
	    !builtEvidence.at("evidences").is_array()) {
		return true; // 読めないなら安全側（観測を要求する）
	}
	const std::string respondSource = std::string("Tool:") + RespondToolName();
	for (const Json& evidence : builtEvidence.at("evidences")) {
		if (!evidence.is_object() || !evidence.contains("provenance") ||
		    !evidence.at("provenance").is_object()) {
			continue;
		}
		const std::string sourceType =
			evidence.at("provenance").value("sourceType", std::string());
		if (sourceType.rfind("Tool:", 0) != 0) continue;
		if (sourceType != respondSource) return true;
	}
	return false;
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

// Task種別(type)は廃止した。修復提案の実体も「どのToolを使うか」だけ。
//
// 以前は RuntimeObservation / CodeSearch / Trace のいずれかに正規化し、
// それ以外を捨てていた。そのためCriticが「応答すればよい」と判断しても
// 提案として通せず、修復の選択肢が調査に限定されていた。
// 件数上限も設けない（不足の数は要求が決める）。
Json NormalizeRepairTasks(const Json& rawTasks) {
	Json normalized = Json::array();
	if (!rawTasks.is_array()) return normalized;

	for (const Json& task : rawTasks) {
		if (!task.is_object()) continue;
		const std::string tool = task.value("tool", std::string());
		if (tool.empty()) continue; // Toolを指さない提案は実行できない

		std::string description = task.value("description", std::string());
		if (description.empty()) description = "Criticが要求した追加Task";
		if (!tool.empty()) {
			const Json command = Json::object({
				{"tool", tool},
				{"arguments", task.value("arguments", Json::object())},
			});
			description += "\nREPAIR_COMMAND " + command.dump();
		}

		normalized.push_back(Json::object({{"description", description}}));
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
// 目的識別子を「要求の権威ある表現」から集める。
//
// 実機失敗（transcript_20260726_230620）:
//   入力     「SqliteDb::Prepareの実装を見せて」
//   Intake   「SqliteDb::Prepare メソッドの実装コードを表示する」へ言い換え
//   結果     Intakeが足しただけの「メソッド」「コード」を目的識別子として
//            要求し、Evidenceに無いためhard fail。修復Taskが2つ合成され、
//            どちらもEvidenceを産まず coverage 1.0 → 0.333 へ悪化した。
//
//   このときLLM Criticはプロンプト内でcurrentUserInputを見ており、
//   goalSatisfied=true・全項目1.0と正しく判定していた。
//   つまり「原文を見ている側には決定権がなく、決定権を持つ決定的ゲートは
//   原文を見ていない」という配線の欠落が原因である。
//
// よってここではresolvedRequest（自由文の言い換え）を読まない。
std::vector<std::string> CollectAuthoritativeGoalIdentifiers() {
	const std::string userInput = prompts::CurrentUserInput();
	const std::string targetConcept = prompts::CurrentTargetConcept();
	const std::string resolvedEntity = prompts::CurrentResolvedEntityName();

	std::vector<std::string> identifiers;
	std::unordered_set<std::string> seen;
	auto append = [&](const std::vector<std::string>& source) {
		for (const std::string& id : source) {
			const std::string key = IsAsciiOnlyIdentifier(id) ? ToLowerAsciiIdentifier(id) : id;
			if (seen.count(key) != 0) continue;
			seen.insert(key);
			identifiers.push_back(id);
		}
	};

	// 原文からはASCII識別子と引用トークンのみ。理由はCriticAgent.hを参照。
	append(critic_internal::ExtractGoalIdentifiers(userInput, /*includeKatakana=*/false));
	// 構造化フィールドはIntakeが対象として確定させたものなので全形式を取る。
	append(critic_internal::ExtractGoalIdentifiers(targetConcept));
	append(critic_internal::ExtractGoalIdentifiers(resolvedEntity));

	// 原文も構造化フィールドも一切無いときだけresolvedRequestへ退避する。
	// 本番のIntakeは両経路でcurrentUserInputを必ず設定するため、
	// ここへ来るのはContextが未設定の場合（単体テスト等）に限られる。
	if (userInput.empty() && targetConcept.empty() && resolvedEntity.empty()) {
		append(critic_internal::ExtractGoalIdentifiers(prompts::CurrentResolvedRequest()));
	}
	return identifiers;
}

bool ApplyGoalIdentifierGate(const Json& builtEvidence, CriticVerdict* out) {
	const std::vector<std::string> identifiers = CollectAuthoritativeGoalIdentifiers();
	if (identifiers.empty()) return true;

	const std::vector<std::string> missing = FindMissingGoalIdentifiers(identifiers, builtEvidence);
	if (missing.empty()) return true;

	const std::string joined = JoinIdentifiers(missing);
	AddFailureOnce(out, "goal identifiers not covered by evidence: " + joined);

	if (!out->additionalTasks.is_array() || out->additionalTasks.empty()) {
		out->additionalTasks = Json::array({
			Json::object({
				{"description", "不足識別子 " + joined + " に関するEvidenceを取得する"},
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

		// 撤回すべきTaskの指名。追加(additionalTasksSuggested)と対になる。
		// ここではID列として正規化するだけで、撤回の可否は
		// EvidenceBuilder側の決定的ガードが判断する。
		if (raw.contains("obsoleteTasks") && raw.at("obsoleteTasks").is_array()) {
			Json normalized = Json::array();
			for (const Json& entry : raw.at("obsoleteTasks")) {
				// {"taskId": 205, "reason": "..."} と 205 の両方を受ける。
				// 小規模モデルは形式を揺らしやすいため寛容に読む。
				const Json& idNode = entry.is_object() ? entry.value("taskId", Json()) : entry;
				if (!idNode.is_number_integer()) continue;
				const std::int64_t taskId = idNode.get<std::int64_t>();
				if (taskId <= 0) continue;
				normalized.push_back(taskId);
			}
			out->obsoleteTasks = std::move(normalized);
		}
	} else {
		out->failures.push_back("critic LLM call failed: " + callResult.error);
	}

	const double coverage = builtEvidence.is_object() ? builtEvidence.value("coverage", 0.0) : 0.0;
	const TopHypothesisShape topHypothesis = InspectTopHypothesis(rankedHypotheses, builtEvidence);

	// 観測要件を適用するかは「要求の種類」ではなく「実際に使ったツール」で決める。
	//
	// 会話応答（Respond）は観測ではないので、常に観測を要求すると
	// 挨拶や「私は誰ですか？」が必ず未完了になる。
	// かといって要求の種類で分岐すると、キーワード判定に逆戻りする。
	// Respond以外のツールが1つでも使われていれば、それはEngine/コードの
	// 事実を主張しているので観測が要る。使われていなければ要らない。
	const bool requiresObservation = EvidenceUsesNonRespondTool(builtEvidence);
	const double topConfidence = topHypothesis.confidence;
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
	// ゲート#7: 最上位仮説が構造的に自立しているか。
	// 以前はconfidence < 0.4というスカラー閾値だったが、
	// これはモデルの自己申告であり根拠が無い（詳細はInspectTopHypothesisのコメント）。
	if (!topHypothesis.exists) {
		AddFailureOnce(out, "programmatic hard fail: no hypothesis was produced");
		hardFail = true;
	} else if (!topHypothesis.hasSupport) {
		AddFailureOnce(out, "programmatic hard fail: top hypothesis has no supporting evidence");
		hardFail = true;
	} else if (!topHypothesis.hasObservationSupport && requiresObservation) {
		// 会話履歴の参照系Evidence（過去発話・要約・過去のAgent応答）だけで
		// 支えられた仮説。参照解決には使えるが観測ではないので断定できない。
		AddFailureOnce(
			out,
			"programmatic hard fail: top hypothesis is supported only by conversation "
			"references (no observation)");
		hardFail = true;
	} else if (topHypothesis.hasMissingEvidence) {
		AddFailureOnce(out, "programmatic hard fail: top hypothesis declares missing evidence");
		hardFail = true;
	} else if (topHypothesis.hasContradiction) {
		AddFailureOnce(out, "programmatic hard fail: top hypothesis contradicts collected evidence");
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
