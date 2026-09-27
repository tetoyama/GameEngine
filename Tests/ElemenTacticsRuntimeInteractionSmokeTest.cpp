#include "Game/ElemenTactics/Runtime/BattleInteractionModel.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace ElemenTactics;

namespace {

std::vector<ElementType> StandardEight(){
	return {
		ElementType::Fire, ElementType::Fire,
		ElementType::Water, ElementType::Water,
		ElementType::Wood, ElementType::Wood,
		ElementType::Dark, ElementType::Light
	};
}

DeckSetup MakeSetup(){
	DeckSetup setup;
	setup.decks[0][0] = StandardEight();
	setup.decks[1][0] = StandardEight();
	return setup;
}

void TestResponsiveLayout(){
	for(const auto [width, height] : std::vector<std::pair<float, float>>{{1280.0f, 720.0f}, {640.0f, 360.0f}}){
		const BoardLayout layout = BoardLayout::Create(width, height);
		assert(layout.HexRadius() >= 26.0f);
		for(const BoardCell& cell : BoardCells){
			const ScreenPoint center = layout.CellCenter(cell.id);
			assert(layout.ContainsPoint(cell.id, center));
			assert(layout.HitTestCell(center) == cell.id);
		}
		const ScreenRect board = layout.BoardBounds();
		const ScreenRect left = layout.LeftHudBounds();
		const ScreenRect right = layout.RightHudBounds();
		const ScreenRect footer = layout.FooterBounds();
		assert(board.x >= left.x + left.width - 1.0f);
		assert(board.x + board.width <= right.x + 1.0f);
		assert(board.y + board.height <= footer.y + 1.0f);
		assert(!layout.HitTestCell(ScreenPoint{-100.0f, -100.0f}));
	}
}

void TestSelectionAndMoveBattle(){
	GameState state = ElemenTacticsRules::CreateInitialState(MakeSetup());
	BattleInteractionModel interaction;
	const PieceId one{PlayerId::One, 0};
	const PieceId two{PlayerId::Two, 0};
	const int oneCell = *ElemenTacticsRules::FindPiece(state, one)->cell;
	const int twoCell = *ElemenTacticsRules::FindPiece(state, two)->cell;

	BattleInteractionResult result = interaction.HandleCell(state, oneCell);
	assert(result.type == InteractionResultType::SelectionChanged);
	assert(interaction.SelectedPiece() == one);

	result = interaction.HandleCell(state, 8);
	assert(result.type == InteractionResultType::ActionReady && result.action);
	assert(*result.action == GameAction::MoveOrBattle(one, 8));
	assert(state.actionSerial == 0 && state.actionsRemaining == 2);

	result = interaction.HandleCell(state, twoCell);
	assert(result.type == InteractionResultType::ActionReady && result.action);
	assert(*result.action == GameAction::MoveOrBattle(one, twoCell));
	assert(state.actionSerial == 0 && state.actionsRemaining == 2);
}

void TestScoutAndInvalidInput(){
	GameState state = ElemenTacticsRules::CreateInitialState(MakeSetup());
	BattleInteractionModel interaction;
	const PieceId one{PlayerId::One, 0};
	const PieceId two{PlayerId::Two, 0};
	const int oneCell = *ElemenTacticsRules::FindPiece(state, one)->cell;
	const int twoCell = *ElemenTacticsRules::FindPiece(state, two)->cell;
	const BoardLayout layout = BoardLayout::Create(1280.0f, 720.0f);

	BattleInteractionResult result = interaction.HandleScreenPoint(state, layout, ScreenPoint{-5.0f, -5.0f});
	assert(result.type == InteractionResultType::Rejected && !result.action);
	assert(state.actionSerial == 0 && state.actionsRemaining == 2);

	assert(interaction.HandleCell(state, oneCell).type == InteractionResultType::SelectionChanged);
	interaction.SetMode(BattleInputMode::Scout);
	result = interaction.HandleCell(state, 8);
	assert(result.type == InteractionResultType::Rejected && !result.action);
	assert(state.actionSerial == 0 && state.actionsRemaining == 2);

	result = interaction.HandleCell(state, twoCell);
	assert(result.type == InteractionResultType::ActionReady && result.action);
	assert(*result.action == GameAction::Scout(one, two));
	assert(state.actionSerial == 0 && state.actionsRemaining == 2);

	result = interaction.Cancel();
	assert(result.type == InteractionResultType::Cancelled);
	assert(!interaction.SelectedPiece() && interaction.Mode() == BattleInputMode::MoveOrBattle);
}

void TestFriendlySelectionAndModalBlock(){
	DeckSetup setup = MakeSetup();
	setup.decks[0][0] = {
		ElementType::Fire, ElementType::Fire,
		ElementType::Water, ElementType::Water
	};
	setup.decks[0][1] = {
		ElementType::Wood, ElementType::Wood,
		ElementType::Dark, ElementType::Light
	};
	GameState state = ElemenTacticsRules::CreateInitialState(setup);
	BattleInteractionModel interaction;
	const PieceId oneA{PlayerId::One, 0};
	const PieceId oneB{PlayerId::One, 1};
	const int oneACell = *ElemenTacticsRules::FindPiece(state, oneA)->cell;
	const int oneBCell = *ElemenTacticsRules::FindPiece(state, oneB)->cell;
	assert(interaction.HandleCell(state, oneACell).type == InteractionResultType::SelectionChanged);
	BattleInteractionResult result = interaction.HandleCell(state, oneBCell);
	assert(result.type == InteractionResultType::SelectionChanged);
	assert(interaction.SelectedPiece() == oneB && !result.action);

	state.pendingReorder = PendingReorder{oneB};
	result = interaction.HandleCell(state, CenterCellId);
	assert(result.type == InteractionResultType::Rejected && !result.action);
	assert(state.actionSerial == 0 && state.actionsRemaining == 2);

	state.pendingReorder.reset();
	state.result.finished = true;
	result = interaction.SelectPiece(state, oneA);
	assert(result.type == InteractionResultType::Rejected && !result.action);
}

} // namespace

int main(){
	TestResponsiveLayout();
	TestSelectionAndMoveBattle();
	TestScoutAndInvalidInput();
	TestFriendlySelectionAndModalBlock();
	std::cout << "ElemenTactics runtime interaction smoke tests passed\n";
	return 0;
}
