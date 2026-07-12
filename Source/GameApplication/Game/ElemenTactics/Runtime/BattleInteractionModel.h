#pragma once

#include "BoardLayout.h"
#include "../Rules/ElemenTacticsRules.h"

#include <optional>
#include <string>

namespace ElemenTactics {

enum class BattleInputMode : std::uint8_t {
	MoveOrBattle,
	Scout
};

enum class InteractionResultType : std::uint8_t {
	None,
	SelectionChanged,
	ActionReady,
	Cancelled,
	Rejected
};

struct BattleInteractionResult {
	InteractionResultType type = InteractionResultType::None;
	std::optional<PieceId> selectedPiece;
	std::optional<GameAction> action;
	std::string message;
};

class BattleInteractionModel final {
public:
	std::optional<PieceId> SelectedPiece() const noexcept{ return m_selectedPiece; }
	BattleInputMode Mode() const noexcept{ return m_mode; }

	void SetMode(BattleInputMode mode) noexcept;
	void ClearSelection() noexcept{ m_selectedPiece.reset(); }

	BattleInteractionResult SelectPiece(
		const GameState& state,
		PieceId piece);
	BattleInteractionResult HandleCell(
		const GameState& state,
		int cellId);
	BattleInteractionResult HandleScreenPoint(
		const GameState& state,
		const BoardLayout& layout,
		ScreenPoint point);
	BattleInteractionResult Cancel() noexcept;

private:
	BattleInteractionResult Reject(std::string message) const;
	BattleInteractionResult Ready(
		const GameState& state,
		GameAction action) const;

	std::optional<PieceId> m_selectedPiece;
	BattleInputMode m_mode = BattleInputMode::MoveOrBattle;
};

} // namespace ElemenTactics
