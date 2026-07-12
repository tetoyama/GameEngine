#include "Game/ElemenTactics/AI/LlmDecisionAdapter.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <initializer_list>
#include <string>
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

DeckSetup MakeSetup(
	const std::array<std::vector<ElementType>, 3>& one,
	const std::array<std::vector<ElementType>, 3>& two){
	DeckSetup setup;
	setup.decks[0] = one;
	setup.decks[1] = two;
	return setup;
}

DeckSetup MakeSetup(
	std::initializer_list<std::vector<ElementType>> one,
	std::initializer_list<std::vector<ElementType>> two){
	assert(one.size() == 3 && two.size() == 3);
	std::array<std::vector<ElementType>, 3> oneArray;
	std::array<std::vector<ElementType>, 3> twoArray;
	std::copy(one.begin(), one.end(), oneArray.begin());
	std::copy(two.begin(), two.end(), twoArray.begin());
	return MakeSetup(oneArray, twoArray);
}

DeckSetup OnePieceEach(){
	return MakeSetup({StandardEight(), {}, {}}, {StandardEight(), {}, {}});
}

void ApplyOrFail(GameState& state, const GameAction& action){
	const ActionResult result = ElemenTacticsRules::ApplyAction(state, action);
	if(!result.applied) std::cerr << "Action failed: " << result.error << '\n';
	assert(result.applied);
}

void MoveOrFail(GameState& state, PieceId actor, int cell){
	ApplyOrFail(state, GameAction::MoveOrBattle(actor, cell));
}

void TestMatchupMatrix(){
	using O = BattleOutcome;
	const std::array<std::array<O, 5>, 5> expected{{
		std::array{O::Draw, O::DefenderWin, O::AttackerWin, O::AttackerWin, O::DefenderWin},
		std::array{O::AttackerWin, O::Draw, O::DefenderWin, O::AttackerWin, O::DefenderWin},
		std::array{O::DefenderWin, O::AttackerWin, O::Draw, O::AttackerWin, O::DefenderWin},
		std::array{O::DefenderWin, O::DefenderWin, O::DefenderWin, O::Draw, O::AttackerWin},
		std::array{O::AttackerWin, O::AttackerWin, O::AttackerWin, O::DefenderWin, O::Draw}
	}};
	for(std::size_t attacker = 0; attacker < 5; ++attacker){
		for(std::size_t defender = 0; defender < 5; ++defender){
			assert(ElemenTacticsRules::ResolveMatchup(
				static_cast<ElementType>(attacker),
				static_cast<ElementType>(defender)) == expected[attacker][defender]);
		}
	}
}

void TestDeckValidationAndZeroPiecePlacement(){
	const std::array<std::array<int, 3>, 5> distributions{{
		std::array{8, 0, 0}, std::array{7, 1, 0}, std::array{6, 1, 1},
		std::array{4, 4, 0}, std::array{3, 3, 2}
	}};
	for(const auto& distribution : distributions){
		const std::vector<ElementType> cards = StandardEight();
		std::array<std::vector<ElementType>, 3> decks;
		std::size_t cursor = 0;
		for(std::size_t slot = 0; slot < 3; ++slot){
			for(int count = 0; count < distribution[slot]; ++count) decks[slot].push_back(cards[cursor++]);
		}
		const DeckSetup setup = MakeSetup(decks, decks);
		std::string error;
		assert(ElemenTacticsRules::ValidateDeckSetup(setup, &error));
		const GameState state = ElemenTacticsRules::CreateInitialState(setup);
		for(std::size_t slot = 0; slot < 3; ++slot){
			const PieceState* piece = ElemenTacticsRules::FindPiece(
				state, PieceId{PlayerId::One, static_cast<std::uint8_t>(slot)});
			assert(piece && piece->cell.has_value() == (distribution[slot] > 0));
		}
	}
	DeckSetup invalid = OnePieceEach();
	invalid.decks[0][0].pop_back();
	std::string error;
	assert(!ElemenTacticsRules::ValidateDeckSetup(invalid, &error));
	invalid = OnePieceEach();
	invalid.decks[0][0][0] = ElementType::Light;
	assert(!ElemenTacticsRules::ValidateDeckSetup(invalid, &error));
}

void TestBattleRotationLossAndPosition(){
	const DeckSetup setup = MakeSetup(
		{{ElementType::Fire, ElementType::Water, ElementType::Wood, ElementType::Dark, ElementType::Light, ElementType::Fire, ElementType::Water, ElementType::Wood}, {}, {}},
		{{ElementType::Wood, ElementType::Water, ElementType::Fire, ElementType::Dark, ElementType::Light, ElementType::Wood, ElementType::Water, ElementType::Fire}, {}, {}});
	GameState state = ElemenTacticsRules::CreateInitialState(setup);
	const PieceId attacker{PlayerId::One, 0};
	const PieceId defender{PlayerId::Two, 0};
	const int attackerCell = *ElemenTacticsRules::FindPiece(state, attacker)->cell;
	const int defenderCell = *ElemenTacticsRules::FindPiece(state, defender)->cell;
	const ActionResult result = ElemenTacticsRules::ApplyAction(state, GameAction::MoveOrBattle(attacker, defenderCell));
	assert(result.applied && result.battle && result.battle->outcome == BattleOutcome::AttackerWin);
	const PieceState* attackerState = ElemenTacticsRules::FindPiece(state, attacker);
	const PieceState* defenderState = ElemenTacticsRules::FindPiece(state, defender);
	assert(attackerState->cell == attackerCell && defenderState->cell == defenderCell);
	assert(attackerState->deck.front() == ElementType::Water && attackerState->deck.back() == ElementType::Fire);
	assert(defenderState->deck.size() == 7 && defenderState->deck.front() == ElementType::Water);
}

void TestDrawRotatesBoth(){
	GameState state = ElemenTacticsRules::CreateInitialState(OnePieceEach());
	const PieceId one{PlayerId::One, 0};
	const PieceId two{PlayerId::Two, 0};
	const int target = *ElemenTacticsRules::FindPiece(state, two)->cell;
	const ActionResult result = ElemenTacticsRules::ApplyAction(state, GameAction::MoveOrBattle(one, target));
	assert(result.applied && result.battle->outcome == BattleOutcome::Draw);
	assert(ElemenTacticsRules::FindPiece(state, one)->deck.front() == ElementType::Fire);
	assert(ElemenTacticsRules::FindPiece(state, one)->deck.back() == ElementType::Fire);
	assert(ElemenTacticsRules::FindPiece(state, two)->deck.front() == ElementType::Fire);
	assert(ElemenTacticsRules::FindPiece(state, two)->deck.back() == ElementType::Fire);
}

void TestDefeatAndMovementRules(){
	const auto rest = std::vector<ElementType>{
		ElementType::Fire, ElementType::Water, ElementType::Water,
		ElementType::Wood, ElementType::Dark, ElementType::Light, ElementType::Fire};
	DeckSetup setup = MakeSetup(
		{{ElementType::Fire, ElementType::Water, ElementType::Wood, ElementType::Dark, ElementType::Light, ElementType::Fire, ElementType::Water, ElementType::Wood}, {}, {}},
		{{ElementType::Wood}, rest, {}});
	GameState state = ElemenTacticsRules::CreateInitialState(setup);
	const PieceId attacker{PlayerId::One, 0};
	const PieceId defender{PlayerId::Two, 0};
	const int target = *ElemenTacticsRules::FindPiece(state, defender)->cell;
	ActionResult result = ElemenTacticsRules::ApplyAction(state, GameAction::MoveOrBattle(attacker, target));
	assert(result.applied && result.battle->defeatedPiece == defender && result.battle->attackerAdvanced);
	assert(ElemenTacticsRules::FindPiece(state, defender)->cell == std::nullopt);
	assert(ElemenTacticsRules::FindPiece(state, attacker)->cell == target);

	setup = MakeSetup(
		{{ElementType::Fire}, {ElementType::Water, ElementType::Wood, ElementType::Dark, ElementType::Light, ElementType::Fire, ElementType::Water, ElementType::Wood}, {}},
		{{ElementType::Water, ElementType::Fire, ElementType::Wood, ElementType::Dark, ElementType::Light, ElementType::Fire, ElementType::Water, ElementType::Wood}, {}, {}});
	state = ElemenTacticsRules::CreateInitialState(setup);
	const int defenderCell = *ElemenTacticsRules::FindPiece(state, defender)->cell;
	result = ElemenTacticsRules::ApplyAction(state, GameAction::MoveOrBattle(attacker, defenderCell));
	assert(result.applied && result.battle->outcome == BattleOutcome::DefenderWin);
	assert(result.battle->defeatedPiece == attacker && !result.battle->attackerAdvanced);
	assert(ElemenTacticsRules::FindPiece(state, defender)->cell == defenderCell);
}

void TestCriticalLossEndsImmediately(){
	DeckSetup setup = MakeSetup(
		{{ElementType::Dark, ElementType::Fire, ElementType::Fire, ElementType::Water, ElementType::Water, ElementType::Wood, ElementType::Wood, ElementType::Light}, {}, {}},
		{{ElementType::Light, ElementType::Fire, ElementType::Fire, ElementType::Water, ElementType::Water, ElementType::Wood, ElementType::Wood, ElementType::Dark}, {}, {}});
	GameState state = ElemenTacticsRules::CreateInitialState(setup);
	const PieceId attacker{PlayerId::One, 0};
	const PieceId defender{PlayerId::Two, 0};
	ActionResult result = ElemenTacticsRules::ApplyAction(
		state, GameAction::MoveOrBattle(attacker, *ElemenTacticsRules::FindPiece(state, defender)->cell));
	assert(result.applied && state.result.finished && state.result.reason == GameEndReason::KingLost);
	assert(state.result.winner == PlayerId::One && state.actionsRemaining == 0);
	assert(ElemenTacticsRules::GenerateLegalActions(state).empty());

	setup = MakeSetup(
		{{ElementType::Fire, ElementType::Fire, ElementType::Water, ElementType::Water, ElementType::Wood, ElementType::Wood, ElementType::Dark, ElementType::Light}, {}, {}},
		{{ElementType::Dark, ElementType::Fire, ElementType::Fire, ElementType::Water, ElementType::Water, ElementType::Wood, ElementType::Wood, ElementType::Light}, {}, {}});
	state = ElemenTacticsRules::CreateInitialState(setup);
	result = ElemenTacticsRules::ApplyAction(
		state, GameAction::MoveOrBattle(attacker, *ElemenTacticsRules::FindPiece(state, defender)->cell));
	assert(result.applied && state.result.finished && state.result.reason == GameEndReason::AssassinLost);
}

void TestTwoActionsAndDoubleScout(){
	GameState state = ElemenTacticsRules::CreateInitialState(OnePieceEach());
	const PieceId one{PlayerId::One, 0};
	const PieceId two{PlayerId::Two, 0};
	MoveOrFail(state, one, 8);
	assert(state.currentPlayer == PlayerId::One && state.actionsRemaining == 1);
	MoveOrFail(state, one, 7);
	assert(state.currentPlayer == PlayerId::Two && state.actionsRemaining == 2);

	state = ElemenTacticsRules::CreateInitialState(OnePieceEach());
	const ActionResult first = ElemenTacticsRules::ApplyAction(state, GameAction::Scout(one, two));
	const ActionResult second = ElemenTacticsRules::ApplyAction(state, GameAction::Scout(one, two));
	assert(first.applied && second.applied && first.scout && second.scout);
	assert(state.currentPlayer == PlayerId::Two);
	assert(ElemenTacticsRules::FindPiece(state, one)->deck.size() == 8);
	assert(ElemenTacticsRules::FindPiece(state, two)->deck.size() == 8);
}

void CompleteTwoMoves(GameState& state, PlayerId player, int firstCell, int secondCell){
	const PieceId actor{player, 0};
	MoveOrFail(state, actor, firstCell);
	if(state.pendingReorder) assert(ElemenTacticsRules::ResolvePendingReorder(state, std::nullopt).applied);
	MoveOrFail(state, actor, secondCell);
	if(state.pendingReorder) assert(ElemenTacticsRules::ResolvePendingReorder(state, std::nullopt).applied);
}

void TestCenterReorderAndCooldown(){
	GameState state = ElemenTacticsRules::CreateInitialState(OnePieceEach());
	const PieceId one{PlayerId::One, 0};
	MoveOrFail(state, one, CenterCellId);
	assert(state.pendingReorder && state.actionsRemaining == 1);
	const std::vector<ElementType> reordered{
		ElementType::Dark, ElementType::Light, ElementType::Wood, ElementType::Wood,
		ElementType::Water, ElementType::Water, ElementType::Fire, ElementType::Fire};
	ReorderResult reorder = ElemenTacticsRules::ResolvePendingReorder(state, reordered);
	assert(reorder.applied && reorder.reordered && state.actionsRemaining == 1);
	assert(ElemenTacticsRules::FindPiece(state, one)->deck.front() == ElementType::Dark);
	MoveOrFail(state, one, 8);
	CompleteTwoMoves(state, PlayerId::Two, 11, 10);
	MoveOrFail(state, one, CenterCellId);
	assert(!state.pendingReorder);
	MoveOrFail(state, one, 8);
	CompleteTwoMoves(state, PlayerId::Two, 11, 10);
	MoveOrFail(state, one, CenterCellId);
	assert(state.pendingReorder);
	assert(ElemenTacticsRules::ResolvePendingReorder(state, std::nullopt).applied);

	state = ElemenTacticsRules::CreateInitialState(OnePieceEach());
	MoveOrFail(state, one, 8);
	MoveOrFail(state, one, CenterCellId);
	assert(state.pendingReorder && state.actionsRemaining == 0 && state.currentPlayer == PlayerId::One);
	std::vector<ElementType> invalid = StandardEight();
	invalid[0] = ElementType::Light;
	assert(!ElemenTacticsRules::ResolvePendingReorder(state, invalid).applied);
	const ReorderResult accepted = ElemenTacticsRules::ResolvePendingReorder(state, StandardEight());
	assert(accepted.applied && accepted.turnAdvanced);
	assert(state.currentPlayer == PlayerId::Two && state.actionsRemaining == 2);
}

void TestPrivacyAndAiValidation(){
	GameState state = ElemenTacticsRules::CreateInitialState(OnePieceEach());
	PublicGameView view = ElemenTacticsRules::BuildPublicView(state, PlayerId::One);
	assert(view.pieces[0][0].visibleDeck.size() == 8 && view.pieces[1][0].visibleDeck.empty());
	const PieceId one{PlayerId::One, 0};
	const PieceId two{PlayerId::Two, 0};
	ApplyOrFail(state, GameAction::Scout(one, two));
	view = ElemenTacticsRules::BuildPublicView(state, PlayerId::One);
	assert(view.pieces[1][0].visibleDeck.empty());
	assert(view.pieces[1][0].knowledge.lastRevealed == ElementType::Fire);

	state = ElemenTacticsRules::CreateInitialState(OnePieceEach());
	MoveOrFail(state, one, 8);
	MoveOrFail(state, one, 7);
	view = ElemenTacticsRules::BuildPublicView(state, PlayerId::Two);
	const std::vector<GameAction> legal = ElemenTacticsRules::GenerateLegalActions(state);
	const auto heuristic = HeuristicAi::ChooseAction(view, legal, 42);
	assert(heuristic && std::find(legal.begin(), legal.end(), heuristic->action) != legal.end());
	assert(view.pieces[0][0].visibleDeck.empty());

	const GameAction selected = legal.front();
	std::string valid;
	if(selected.type == ActionType::Scout){
		valid = "{\"action_type\":\"scout\",\"own_piece\":" + std::to_string(selected.actor.slot) +
			",\"target_piece\":" + std::to_string(selected.targetPiece->slot) +
			",\"public_intent\":\"inspect\",\"confidence\":0.64,\"public_reason\":\"public info only\"}";
	} else {
		valid = "{\"action_type\":\"move_or_battle\",\"own_piece\":" + std::to_string(selected.actor.slot) +
			",\"target_cell\":" + std::to_string(selected.targetCell) +
			",\"public_intent\":\"position\",\"confidence\":0.64,\"public_reason\":\"public info only\"}";
	}
	std::string error;
	const auto parsed = LlmDecisionAdapter::ParseAndValidate(view, legal, valid, &error);
	assert(parsed && parsed->action == selected);
	assert(!LlmDecisionAdapter::ParseAndValidate(
		view, legal, "{\"action_type\":\"move_or_battle\",\"own_piece\":0,\"target_cell\":999}", &error));
	const std::string prompt = LlmDecisionAdapter::BuildPrompt(view, legal);
	assert(prompt.find("OPPONENT PUBLIC PIECES") != std::string::npos);
	assert(prompt.find("own_order") != std::string::npos);
}

} // namespace

int main(){
	TestMatchupMatrix();
	TestDeckValidationAndZeroPiecePlacement();
	TestBattleRotationLossAndPosition();
	TestDrawRotatesBoth();
	TestDefeatAndMovementRules();
	TestCriticalLossEndsImmediately();
	TestTwoActionsAndDoubleScout();
	TestCenterReorderAndCooldown();
	TestPrivacyAndAiValidation();
	std::cout << "ElemenTactics rules smoke tests passed\n";
	return 0;
}
