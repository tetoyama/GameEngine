// =======================================================================
//
// AgentContext.cpp
//
// =======================================================================
#include "AgentContext.h"

#include <cstdint>
#include <string>
#include <utility>

#include "../Llm/JsonExtractor.h"

namespace agentos {

namespace {

const char* kFenceReminder = "\n\n出力は```jsonフェンス内の単一JSONオブジェクトのみ。";

bool ReplyContainsHistoryOnlyIdentifier(const std::string& reply) {
	if (prompts::CurrentTurnRelation() != "new" || reply.empty()) return false;

	const Json current = prompts::CurrentConversationRequestContext();
	std::string activeText = prompts::CurrentResolvedRequest();
	if (current.is_object() && current.contains("intake") && current.at("intake").is_object()) {
		activeText += "\n" + current.at("intake").value("currentUserInput", std::string());
	}

	const Json identifiers = prompts::CurrentHistoryIdentifiers();
	if (!identifiers.is_array()) return false;
	for (const Json& value : identifiers) {
		if (!value.is_string()) continue;
		const std::string identifier = value.get<std::string>();
		if (identifier.size() < 3) continue;
		if (reply.find(identifier) != std::string::npos &&
		    activeText.find(identifier) == std::string::npos) {
			return true;
		}
	}
	return false;
}

// DirectReplyのTool誤提案と、new Turnへの過去トピック混入をJSON抽出直後に遮断する。
void SanitizeConversationResult(Json* value) {
	if (value == nullptr || !value->is_object()) return;

	const bool directReplyShape =
		value->contains("reply") || value->contains("toolCall") || value->contains("escalate");
	if (!directReplyShape) return;

	if (prompts::CurrentRequestIsPersonalIdentityQuestion()) {
		(*value)["toolCall"] = nullptr;
		(*value)["escalate"] = false;
	}

	if (value->contains("reply") && value->at("reply").is_string()) {
		const std::string reply = value->at("reply").get<std::string>();
		if (ReplyContainsHistoryOnlyIdentifier(reply)) {
			(*value)["reply"] = prompts::CurrentRequestIsSimpleConversation()
				? Json("こんにちは。何を確認する？")
				: Json("今回の話題について、もう少し具体的に教えて。");
			(*value)["toolCall"] = nullptr;
			(*value)["escalate"] = false;
		}
	}
}

} // namespace

Result CallLlmJson(AgentContext& ctx, const PromptPair& prompt, Json* out) {
	if (out == nullptr) return Result::Fail("CallLlmJson: out is null");
	if (ctx.llm == nullptr) return Result::Fail("CallLlmJson: llm backend is not set");

	auto generateAndConsume = [&](const std::string& userPrompt, std::string* responseOut) -> Result {
		LlmGenerationStats stats;
		*responseOut = ctx.llm->Generate(prompt.system, userPrompt, &stats);

		if (ctx.budget != nullptr) {
			std::int64_t chars = stats.promptChars + stats.completionChars;
			if (chars <= 0) {
				chars = static_cast<std::int64_t>(
					prompt.system.size() + userPrompt.size() + responseOut->size());
			}
			return ctx.budget->ConsumeLlmCall(chars);
		}
		return Result::Ok();
	};

	std::string response;
	Result budgetResult = generateAndConsume(prompt.user, &response);
	if (!budgetResult) return budgetResult;

	Json extracted;
	Result extractResult = JsonExtractor::Extract(response, &extracted);
	if (extractResult) {
		SanitizeConversationResult(&extracted);
		*out = std::move(extracted);
		return Result::Ok();
	}

	const std::string retryUser = prompt.user + kFenceReminder;
	std::string retryResponse;
	Result retryBudgetResult = generateAndConsume(retryUser, &retryResponse);
	if (!retryBudgetResult) return retryBudgetResult;

	Json retryExtracted;
	Result retryExtractResult = JsonExtractor::Extract(retryResponse, &retryExtracted);
	if (!retryExtractResult) {
		return Result::Fail(
			"CallLlmJson: JSON extraction failed after retry: " + retryExtractResult.error);
	}
	SanitizeConversationResult(&retryExtracted);
	*out = std::move(retryExtracted);
	return Result::Ok();
}

} // namespace agentos
