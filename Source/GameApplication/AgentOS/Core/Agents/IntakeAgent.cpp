// =======================================================================
//
// IntakeAgent.cpp
//
// =======================================================================
#include "IntakeAgent.h"

namespace agentos {

namespace {

void NormalizeStringArray(const Json& raw, const char* key, Json* normalized) {
	Json arr = Json::array();
	if (raw.contains(key) && raw.at(key).is_array()) {
		for (const auto& v : raw.at(key)) {
			if (v.is_string()) {
				arr.push_back(v.get<std::string>());
			}
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

} // namespace

Result IntakeAgent::Run(
	AgentContext& ctx,
	const std::string& userRequest,
	const Json& conversationContext,
	Json* intakeOut) {

	if (intakeOut == nullptr) {
		return Result::Fail("IntakeAgent: intakeOut is null");
	}

	const PromptPair prompt = prompts::Intake(userRequest, conversationContext);

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

	// 後続Agentが履歴全体を再解釈せずに済むよう、Intake時点の解決根拠を
	// compactな形で同梱する。生の全TurnはService側で保持される。
	if (conversationContext.is_object() && !conversationContext.empty()) {
		normalized["conversationContext"] = conversationContext;
	}

	*intakeOut = std::move(normalized);
	return Result::Ok();
}

} // namespace agentos
