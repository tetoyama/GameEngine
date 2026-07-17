// =======================================================================
//
// IntakeAgent.cpp
//
// =======================================================================
#include "IntakeAgent.h"

#include <cstddef>
#include <string>

namespace agentos {

namespace {

constexpr std::size_t kKeepRecentTurns = 6;
constexpr std::size_t kCompressTurnThreshold = 9;
constexpr std::size_t kCompressCharThreshold = 12000;

void NormalizeStringArray(const Json& raw, const char* key, Json* normalized) {
	Json arr = Json::array();
	if (raw.contains(key) && raw.at(key).is_array()) {
		for (const auto& v : raw.at(key)) {
			if (v.is_string()) {
				arr.push_back(v.get<std::string>());
			}
		}
	(*normalized)[key] = std::move(arr);
}

std::string NormalizeRelation(const Json& raw) {
	if (!raw.contains("turnRelation") || !raw.at("turnRelation").is_string()) {
		return "new";
	}
	const std::string value = raw.at("turnRelation").get<std::string>();
	if (value == "new" || value == "continue" || value == "correct" ||
	    value == "clarify" || value == "refer") {
		return value;
	}
	return "new";
}

std::size_t ConversationChars(const Json& context) {
	if (!context.is_object() || !context.contains("recentTurns") ||
	    !context.at("recentTurns").is_array()) {
		return 0;
	}
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
	    !context.at("recentTurns").is_array()) {
		return false;
	}
	const std::size_t count = context.at("recentTurns").size();
	return count >= kCompressTurnThreshold || ConversationChars(context) > kCompressCharThreshold;
}

Json LatestTurnsOnly(const Json& context) {
	Json compact = context;
	Json latest = Json::array();
	if (context.is_object() && context.contains("recentTurns") &&
	    context.at("recentTurns").is_array()) {
		const Json& turns = context.at("recentTurns");
		const std::size_t begin = turns.size() > kKeepRecentTurns
			? turns.size() - kKeepRecentTurns
			: 0;
		for (std::size_t i = begin; i < turns.size(); ++i) {
			latest.push_back(turns[i]);
		}
	}
	compact["recentTurns"] = std::move(latest);
	compact["contextDegraded"] = true;
	compact["contextDegradedReason"] = "conversation compression failed; latest raw turns retained";
	return compact;
}

Result CompressStoredConversationIfNeeded(AgentContext& ctx, Json* context) {
	if (context == nullptr || ctx.store == nullptr || !NeedsCompression(*context)) {
		return Result::Ok();
	}

	const Json& turns = context->at("recentTurns");
	if (turns.size() <= kKeepRecentTurns) {
		return Result::Ok();
	}

	const std::size_t compressCount = turns.size() - kKeepRecentTurns;
	Json toCompress = Json::array();
	for (std::size_t i = 0; i < compressCount; ++i) {
		toCompress.push_back(turns[i]);
	}

	const std::string existingSummary = context->value("summary", std::string());
	const PromptPair prompt = prompts::CompressConversationMemory(existingSummary, toCompress);
	Json raw;
	Result callResult = CallLlmJson(ctx, prompt, &raw);
	if (!callResult || !raw.is_object() || !raw.contains("summary") ||
	    !raw.at("summary").is_string() || raw.at("summary").get<std::string>().empty()) {
		*context = LatestTurnsOnly(*context);
		return callResult
			? Result::Fail("IntakeAgent: conversation compression returned no summary")
			: callResult;
	}

	const Json& lastCompressed = toCompress.back();
	if (!lastCompressed.is_object() || !lastCompressed.contains("sessionId") ||
	    !lastCompressed.at("sessionId").is_number_integer()) {
		*context = LatestTurnsOnly(*context);
		return Result::Fail("IntakeAgent: compressed turn has no sessionId");
	}

	const SessionId through = lastCompressed.at("sessionId").get<SessionId>();
	Result updateResult = ctx.store->UpdateConversationSummary(
		raw.at("summary").get<std::string>(), through);
	if (!updateResult) {
		*context = LatestTurnsOnly(*context);
		return updateResult;
	}

	*context = ctx.store->GetConversationContext(ctx.sessionId);
	return Result::Ok();
}

} // namespace

Result IntakeAgent::Run(
	AgentContext& ctx,
	const std::string& userRequest,
	const Json& conversationContext,
	Json* intakeOut) {

	prompts::ClearCurrentConversationRequestContext();

	if (intakeOut == nullptr) {
		return Result::Fail("IntakeAgent: intakeOut is null");
	}

	Json effectiveContext = conversationContext;
	if ((!effectiveContext.is_object() || effectiveContext.empty()) &&
	    ctx.store != nullptr && ctx.sessionId != kInvalidId) {
		effectiveContext = ctx.store->GetConversationContext(ctx.sessionId);
		(void)CompressStoredConversationIfNeeded(ctx, &effectiveContext);
	}

	const PromptPair prompt = prompts::Intake(userRequest, effectiveContext);

	Json raw;
	Result callResult = CallLlmJson(ctx, prompt, &raw);
	if (!callResult) {
		return callResult;
	}

	if (!raw.is_object() || !raw.contains("goal") || !raw.at("goal").is_string() ||
	    raw.at("goal").get<std::string>().empty()) {
		return Result::Fail("IntakeAgent: response missing non-empty string 'goal'");
	}

	Json normalized = Json::object();
	const std::string goal = raw.at("goal").get<std::string>();
	normalized["goal"] = goal;

	std::string resolvedRequest = goal;
	if (raw.contains("resolvedRequest") && raw.at("resolvedRequest").is_string() &&
	    !raw.at("resolvedRequest").get<std::string>().empty()) {
		resolvedRequest = raw.at("resolvedRequest").get<std::string>();
	}
	normalized["resolvedRequest"] = resolvedRequest;
	normalized["turnRelation"] = NormalizeRelation(raw);

	NormalizeStringArray(raw, "symptoms", &normalized);
	NormalizeStringArray(raw, "constraints", &normalized);
	NormalizeStringArray(raw, "requiredCapabilities", &normalized);
	NormalizeStringArray(raw, "unresolvedReferences", &normalized);

	std::string requestType = "investigation";
	if (raw.contains("requestType") && raw.at("requestType").is_string()) {
		const std::string rawType = raw.at("requestType").get<std::string>();
		if (rawType == "conversation" || rawType == "investigation") {
			requestType = rawType;
		}
	}
	normalized["requestType"] = requestType;

	if (effectiveContext.is_object() && !effectiveContext.empty()) {
		normalized["conversationContext"] = effectiveContext;
	}

	prompts::SetCurrentConversationRequestContext(effectiveContext, normalized);
	*intakeOut = std::move(normalized);
	return Result::Ok();
}

} // namespace agentos
