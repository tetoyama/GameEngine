#pragma once

#include "../Core/ElemenTacticsCore.h"

#include <optional>
#include <string>
#include <vector>

namespace ElemenTactics {

class ElemenTacticsRules final {
public:
	static BattleOutcome ResolveMatchup(ElementType attacker, ElementType defender) noexcept;
	static bool ValidateDeckSetup(const DeckSetup& setup, std::string* error = nullptr);
	static GameState CreateInitialState(const DeckSetup& setup, PlayerId firstPlayer = PlayerId::One);
	static std::vector<GameAction> GenerateLegalActions(const GameState& state);
	static bool IsLegalAction(const GameState& state, const GameAction& action, std::string* error = nullptr);
	static ActionResult ApplyAction(GameState& state, const GameAction& action);
	// nullopt skips the optional reorder. A supplied order must preserve the exact remaining multiset.
	static ReorderResult ResolvePendingReorder(
		GameState& state,
		const std::optional<std::vector<ElementType>>& newOrder);
	static PublicGameView BuildPublicView(const GameState& state, PlayerId viewer);
	static bool ValidateInvariant(const GameState& state, std::string* error = nullptr);
	static const PieceState* FindPiece(const GameState& state, PieceId id) noexcept;
	static PieceState* FindPiece(GameState& state, PieceId id) noexcept;

private:
	static bool CanReorderOnArrival(const GameState& state, const PieceState& piece) noexcept;
	static void TryOpenCenterReorder(GameState& state, PieceState& piece);
	static void FinishAction(GameState& state, ActionResult& result);
	static void AdvanceTurn(GameState& state, bool* advanced = nullptr);
	static void RecordReveal(GameState& state, PieceId piece, ElementType element);
	static void RecordLoss(GameState& state, PieceId piece, ElementType element);
	static void RecordRotation(GameState& state, PieceId piece, ElementType element);
	static void MarkGameSet(
		GameState& state,
		PlayerId loser,
		ElementType lostElement,
		std::optional<PieceId> decisivePiece);
};

} // namespace ElemenTactics
