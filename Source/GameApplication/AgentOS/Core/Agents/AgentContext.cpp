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

// フェンス厳守を再度促すリマインダー。ローカル小型モデルは出力契約を
// 守れないことがあるため、リトライ時にのみ追記する。
const char* kFenceReminder = "\n\n出力は```jsonフェンス内の単一JSONオブジェクトのみ。";

} // namespace

Result CallLlmJson(AgentContext& ctx, const PromptPair& prompt, Json* out) {
	if (out == nullptr) {
		return Result::Fail("CallLlmJson: out is null");
	}
	if (ctx.llm == nullptr) {
		return Result::Fail("CallLlmJson: llm backend is not set");
	}

	// systemPrompt/userPromptでLLMを呼び、Budgetを（prompt+response文字数で）消費する。
	// Budget超過はリトライしても解消しないため、その場でResultを返す。
	auto generateAndConsume = [&](const std::string& userPrompt, std::string* responseOut) -> Result {
		LlmGenerationStats stats;
		*responseOut = ctx.llm->Generate(prompt.system, userPrompt, &stats);

		if (ctx.budget != nullptr) {
			std::int64_t chars = stats.promptChars + stats.completionChars;
			if (chars <= 0) {
				// バックエンドがstatsを埋めない場合の保険（文字数を実測する）。
				chars = static_cast<std::int64_t>(prompt.system.size() + userPrompt.size() + responseOut->size());
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
		*out = std::move(extracted);
		return Result::Ok();
	}

	// --- JSON抽出失敗 → リマインダーを追記して1回だけリトライ ---
	const std::string retryUser = prompt.user + kFenceReminder;
	std::string retryResponse;
	Result retryBudgetResult = generateAndConsume(retryUser, &retryResponse);
	if (!retryBudgetResult) {
		return retryBudgetResult;
	}

	Json retryExtracted;
	Result retryExtractResult = JsonExtractor::Extract(retryResponse, &retryExtracted);
	if (!retryExtractResult) {
		return Result::Fail("CallLlmJson: JSON extraction failed after retry: " + retryExtractResult.error);
	}
	*out = std::move(retryExtracted);
	return Result::Ok();
}

} // namespace agentos
