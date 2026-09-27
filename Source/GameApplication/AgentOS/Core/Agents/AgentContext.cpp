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

// 定数初期化のみ。動的初期化を伴うthread_localは置かないこと（AgentContext.h参照）。
namespace {
thread_local SessionId g_currentSessionId = 0;
}

void SetCurrentSessionId(SessionId sessionId) noexcept { g_currentSessionId = sessionId; }
SessionId CurrentSessionId() noexcept { return g_currentSessionId; }

namespace {

const char* kFenceReminder = "\n\n出力は```jsonフェンス内の単一JSONオブジェクトのみ。";

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
	*out = std::move(retryExtracted);
	return Result::Ok();
}

} // namespace agentos
