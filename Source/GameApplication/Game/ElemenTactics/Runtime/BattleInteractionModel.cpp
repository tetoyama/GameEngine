#include "BattleInteractionModel.h"

namespace ElemenTactics {

void BattleInteractionModel::SetMode(BattleInputMode mode) noexcept{
	m_mode = mode;
}

BattleInteractionResult BattleInteractionModel::SelectPiece(
	const GameState& state,
	PieceId piece){
	if(state.result.finished) return Reject("the match is already finished");
	if(state.pendingReorder) return Reject("center reorder blocks board input");
	if(piece.owner != state.currentPlayer) return Reject("only the active player's piece can be selected");
	const PieceState* selected = ElemenTacticsRules::FindPiece(state, piece);
	if(!selected || !selected->IsAlive()) return Reject("selected piece is not alive on the board");
	m_selectedPiece = piece;
	return BattleInteractionResult{
		InteractionResultType::SelectionChanged,
		m_selectedPiece,
		std::nullopt,
		{}
	};
}

BattleInteractionResult BattleInteractionModel::HandleCell(
	const GameState& state,
	int cellId){
	if(state.result.finished) return Reject("the match is already finished");
	if(state.pendingReorder) return Reject("center reorder blocks board input");
	if(!IsBoardCell(cellId)) return Reject("cell is outside the board");

	const auto& occupant = state.occupancy[static_cast<std::size_t>(cellId)];
	if(occupant && occupant->owner == state.currentPlayer){
		return SelectPiece(state, *occupant);
	}
	if(!m_selectedPiece) return Reject("select an active piece first");

	if(m_mode == BattleInputMode::MoveOrBattle){
		return Ready(state, GameAction::MoveOrBattle(*m_selectedPiece, cellId));
	}
	if(!occupant || occupant->owner == state.currentPlayer){
		return Reject("scout requires an enemy piece");
	}
	return Ready(state, GameAction::Scout(*m_selectedPiece, *occupant));
}

BattleInteractionResult BattleInteractionModel::HandleScreenPoint(
	const GameState& state,
	const BoardLayout& layout,
	ScreenPoint point){
	const std::optional<int> cell = layout.HitTestCell(point);
	if(!cell) return Reject("pointer is outside the board");
	return HandleCell(state, *cell);
}

BattleInteractionResult BattleInteractionModel::Cancel() noexcept{
	m_selectedPiece.reset();
	m_mode = BattleInputMode::MoveOrBattle;
	return BattleInteractionResult{
		InteractionResultType::Cancelled,
		std::nullopt,
		std::nullopt,
		{}
	};
}

BattleInteractionResult BattleInteractionModel::Reject(std::string message) const{
	return BattleInteractionResult{
		InteractionResultType::Rejected,
		m_selectedPiece,
		std::nullopt,
		std::move(message)
	};
}

BattleInteractionResult BattleInteractionModel::Ready(
	const GameState& state,
	GameAction action) const{
	std::string error;
	if(!ElemenTacticsRules::IsLegalAction(state, action, &error)){
		return Reject(std::move(error));
	}
	return BattleInteractionResult{
		InteractionResultType::ActionReady,
		m_selectedPiece,
		std::move(action),
		{}
	};
}

} // namespace ElemenTactics
