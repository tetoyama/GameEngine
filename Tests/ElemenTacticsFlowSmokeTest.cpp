#include "Game/ElemenTactics/Flow/MatchFlowModel.h"

#include <algorithm>
#include <cassert>
#include <iostream>

using namespace ElemenTactics;

namespace {

void TestDeckEditor(){
	DeckSetupModel deck = DeckSetupModel::ConcentratedDefault();
	std::string error;
	assert(deck.Validate(&error));
	assert(deck.Decks()[0].size() == 8 && deck.Decks()[1].empty());
	const ElementType moved = deck.Decks()[0][0];
	assert(deck.MoveCard(0, 0, 1, 0, &error));
	assert(deck.Decks()[0].size() == 7 && deck.Decks()[1].size() == 1);
	assert(deck.Decks()[1][0] == moved && deck.Validate(&error));
	assert(deck.ReorderCard(0, 0, deck.Decks()[0].size(), &error));
	assert(!deck.MoveCard(3, 0, 0, 0, &error));
}

void TestSinglePlayerFlow(){
	MatchFlowModel flow;
	assert(flow.Screen() == FlowScreen::Title);
	assert(flow.OpenModeSelect());
	assert(flow.SelectMode(GameMode::HumanVsLlm));
	assert(flow.Screen() == FlowScreen::DeckSetupPlayerOne);
	flow.EditingDeck() = DeckSetupModel::ConcentratedDefault();
	std::string error;
	assert(flow.ConfirmCurrentDeck(&error));
	assert(flow.Screen() == FlowScreen::MatchIntroduction);
	assert(flow.BeginMatch(PlayerId::One, &error));
	assert(flow.Screen() == FlowScreen::BattleBoard && flow.Match());
	const PublicGameView playerView = flow.BuildView(PlayerId::One);
	assert(playerView.pieces[0][0].visibleDeck.size() == 8);
	assert(playerView.pieces[1][0].visibleDeck.empty());
	flow.ReturnToTitle();
	assert(flow.Screen() == FlowScreen::Title && !flow.Match());
}

void TestLocalPrivacyHandoff(){
	MatchFlowModel flow;
	assert(flow.OpenModeSelect());
	assert(flow.SelectMode(GameMode::LocalHumanVsHuman));
	flow.EditingDeck() = DeckSetupModel::ConcentratedDefault();
	std::string error;
	assert(flow.ConfirmCurrentDeck(&error));
	assert(flow.Screen() == FlowScreen::LocalPrivacyHandoff);
	assert(flow.ConfirmPrivacyHandoff());
	assert(flow.Screen() == FlowScreen::DeckSetupPlayerTwo);
	assert(flow.EditingDeck().Decks()[0].size() == 3);
	assert(flow.ConfirmCurrentDeck(&error));
	assert(flow.BeginMatch(PlayerId::Two, &error));
	assert(flow.Match()->currentPlayer == PlayerId::Two);
}

void TestAiStepReevaluatesAndFallsBack(){
	MatchFlowModel flow;
	assert(flow.OpenModeSelect());
	assert(flow.SelectMode(GameMode::HumanVsLlm));
	std::string error;
	assert(flow.ConfirmCurrentDeck(&error));
	assert(flow.BeginMatch(PlayerId::Two, &error));
	GameState* state = flow.MutableMatch();
	assert(state);
	const std::uint64_t serialBefore = state->actionSerial;
	AiStepResult first = AiTurnCoordinator::ExecuteNextStep(
		*state, PlayerId::Two, 10,
		std::string{"{\"action_type\":\"move_or_battle\",\"own_piece\":0,\"target_cell\":999}"});
	assert(first.applied && first.usedFallback && !first.usedLlm);
	assert(!first.fallbackReason.empty() && first.error.empty());
	assert(state->actionSerial == serialBefore + 1 && state->actionsRemaining == 1);
	const std::uint64_t afterFirst = state->actionSerial;
	AiStepResult second = AiTurnCoordinator::ExecuteNextStep(*state, PlayerId::Two, 11);
	assert(second.applied && state->actionSerial == afterFirst + 1);
	assert(state->currentPlayer == PlayerId::One || state->result.finished || state->pendingReorder);
}

void TestFlowMirrorsReorderAndResult(){
	MatchFlowModel flow;
	assert(flow.OpenModeSelect());
	assert(flow.SelectMode(GameMode::HumanVsLlm));
	std::string error;
	assert(flow.ConfirmCurrentDeck(&error));
	assert(flow.BeginMatch(PlayerId::One, &error));
	GameState* state = flow.MutableMatch();
	const PieceId one{PlayerId::One, 0};
	assert(ElemenTacticsRules::ApplyAction(*state, GameAction::MoveOrBattle(one, CenterCellId)).applied);
	assert(flow.NotifyRuleStateChanged());
	assert(flow.Screen() == FlowScreen::CenterReorder);
	assert(ElemenTacticsRules::ResolvePendingReorder(*state, std::nullopt).applied);
	assert(flow.NotifyRuleStateChanged());
	assert(flow.Screen() == FlowScreen::BattleBoard);
	state->result.finished = true;
	state->result.winner = PlayerId::One;
	assert(flow.NotifyRuleStateChanged() && flow.Screen() == FlowScreen::Result);
	assert(flow.Retry(&error));
	assert(flow.Screen() == FlowScreen::BattleBoard && !flow.Match()->result.finished);
}

} // namespace

int main(){
	TestDeckEditor();
	TestSinglePlayerFlow();
	TestLocalPrivacyHandoff();
	TestAiStepReevaluatesAndFallsBack();
	TestFlowMirrorsReorderAndResult();
	std::cout << "ElemenTactics flow smoke tests passed\n";
	return 0;
}
