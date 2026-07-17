// =======================================================================
//
// MockLlmBackend.cpp
//
// =======================================================================
#include "MockLlmBackend.h"

namespace agentos {

void MockLlmBackend::EnqueueResponse(std::string response) {
	queue_.push_back(std::move(response));
}

void MockLlmBackend::AddRule(std::string promptSubstring, std::string response) {
	rules_.push_back(Rule{std::move(promptSubstring), std::move(response)});
}

std::string MockLlmBackend::Generate(
	const std::string& systemPrompt,
	const std::string& userPrompt,
	LlmGenerationStats* statsOut) {

	calls_.emplace_back(systemPrompt, userPrompt);

	if (statsOut != nullptr) {
		statsOut->promptChars = static_cast<std::int64_t>(systemPrompt.size() + userPrompt.size());
		statsOut->completionChars = 0;
		statsOut->elapsedMillis = 0;
	}

	const std::string combined = systemPrompt + userPrompt;
	for (const Rule& rule : rules_) {
		if (combined.find(rule.promptSubstring) != std::string::npos) {
			if (statsOut != nullptr) {
				statsOut->completionChars = static_cast<std::int64_t>(rule.response.size());
			}
			return rule.response;
		}
	}

	if (!queue_.empty()) {
		std::string response = std::move(queue_.front());
		queue_.erase(queue_.begin());
		if (statsOut != nullptr) {
			statsOut->completionChars = static_cast<std::int64_t>(response.size());
		}
		return response;
	}

	if (statsOut != nullptr) {
		statsOut->completionChars = 2;
	}
	return "{}";
}

std::vector<std::pair<std::string, std::string>> MockLlmBackend::GetCalls() const {
	return calls_;
}

} // namespace agentos
