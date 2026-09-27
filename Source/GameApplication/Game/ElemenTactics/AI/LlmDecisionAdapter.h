#pragma once

#include "ElemenTacticsAi.h"

#include <optional>
#include <string>
#include <vector>

namespace ElemenTactics {

struct LlmDecision {
	GameAction action;
	PublicReasoning publicReasoning;
};

class LlmDecisionAdapter final {
public:
	static std::string BuildPrompt(
		const PublicGameView& view,
		const std::vector<GameAction>& legalActions);
	static std::optional<LlmDecision> ParseAndValidate(
		const PublicGameView& view,
		const std::vector<GameAction>& legalActions,
		const std::string& response,
		std::string* error = nullptr);
private:
	static std::string SanitizePublicText(std::string text, std::size_t maxLength);
};

} // namespace ElemenTactics
