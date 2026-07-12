#pragma once

#include "../Rules/ElemenTacticsRules.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ElemenTactics {

struct PieceBelief {
	PieceId piece{};
	std::array<double, ElementIndex(ElementType::Count)> frontProbability{};
	double entropy = 0.0;
};

class BeliefState final {
public:
	static BeliefState Build(const PublicGameView& view);
	const PieceBelief* Find(PieceId piece) const noexcept;
	const std::vector<PieceBelief>& Pieces() const noexcept{ return m_pieces; }
private:
	std::vector<PieceBelief> m_pieces;
};

struct PublicReasoning {
	std::string currentGoal;
	std::string warningPiece;
	std::string lightCandidate;
	std::string darkCandidate;
	std::string actionReason;
	double confidence = 0.0;
	std::optional<PieceId> highlightedPiece;
};

struct AiDecision {
	GameAction action;
	double score = 0.0;
	PublicReasoning reasoning;
};

class HeuristicAi final {
public:
	static std::optional<AiDecision> ChooseAction(
		const PublicGameView& view,
		const std::vector<GameAction>& legalActions,
		std::uint64_t seed);
	static std::vector<ElementType> ChooseCenterReorder(
		const PublicGameView& view,
		PieceId piece,
		std::uint64_t seed);
private:
	static double ScoreAction(
		const PublicGameView& view,
		const BeliefState& belief,
		const GameAction& action,
		std::uint64_t seed);
};

} // namespace ElemenTactics
