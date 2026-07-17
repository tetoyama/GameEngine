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
	    value == "clarify" || value == "refer") {
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

bool IsSimpleConversationInput(const std::string& userRequest) {
	if (userRequest.empty() || userRequest.size() > 96) return false;
	const bool greeting = ContainsAny(userRequest, {
		"こんにちは", "こんばんは", "おはよう", "はじめまして", "ありがとう", "よろしく",
		"Hello", "hello", "Hi", "hi"
	});
	const bool engineRequest = ContainsAny(userRequest, {
		"Entity", "Component", "System", "シーン", "コード", "調査", "確認", "一覧", "修正"
	});
	return greeting && !engineRequest;
}

Json SelectConversationContext(
	const Json& fullContext,
	const std::string& relation,
	const Json& referencedSessionIds) {

	Json selected = Json::object();
	selected["summary"] = "";
	selected["summarizedThroughSessionId"] =
		fullContext.value("summarizedThroughSessionId", kInvalidId);
	selected["totalTurns"] = fullContext.value("totalTurns", 0);
	selected["recentTurns"] = Json::array();
	selected["selectionPolicy"] = relation;
	if (fullContext.contains("contextDegraded")) selected["contextDegraded"] = fullContext.at("contextDegraded");
	if (fullContext.contains("contextDegradedReason")) {
		selected["contextDegradedReason"] = fullContext.at("contextDegradedReason");
	}

	// newは履歴を保存したまま生成Contextから隔離する。
	if (relation == "new") {
		selected["selectionPolicy"] = "active_turn_only";
		return selected;
	}

	selected["summary"] = fullContext.value("summary", std::string());
	if (!fullContext.contains("recentTurns") || !fullContext.at("recentTurns").is_array()) {
		return selected;
	}
	const Json& turns = fullContext.at("recentTurns");

	std::unordered_set<SessionId> referenced;
	if (referencedSessionIds.is_array()) {
		for (const Json& id : referencedSessionIds) {
			if (id.is_number_integer()) referenced.insert(id.get<SessionId>());
		}
	}
	if (!referenced.empty()) {
		for (const Json& turn : turns) {
			if (turn.is_object() && turn.contains("sessionId") &&
			    turn.at("sessionId").is_number_integer() &&
			    referenced.count(turn.at("sessionId").get<SessionId>()) != 0) {
				selected["recentTurns"].push_back(turn);
			}
		}
		if (!selected["recentTurns"].empty()) {
			selected["selectionPolicy"] = "explicit_session_reference";
			return selected;
		}
	}

	std::size_t keep = relation == "refer" ? 3 : 2;
	if (relation == "clarify") keep = 3;
	const std::size_t begin = turns.size() > keep ? turns.size() - keep : 0;
	for (std::size_t i = begin; i < turns.size(); ++i) selected["recentTurns"].push_back(turns[i]);
	selected["selectionPolicy"] = "recent_related_" + relation;
	return selected;
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

Json ExtractHistoryIdentifiers(const Json& fullContext) {
	std::unordered_set<std::string> identifiers;
	if (fullContext.is_object()) {
		CollectIdentifiersFromText(fullContext.value("summary", std::string()), &identifiers);
		if (fullContext.contains("recentTurns") && fullContext.at("recentTurns").is_array()) {
			for (const Json& turn : fullContext.at("recentTurns")) {
				if (!turn.is_object()) continue;
				CollectIdentifiersFromText(turn.value("user", std::string()), &identifiers);
				CollectIdentifiersFromText(turn.value("assistant", std::string()), &identifiers);
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

	Json fullContext = conversationContext;
	if ((!fullContext.is_object() || fullContext.empty()) &&
	    ctx.store != nullptr && ctx.sessionId != kInvalidId) {
		fullContext = ctx.store->GetConversationContext(ctx.sessionId);
		(void)CompressStoredConversationIfNeeded(ctx, &fullContext);
	}

	const PromptPair prompt = prompts::Intake(userRequest, fullContext);
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
	normalized["turnRelation"] = relation;
	NormalizeStringArray(raw, "symptoms", &normalized);
	NormalizeStringArray(raw, "constraints", &normalized);
	NormalizeStringArray(raw, "requiredCapabilities", &normalized);
	NormalizeStringArray(raw, "unresolvedReferences", &normalized);
	normalized["referencedSessionIds"] = NormalizeReferencedSessionIds(raw);
	normalized["requestType"] = requestType;
	normalized["currentUserInput"] = userRequest;
	normalized["requestRevision"] = 0;
	normalized["historyIdentifiers"] = ExtractHistoryIdentifiers(fullContext);

	const bool simpleConversation = requestType == "conversation" && IsSimpleConversationInput(userRequest);
	if (simpleConversation) {
		normalized["goal"] = "ユーザーの挨拶または短い会話へ応答する";
		normalized["resolvedRequest"] = "現在のユーザー入力に対して短く自然に会話応答する";
		normalized["turnRelation"] = "new";
		normalized["symptoms"] = Json::array();
		normalized["constraints"] = Json::array();
		normalized["requiredCapabilities"] = Json::array();
		normalized["unresolvedReferences"] = Json::array();
		normalized["referencedSessionIds"] = Json::array();
		normalized["simpleConversation"] = true;
		relation = "new";
	}

	const Json selectedContext = SelectConversationContext(
		fullContext,
		relation,
		normalized.value("referencedSessionIds", Json::array()));
	normalized["conversationContext"] = selectedContext;

	prompts::SetCurrentConversationRequestContext(selectedContext, normalized);
	*intakeOut = std::move(normalized);
	return Result::Ok();
}

} // namespace agentos
