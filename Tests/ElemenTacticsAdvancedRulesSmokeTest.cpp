#include "Game/ElemenTactics/Rules/ElemenTacticsRules.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace ElemenTactics;

namespace {

DeckSetup BuildSplitDefenderSetup(){
	DeckSetup setup;
	setup.decks[PlayerIndex(PlayerId::One)][0] = {
		ElementType::Fire,
		ElementType::Water,
		ElementType::Light,
		ElementType::Dark,
		ElementType::Fire,
		ElementType::Water,
		ElementType::Wood,
		ElementType::Wood
	};
	setup.decks[PlayerIndex(PlayerId::Two)][0] = {ElementType::Wood};
	setup.decks[PlayerIndex(PlayerId::Two)][1] = {
		ElementType::Fire,
		ElementType::Fire,
		ElementType::Water,
		ElementType::Water,
		ElementType::Wood,
		ElementType::Dark,
		ElementType::Light
	};
	return setup;
}

ActionResult Apply(GameState& state, const GameAction& action){
	ActionResult result = ElemenTacticsRules::ApplyAction(state, action);
	if(!result.applied) std::cerr << result.error << '\n';
	assert(result.applied);
	return result;
}

void TestSamePieceCanBattleTwiceUsingRotatedFront(){
	GameState state = ElemenTacticsRules::CreateInitialState(BuildSplitDefenderSetup());
	const PieceId attacker{PlayerId::One, 0};
	const PieceId firstTarget{PlayerId::Two, 0};
	const PieceId secondTarget{PlayerId::Two, 1};
	const int firstTargetCell = *ElemenTacticsRules::FindPiece(state, firstTarget)->cell;
	const int secondTargetCell = *ElemenTacticsRules::FindPiece(state, secondTarget)->cell;

	const ActionResult first = Apply(
		state,
		GameAction::MoveOrBattle(attacker, firstTargetCell));
	assert(first.battle);
	assert(first.battle->attackerElement == ElementType::Fire);
	assert(first.battle->defenderElement == ElementType::Wood);
	assert(first.battle->outcome == BattleOutcome::AttackerWin);
	assert(first.battle->defeatedPiece == firstTarget);
	assert(first.battle->attackerAdvanced);
	assert(state.currentPlayer == PlayerId::One);
	assert(state.actionsRemaining == 1);
	assert(ElemenTacticsRules::FindPiece(state, attacker)->deck.front() == ElementType::Water);

	const ActionResult second = Apply(
		state,
		GameAction::MoveOrBattle(attacker, secondTargetCell));
	assert(second.battle);
	assert(second.battle->attackerElement == ElementType::Water);
	assert(second.battle->defenderElement == ElementType::Fire);
	assert(second.battle->outcome == BattleOutcome::AttackerWin);
	assert(!second.battle->defeatedPiece);
	assert(!second.battle->attackerAdvanced);
	assert(state.actionSerial == 2);
	assert(state.currentPlayer == PlayerId::Two);
	assert(state.actionsRemaining == 2);
	assert(ElemenTacticsRules::FindPiece(state, attacker)->cell == firstTargetCell);
	assert(ElemenTacticsRules::FindPiece(state, secondTarget)->cell == secondTargetCell);
}

void TestCenterCaptureTriggersOptionalReorderWithoutExtraAction(){
	GameState state = ElemenTacticsRules::CreateInitialState(BuildSplitDefenderSetup());
	const PieceId playerOne{PlayerId::One, 0};
	const PieceId centerDefender{PlayerId::Two, 0};
	const PieceId playerTwoOther{PlayerId::Two, 1};

	Apply(state, GameAction::MoveOrBattle(playerOne, 8));
	Apply(state, GameAction::MoveOrBattle(playerOne, 7));
	assert(state.currentPlayer == PlayerId::Two);

	Apply(state, GameAction::MoveOrBattle(centerDefender, CenterCellId));
	assert(state.pendingReorder);
	const ReorderResult defenderSkip = ElemenTacticsRules::ResolvePendingReorder(state, std::nullopt);
	assert(defenderSkip.applied && !defenderSkip.reordered);
	Apply(state, GameAction::MoveOrBattle(playerTwoOther, 11));
	assert(state.currentPlayer == PlayerId::One);

	const ActionResult capture = Apply(
		state,
		GameAction::MoveOrBattle(playerOne, CenterCellId));
	assert(capture.battle);
	assert(capture.battle->outcome == BattleOutcome::AttackerWin);
	assert(capture.battle->defeatedPiece == centerDefender);
	assert(capture.battle->attackerAdvanced);
	assert(ElemenTacticsRules::FindPiece(state, playerOne)->cell == CenterCellId);
	assert(state.pendingReorder && state.pendingReorder->piece == playerOne);
	assert(state.actionsRemaining == 1);

	const std::vector<ElementType> newOrder{
		ElementType::Dark,
		ElementType::Light,
		ElementType::Water,
		ElementType::Wood,
		ElementType::Wood,
		ElementType::Fire,
		ElementType::Water
	};
	const ReorderResult reordered = ElemenTacticsRules::ResolvePendingReorder(state, newOrder);
	assert(reordered.applied && reordered.reordered);
	assert(state.actionsRemaining == 1);
	assert(state.currentPlayer == PlayerId::One);
	assert(ElemenTacticsRules::FindPiece(state, playerOne)->deck == newOrder);
}

} // namespace

int main(){
	TestSamePieceCanBattleTwiceUsingRotatedFront();
	TestCenterCaptureTriggersOptionalReorderWithoutExtraAction();
	std::cout << "ElemenTactics advanced rules smoke tests passed\n";
	return 0;
}
