// =======================================================================
//
// AgentContext.cpp
//
// =======================================================================
#include "AgentContext.h"

#include <cstdint>
#include <utility>

#include "../Llm/JsonExtractor.h"

namespace agentos {

namespace {

const char* kFenceReminder = "\n\n出力は```jsonフェンス内の単一JSONオブジェクトのみ。";

// DirectReplyが会話上の人物質問に対してEngine Toolを誤提案した場合、
// Orchestratorへ渡す前にTool側だけを無効化する。reply文字列は保持するため、
// 「私は誰ですか」がTool失敗へ変換されず会話として返る。
void SanitizeConversationResult(Json* value) {
	if (value == nullptr || !value->is_object()) {
		return;
	}
	if (!prompts::CurrentRequestIsPersonalIdentityQuestion()) {
		return;
	}
	const bool directReplyShape =
		value->contains("reply") || value->contains("toolCall") || value->contains("escalate");
	if (!directReplyShape) {
		return;
	}
	(*value)["toolCall"] = nullptr;
	(*value)["escalate"] = false;
}

} // namespace

Result CallLlmJson(AgentContext& ctx, const PromptPair& prompt, Json* out) {
	if (out == nullptr) {
		return Result::Fail("CallLlmJson: out is null");
	}
	if (ctx.llm == nullptr) {
		return Result::Fail("CallLlmJson: llm backend is not set");
	}

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
	if (!budgetResult) {
		return budgetResult;
	}

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
	if (!retryBudgetResult) {
		return retryBudgetResult;
	}

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
