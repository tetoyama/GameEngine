// =======================================================================
//
// Budget.cpp
//
// =======================================================================
#include "Budget.h"

#include <algorithm>

namespace agentos {

BudgetTracker::BudgetTracker(Budget budget) : budget_(budget) {}

Result BudgetTracker::ConsumeToolCall() {
	std::lock_guard<std::mutex> lock(mutex_);
	if (usedToolCalls_ >= budget_.maxToolCalls) {
		return Result::Fail("Budget exceeded: toolCalls");
	}
	++usedToolCalls_;
	return Result::Ok();
}

Result BudgetTracker::ConsumeLlmCall(std::int64_t chars) {
	std::lock_guard<std::mutex> lock(mutex_);
	if (usedLlmCalls_ >= budget_.maxLlmCalls) {
		return Result::Fail("Budget exceeded: llmCalls");
	}
	if (usedLlmChars_ + chars > budget_.maxLlmChars) {
		return Result::Fail("Budget exceeded: llmChars");
	}
	++usedLlmCalls_;
	usedLlmChars_ += chars;
	return Result::Ok();
}

Result BudgetTracker::ConsumeRetry() {
	std::lock_guard<std::mutex> lock(mutex_);
	if (usedRetries_ >= budget_.maxRetries) {
		return Result::Fail("Budget exceeded: retries");
	}
	++usedRetries_;
	return Result::Ok();
}

Result BudgetTracker::ConsumeModifiedFile() {
	std::lock_guard<std::mutex> lock(mutex_);
	if (usedModifiedFiles_ >= budget_.maxModifiedFiles) {
		return Result::Fail("Budget exceeded: modifiedFiles");
	}
	++usedModifiedFiles_;
	return Result::Ok();
}

Result BudgetTracker::CheckDepth(int depth) const {
	std::lock_guard<std::mutex> lock(mutex_);
	if (depth > budget_.maxDepth) {
		return Result::Fail("Budget exceeded: depth");
	}
	return Result::Ok();
}

void BudgetTracker::AddElapsedMillis(std::int64_t millis) {
	std::lock_guard<std::mutex> lock(mutex_);
	usedMillis_ += millis;
}

double BudgetTracker::RemainingRatio() const {
	std::lock_guard<std::mutex> lock(mutex_);

	auto ratio = [](std::int64_t used, std::int64_t limit) -> double {
		if (limit <= 0) {
			return 0.0;
		}
		const double raw = 1.0 - static_cast<double>(used) / static_cast<double>(limit);
		return std::clamp(raw, 0.0, 1.0);
	};

	const double toolRatio    = ratio(usedToolCalls_, budget_.maxToolCalls);
	const double llmCallRatio = ratio(usedLlmCalls_, budget_.maxLlmCalls);
	const double charRatio    = ratio(usedLlmChars_, budget_.maxLlmChars);
	const double millisRatio  = ratio(usedMillis_, budget_.maxMillis);

	return std::min({toolRatio, llmCallRatio, charRatio, millisRatio});
}

Json BudgetTracker::ToJson() const {
	std::lock_guard<std::mutex> lock(mutex_);

	Json j = Json::object();
	j["toolCalls"]     = Json{{"used", usedToolCalls_},    {"limit", budget_.maxToolCalls}};
	j["llmCalls"]      = Json{{"used", usedLlmCalls_},     {"limit", budget_.maxLlmCalls}};
	j["retries"]       = Json{{"used", usedRetries_},      {"limit", budget_.maxRetries}};
	j["modifiedFiles"] = Json{{"used", usedModifiedFiles_},{"limit", budget_.maxModifiedFiles}};
	j["llmChars"]      = Json{{"used", usedLlmChars_},     {"limit", budget_.maxLlmChars}};
	j["millis"]        = Json{{"used", usedMillis_},       {"limit", budget_.maxMillis}};
	return j;
}

} // namespace agentos
