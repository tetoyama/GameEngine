// =======================================================================
//
// IntakeAgent.cpp
//
// =======================================================================
#include "IntakeAgent.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace agentos {

namespace {

constexpr std::size_t kKeepRecentTurns = 6;
constexpr std::size_t kCompressTurnThreshold = 9;
constexpr std::size_t kCompressCharThreshold = 12000;

void NormalizeStringArray(const Json& raw, const char* key, Json* normalized) {
	Json arr = Json::array();
	if (raw.contains(key) && raw.at(key).is_array()) {
		for (const auto& v : raw.at(key)) {
			if (v.is_string() && !v.get<std::string>().empty()) arr.push_back(v.get<std::string>());
		}
	}
	(*normalized)[key] = std::move(arr);
}

Json NormalizeReferencedSessionIds(const Json& raw) {
	Json ids = Json::array();
	if (!raw.is_object() || !raw.contains("referencedSessionIds") ||
	    !raw.at("referencedSessionIds").is_array()) {
		return ids;
	}
	for (const Json& value : raw.at("referencedSessionIds")) {
		if (value.is_number_integer()) ids.push_back(value.get<SessionId>());
	}
	return ids;
}

std::string NormalizeRelation(const Json& raw) {
	if (!raw.contains("turnRelation") || !raw.at("turnRelation").is_string()) return "new";
	const std::string value = raw.at("turnRelation").get<std::string>();
	if (value == "new" || value == "continue" || value == "correct" ||
	    value == "clarify" || value == "refer" || value == "refresh") {
		return value;
	}
	return "new";
}

std::size_t ConversationChars(const Json& context) {
	if (!context.is_object() || !context.contains("recentTurns") ||
	    !context.at("recentTurns").is_array()) return 0;
	std::size_t chars = 0;
	for (const Json& turn : context.at("recentTurns")) {
		if (!turn.is_object()) continue;
		chars += turn.value("user", std::string()).size();
		chars += turn.value("assistant", std::string()).size();
	}
	return chars;
}

bool NeedsCompression(const Json& context) {
	if (!context.is_object() || !context.contains("recentTurns") ||
	    !context.at("recentTurns").is_array()) return false;
	const std::size_t count = context.at("recentTurns").size();
	return count >= kCompressTurnThreshold || ConversationChars(context) > kCompressCharThreshold;
}

Json LatestTurnsOnly(const Json& context, const std::string& reason) {
	Json compact = context;
	Json latest = Json::array();
	if (context.is_object() && context.contains("recentTurns") &&
	    context.at("recentTurns").is_array()) {
		const Json& turns = context.at("recentTurns");
		const std::size_t begin = turns.size() > kKeepRecentTurns
			? turns.size() - kKeepRecentTurns
			: 0;
		for (std::size_t i = begin; i < turns.size(); ++i) latest.push_back(turns[i]);
	}
	compact["recentTurns"] = std::move(latest);
	compact["contextDegraded"] = true;
	compact["contextDegradedReason"] = reason;
	return compact;
}

std::string BuildDeterministicSummary(
	const std::string& existingSummary,
	const Json& turnsToCompress) {

	std::string added;
	if (turnsToCompress.is_array()) {
		for (std::size_t index = turnsToCompress.size(); index > 0; --index) {
			const Json& turn = turnsToCompress[index - 1];
			if (!turn.is_object()) continue;
			const std::string segment =
				"- User: " + prompts::Truncate(turn.value("user", std::string()), 360) +
				"\n  Assistant final: " +
				prompts::Truncate(turn.value("assistant", std::string()), 560) + "\n";
			if (added.size() + segment.size() > 2600) break;
			added = segment + added;
		}
	}

	std::string summary = prompts::Truncate(existingSummary, 1200);
	if (!summary.empty()) summary += "\n";
	summary += "[deterministic conversation compression]\n" + added;
	return prompts::Truncate(summary, 4000);
}

Result CompressStoredConversationIfNeeded(AgentContext& ctx, Json* context) {
	if (context == nullptr || ctx.store == nullptr || !NeedsCompression(*context)) {
		return Result::Ok();
	}

	const Json& turns = context->at("recentTurns");
	if (turns.size() <= kKeepRecentTurns) return Result::Ok();

	const std::size_t compressCount = turns.size() - kKeepRecentTurns;
	Json toCompress = Json::array();
	for (std::size_t i = 0; i < compressCount; ++i) toCompress.push_back(turns[i]);

	const Json& lastCompressed = toCompress.back();
	if (!lastCompressed.is_object() || !lastCompressed.contains("sessionId") ||
	    !lastCompressed.at("sessionId").is_number_integer()) {
		*context = LatestTurnsOnly(*context, "compressed turn has no sessionId");
		return Result::Fail("IntakeAgent: compressed turn has no sessionId");
	}
	const SessionId through = lastCompressed.at("sessionId").get<SessionId>();
	const std::string existingSummary = context->value("summary", std::string());

	const PromptPair prompt = prompts::CompressConversationMemory(existingSummary, toCompress);
	Json raw;
	const Result callResult = CallLlmJson(ctx, prompt, &raw);
	const bool generatedSummary =
		callResult && raw.is_object() && raw.contains("summary") &&
		raw.at("summary").is_string() && !raw.at("summary").get<std::string>().empty();

	const std::string summary = generatedSummary
		? raw.at("summary").get<std::string>()
		: BuildDeterministicSummary(existingSummary, toCompress);

	Result updateResult = ctx.store->UpdateConversationSummary(summary, through);
	if (!updateResult) {
		*context = LatestTurnsOnly(*context, "conversation summary persistence failed");
		return updateResult;
	}

	*context = ctx.store->GetConversationContext(ctx.sessionId);
	if (!generatedSummary) {
		(*context)["contextDegraded"] = true;
		(*context)["contextDegradedReason"] = callResult
			? "conversation compression returned no summary; deterministic summary used"
			: "conversation compression LLM failed; deterministic summary used";
	}
	return Result::Ok();
}

bool ContainsAny(const std::string& text, std::initializer_list<const char*> needles) {
	for (const char* needle : needles) {
		if (needle != nullptr && text.find(needle) != std::string::npos) return true;
	}
	return false;
}

std::string Trim(std::string value) {
	auto isNotSpace = [](unsigned char ch) { return std::isspace(ch) == 0; };
	value.erase(value.begin(), std::find_if(value.begin(), value.end(), isNotSpace));
	value.erase(std::find_if(value.rbegin(), value.rend(), isNotSpace).base(), value.end());
	return value;
}

// ---------------------------------
// CHANGE 1: LLM出力に対するsymptoms/constraints Grounding filter。
//
// 原則: LLM出力は信頼しない。IntakeのLLMはconversationContext越しに過去turnの
// 失敗記述（ツールエラー・許可エラー等）を今回turnのsymptoms/constraintsへ
// 紛れ込ませることがある。これらは今回turnのEvidenceではないため、決定的な
// Grounding判定で「現在turnに根拠がある」ものだけをsymptoms/constraintsに残し、
// それ以外の失敗語彙混じりの記述はmemoryDerivedNotesへ隔離する
// （ルーティング文言・Plannerプロンプト・最終reportへ混入させない）。
// これはSelectConversationContextによる一次防御（後続Agentへ渡す履歴の絞り込み）を
// 置き換えるものではなく、IntakeのLLM出力そのものに対する二次防御である。
// ---------------------------------

constexpr std::size_t kMinIdentifierLen = 3;

bool IsAsciiWordChar(unsigned char ch) {
	return (std::isalnum(ch) != 0) || ch == '_';
}

std::size_t Utf8SequenceLength(unsigned char leadByte) {
	if ((leadByte & 0x80) == 0x00) return 1;
	if ((leadByte & 0xE0) == 0xC0) return 2;
	if ((leadByte & 0xF0) == 0xE0) return 3;
	if ((leadByte & 0xF8) == 0xF0) return 4;
	return 1; // 不正なバイト列は1バイトずつ読み進めて壊れないようにする。
}

// U+30A0-U+30FF（カタカナブロック）をUTF-8バイト列から判定する簡易ヒューリスティック。
bool IsKatakanaUtf8Char(const std::string& text, std::size_t pos, std::size_t len) {
	if (len != 3 || pos + 3 > text.size()) return false;
	const unsigned char b0 = static_cast<unsigned char>(text[pos]);
	const unsigned char b1 = static_cast<unsigned char>(text[pos + 1]);
	if (b0 != 0xE3) return false;
	if (b1 == 0x82) return static_cast<unsigned char>(text[pos + 2]) >= 0xA0;
	if (b1 == 0x83) return static_cast<unsigned char>(text[pos + 2]) <= 0xBF;
	return false;
}

// テキストから「識別子候補」を集める:
//   - ASCIIトークン（英数字/アンダースコア、長さ3以上、小文字化して比較）
//   - カタカナ連続（長さ3コードポイント以上）
//   - 引用トークン（"..." '...' `...` 「...」 『...』の中身）
void CollectDistinctiveIdentifiers(const std::string& text, std::unordered_set<std::string>* out) {
	std::string asciiToken;
	auto flushAscii = [&]() {
		if (asciiToken.size() >= kMinIdentifierLen) {
			std::string lower = asciiToken;
			std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			});
			out->insert(std::move(lower));
		}
		asciiToken.clear();
	};

	std::string katakanaRun;
	std::size_t katakanaCount = 0;
	auto flushKatakana = [&]() {
		if (katakanaCount >= kMinIdentifierLen) out->insert(katakanaRun);
		katakanaRun.clear();
		katakanaCount = 0;
	};

	std::size_t i = 0;
	while (i < text.size()) {
		const unsigned char lead = static_cast<unsigned char>(text[i]);
		const std::size_t len = Utf8SequenceLength(lead);
		if (len == 1) {
			flushKatakana();
			if (IsAsciiWordChar(lead)) {
				asciiToken.push_back(static_cast<char>(lead));
			} else {
				flushAscii();
			}
			++i;
			continue;
		}
		flushAscii();
		if (IsKatakanaUtf8Char(text, i, len)) {
			katakanaRun.append(text, i, len);
			++katakanaCount;
		} else {
			flushKatakana();
		}
		i += len;
	}
	flushAscii();
	flushKatakana();

	static const std::vector<std::pair<std::string, std::string>> kQuotePairs = {
		{"\"", "\""}, {"'", "'"}, {"`", "`"}, {"「", "」"}, {"『", "』"},
	};
	for (const auto& quotePair : kQuotePairs) {
		std::size_t searchPos = 0;
		while (true) {
			const std::size_t start = text.find(quotePair.first, searchPos);
			if (start == std::string::npos) break;
			const std::size_t contentStart = start + quotePair.first.size();
			const std::size_t end = text.find(quotePair.second, contentStart);
			if (end == std::string::npos) break;
			const std::string inner = Trim(text.substr(contentStart, end - contentStart));
			if (!inner.empty()) out->insert(inner);
			searchPos = end + quotePair.second.size();
		}
	}
}

bool ContainsIdentifierOverlap(const std::string& text, const std::unordered_set<std::string>& currentIdentifiers) {
	if (currentIdentifiers.empty()) return false;
	std::unordered_set<std::string> entryIdentifiers;
	CollectDistinctiveIdentifiers(text, &entryIdentifiers);
	for (const std::string& identifier : entryIdentifiers) {
		if (currentIdentifiers.count(identifier) != 0) return true;
	}
	return false;
}

bool ContainsFailureVocabulary(const std::string& text) {
	static const std::initializer_list<const char*> kJapaneseFailureWords = {
		"エラー", "失敗", "利用不可", "許可", "ツール実行",
	};
	if (ContainsAny(text, kJapaneseFailureWords)) return true;

	std::string lower = text;
	std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	static const std::initializer_list<const char*> kAsciiFailureWords = {
		"rejected", "failed", "unknown field",
	};
	return ContainsAny(lower, kAsciiFailureWords);
}

// symptoms/constraintsのうち、失敗語彙を含みかつ現在turnに根拠がない記述を
// memoryDerivedNotesへ退避する。それ以外（根拠があるもの・失敗語彙を含まないもの）は
// そのまま残す。
void ApplyMemoryGroundingFilter(
	const std::string& currentUserInput,
	const std::string& resolvedRequest,
	Json* normalized) {

	if (normalized == nullptr) return;

	std::unordered_set<std::string> currentIdentifiers;
	CollectDistinctiveIdentifiers(currentUserInput, &currentIdentifiers);
	CollectDistinctiveIdentifiers(resolvedRequest, &currentIdentifiers);

	Json memoryDerivedNotes = Json::array();
	for (const char* key : {"symptoms", "constraints"}) {
		if (!normalized->contains(key) || !(*normalized)[key].is_array()) continue;
		Json kept = Json::array();
		for (const Json& entry : (*normalized)[key]) {
			if (!entry.is_string()) continue;
			const std::string text = entry.get<std::string>();
			const bool hasFailureVocab = ContainsFailureVocabulary(text);
			const bool grounded = ContainsIdentifierOverlap(text, currentIdentifiers);
			if (!hasFailureVocab || grounded) {
				kept.push_back(text);
			} else {
				memoryDerivedNotes.push_back(text);
			}
		}
		(*normalized)[key] = std::move(kept);
	}
	(*normalized)["memoryDerivedNotes"] = std::move(memoryDerivedNotes);
}

Json EmptyStructuredContext(const std::string& policy, const Json& fullContext = Json::object()) {
	Json selected = Json::object();
	selected["summary"] = "";
	selected["summarizedThroughSessionId"] =
		fullContext.value("summarizedThroughSessionId", kInvalidId);
	selected["totalTurns"] = fullContext.value("totalTurns", 0);
	selected["recentTurns"] = Json::array();
	selected["threadStates"] = Json::array();
	selected["selectionPolicy"] = policy;
	selected["memoryPolicy"] = "structured_thread_state_only";
	return selected;
}

Json BoundedRecentTurn(const Json& turn) {
	Json result = Json::object();
	if (!turn.is_object()) return result;
	if (turn.contains("sessionId")) result["sessionId"] = turn.at("sessionId");
	result["user"] = prompts::Truncate(turn.value("user", std::string()), 600);
	// 構造化Thread StateがまだDBに無い場合の一次履歴。訂正解決（「そうじゃなくて」等）は
	// 直前のassistant応答が見えないと成立しないため、切り詰めた上でassistant本文も渡す。
	// Intakeプロンプト自身が全履歴参照を許可している段階であり（PromptTemplates::Intake参照）、
	// 再注入リスクは長さ制限で抑える。threadStateが存在する通常経路はこのフォールバックを通らない。
	const std::string assistant = turn.value("assistant", std::string());
	if (!assistant.empty()) result["assistant"] = prompts::Truncate(assistant, 560);
	return result;
}

// 履歴を渡すかどうかを先回りで決めない。
//
// 以前は「続けて」「それ」「さっき」等14語の部分一致で決めていたため、
// 「5件全部知りたい」のような明らかな継続を取りこぼし、DBに51件あるのに
// totalTurns=0 のままIntakeへ渡していた（transcript_20260727_013726）。
// Intakeの仕事は参照解決なので、材料は常に渡す。
Json BuildIntakePromptContext(const std::string& userRequest, const Json& fullContext) {
	(void)userRequest;
	Json selected = EmptyStructuredContext("structured_thread_reference", fullContext);
	if (fullContext.contains("threadStates") && fullContext.at("threadStates").is_array()) {
		const Json& states = fullContext.at("threadStates");
		const std::size_t begin = states.size() > 3 ? states.size() - 3 : 0;
		for (std::size_t i = begin; i < states.size(); ++i) {
			if (states[i].is_object()) selected["threadStates"].push_back(states[i]);
		}
	}

	// 既存DBにThread Stateがまだ無い場合だけ、直近turnを短く渡す。
	// 訂正解決に必要な直前assistant応答も、切り詰めた上で含める（BoundedRecentTurn）。
	// この経路はthreadStateが無いときのみ通り、通常の構造化経路とは分離されている。
	if (selected["threadStates"].empty() && fullContext.contains("recentTurns") &&
	    fullContext.at("recentTurns").is_array()) {
		const Json& turns = fullContext.at("recentTurns");
		const std::size_t begin = turns.size() > 2 ? turns.size() - 2 : 0;
		for (std::size_t i = begin; i < turns.size(); ++i) {
			const Json bounded = BoundedRecentTurn(turns[i]);
			if (!bounded.empty()) selected["recentTurns"].push_back(bounded);
		}
		selected["selectionPolicy"] = "bounded_recent_turn_fallback";
	}
	return selected;
}

Json SelectConversationContext(
	const Json& promptContext,
	const std::string& relation,
	const Json& referencedSessionIds) {

	if (relation == "new" || relation == "refresh") {
		return EmptyStructuredContext(
			relation == "refresh" ? "fresh_runtime_observation" : "active_turn_only",
			promptContext);
	}

	Json selected = EmptyStructuredContext("structured_" + relation, promptContext);
	std::unordered_set<SessionId> referenced;
	if (referencedSessionIds.is_array()) {
		for (const Json& id : referencedSessionIds) {
			if (id.is_number_integer()) referenced.insert(id.get<SessionId>());
		}
	}

	auto appendMatching = [&](const Json& values, const char* destination) {
		if (!values.is_array()) return;
		for (const Json& value : values) {
			if (!value.is_object()) continue;
			if (!referenced.empty()) {
				if (!value.contains("sessionId") || !value.at("sessionId").is_number_integer() ||
				    referenced.count(value.at("sessionId").get<SessionId>()) == 0) continue;
			}
			selected[destination].push_back(value);
		}
	};

	if (promptContext.contains("threadStates")) {
		appendMatching(promptContext.at("threadStates"), "threadStates");
	}
	if (selected["threadStates"].empty() && promptContext.contains("recentTurns")) {
		appendMatching(promptContext.at("recentTurns"), "recentTurns");
	}
	if (!referenced.empty()) selected["selectionPolicy"] = "explicit_structured_session_reference";
	return selected;
}

Json BuildThreadState(SessionId sessionId, const Json& intake) {
	Json state = Json::object();
	state["sessionId"] = sessionId;
	state["goal"] = intake.value("goal", std::string());
	state["resolvedRequest"] = intake.value("resolvedRequest", std::string());
	state["turnRelation"] = intake.value("turnRelation", std::string("new"));
	state["requestType"] = intake.value("requestType", std::string("investigation"));
	state["targets"] = Json::object({
		{"kind", intake.value("targetKind", std::string("unknown"))},
		{"concept", intake.contains("targetConcept") ? intake.at("targetConcept") : Json(nullptr)},
		{"entityName", intake.contains("resolvedEntityName") ? intake.at("resolvedEntityName") : Json(nullptr)},
	});
	state["constraints"] = intake.value("constraints", Json::array());
	state["unresolved"] = intake.value("unresolvedReferences", Json::array());
	state["status"] = intake.value("simpleConversation", false) ? "conversation" : "active";
	return state;
}

void CollectIdentifiersFromText(const std::string& text, std::unordered_set<std::string>* out) {
	static const std::unordered_set<std::string> kStopWords = {
		"User", "Assistant", "Scene", "Entity", "Component", "System", "Tool", "AgentOS",
		"Runtime", "Current", "Report", "Field", // Fieldは一般語にもなり得るが固有名ログでは後で復元する。
	};

	std::string token;
	auto flush = [&]() {
		if (token.size() >= 3 && kStopWords.count(token) == 0) out->insert(token);
		token.clear();
	};
	for (unsigned char ch : text) {
		if (std::isalnum(ch) != 0 || ch == '_' || ch == ':' || ch == '/' || ch == '.' || ch == '-') {
			token.push_back(static_cast<char>(ch));
		} else {
			flush();
		}
	}
	flush();

	// 実機で頻出する短い固有Entity名は明示的に拾う。
	for (const char* known : {"Field", "Player", "Camera", "Light", "SkyBox", "MainCamera"}) {
		if (text.find(known) != std::string::npos) out->insert(known);
	}
}

Json ExtractHistoryIdentifiers(const Json& structuredContext) {
	std::unordered_set<std::string> identifiers;
	if (structuredContext.is_object()) {
		if (structuredContext.contains("threadStates") && structuredContext.at("threadStates").is_array()) {
			for (const Json& state : structuredContext.at("threadStates")) {
				if (!state.is_object()) continue;
				CollectIdentifiersFromText(state.value("goal", std::string()), &identifiers);
				CollectIdentifiersFromText(state.value("resolvedRequest", std::string()), &identifiers);
				if (state.contains("targets")) CollectIdentifiersFromText(state.at("targets").dump(), &identifiers);
			}
		}
		if (structuredContext.contains("recentTurns") && structuredContext.at("recentTurns").is_array()) {
			for (const Json& turn : structuredContext.at("recentTurns")) {
				if (turn.is_object()) CollectIdentifiersFromText(turn.value("user", std::string()), &identifiers);
			}
		}
	}
	Json result = Json::array();
	std::vector<std::string> ordered(identifiers.begin(), identifiers.end());
	std::sort(ordered.begin(), ordered.end());
	for (const std::string& identifier : ordered) result.push_back(identifier);
	return result;
}

} // namespace

Result IntakeAgent::Run(
	AgentContext& ctx,
	const std::string& userRequest,
	const Json& conversationContext,
	Json* intakeOut) {

	prompts::ClearCurrentConversationRequestContext();
	if (intakeOut == nullptr) return Result::Fail("IntakeAgent: intakeOut is null");

	// 履歴は常に読む。キーワード一致で読むかどうかを決めない（上の注記参照）。
	Json fullContext = conversationContext;
	if ((!fullContext.is_object() || fullContext.empty()) &&
	    ctx.store != nullptr && ctx.sessionId != kInvalidId) {
		fullContext = ctx.store->GetConversationContext(ctx.sessionId);
	}

	// 会話履歴が閾値を超えたら、プロンプト構築の前に自動圧縮する。
	// 圧縮は古いturnを要約へ畳み込み、直近rawturnだけを残す（*fullContextを更新）。
	// 圧縮の成否はIntake継続を妨げない（失敗時は関数内で安全な縮約へフォールバックする）。
	CompressStoredConversationIfNeeded(ctx, &fullContext);

	const Json promptContext = BuildIntakePromptContext(userRequest, fullContext);

	const PromptPair prompt = prompts::Intake(userRequest, promptContext);
	Json raw;
	Result callResult = CallLlmJson(ctx, prompt, &raw);
	if (!callResult) return callResult;

	if (!raw.is_object() || !raw.contains("goal") || !raw.at("goal").is_string() ||
	    raw.at("goal").get<std::string>().empty()) {
		return Result::Fail("IntakeAgent: response missing non-empty string 'goal'");
	}

	Json normalized = Json::object();
	std::string goal = raw.at("goal").get<std::string>();
	std::string resolvedRequest = goal;
	if (raw.contains("resolvedRequest") && raw.at("resolvedRequest").is_string() &&
	    !raw.at("resolvedRequest").get<std::string>().empty()) {
		resolvedRequest = raw.at("resolvedRequest").get<std::string>();
	}
	std::string relation = NormalizeRelation(raw);

	std::string requestType = "investigation";
	if (raw.contains("requestType") && raw.at("requestType").is_string()) {
		const std::string rawType = raw.at("requestType").get<std::string>();
		if (rawType == "conversation" || rawType == "investigation") requestType = rawType;
	}

	normalized["goal"] = goal;
	normalized["resolvedRequest"] = resolvedRequest;
	normalized["rootGoal"] = goal;
	normalized["rootResolvedRequest"] = resolvedRequest;
	normalized["turnRelation"] = relation;
	NormalizeStringArray(raw, "symptoms", &normalized);
	NormalizeStringArray(raw, "constraints", &normalized);
	NormalizeStringArray(raw, "requiredCapabilities", &normalized);
	NormalizeStringArray(raw, "unresolvedReferences", &normalized);
	normalized["referencedSessionIds"] = NormalizeReferencedSessionIds(raw);
	normalized["requestType"] = requestType;
	normalized["currentUserInput"] = userRequest;
	normalized["requestRevision"] = 0;

	const std::string targetKind = raw.value("targetKind", std::string("unknown"));
	normalized["targetKind"] = targetKind;
	normalized["targetConcept"] = raw.contains("targetConcept") && raw.at("targetConcept").is_string()
		? raw.at("targetConcept")
		: Json(nullptr);
	normalized["resolvedEntityName"] =
		raw.contains("resolvedEntityName") && raw.at("resolvedEntityName").is_string()
			? raw.at("resolvedEntityName")
			: Json(nullptr);

	const bool hasPreviousTurns =
		(promptContext.contains("threadStates") && promptContext.at("threadStates").is_array() &&
		 !promptContext.at("threadStates").empty()) ||
		(promptContext.contains("recentTurns") && promptContext.at("recentTurns").is_array() &&
		 !promptContext.at("recentTurns").empty());
	// turnRelationをキーワードで上書きしない。Intakeの判定をそのまま使う。
	(void)hasPreviousTurns;
	normalized["turnRelation"] = relation;
	normalized["requestType"] = requestType;

	ApplyMemoryGroundingFilter(userRequest, resolvedRequest, &normalized);

	const Json selectedContext = SelectConversationContext(
		promptContext,
		relation,
		normalized.value("referencedSessionIds", Json::array()));
	normalized["historyIdentifiers"] = ExtractHistoryIdentifiers(selectedContext);
	normalized["conversationContext"] = selectedContext;

	if (ctx.store != nullptr && ctx.sessionId != kInvalidId) {
		(void)ctx.store->SetConversationThreadState(ctx.sessionId, BuildThreadState(ctx.sessionId, normalized));
	}
	prompts::SetCurrentConversationRequestContext(selectedContext, normalized);
	*intakeOut = std::move(normalized);
	return Result::Ok();
}


} // namespace agentos
