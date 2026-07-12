#include "MatchFlowModel.h"

namespace ElemenTactics {

bool MatchFlowModel::OpenModeSelect(){
	if(m_screen != FlowScreen::Title) return false;
	m_screen = FlowScreen::ModeSelect;
	return true;
}

bool MatchFlowModel::OpenRules(){
	if(m_screen == FlowScreen::CenterReorder || m_screen == FlowScreen::BattleBoard) return false;
	m_rulesReturnScreen = m_screen;
	m_screen = FlowScreen::Rules;
	return true;
}

bool MatchFlowModel::ReturnFromRules(){
	if(m_screen != FlowScreen::Rules) return false;
	m_screen = m_rulesReturnScreen;
	return true;
}

bool MatchFlowModel::SelectMode(GameMode mode){
	if(m_screen != FlowScreen::ModeSelect) return false;
	m_mode = mode;
	m_playerOneDecks = {};
	m_playerTwoDecks = {};
	m_editingDeck = DeckSetupModel::BalancedDefault();
	m_screen = FlowScreen::DeckSetupPlayerOne;
	return true;
}

bool MatchFlowModel::ConfirmCurrentDeck(std::string* error){
	if(!m_mode) return false;
	if(!m_editingDeck.Validate(error)) return false;
	if(m_screen == FlowScreen::DeckSetupPlayerOne){
		m_playerOneDecks = m_editingDeck.Decks();
		if(*m_mode == GameMode::LocalHumanVsHuman){
			m_editingDeck = DeckSetupModel::BalancedDefault();
			m_screen = FlowScreen::LocalPrivacyHandoff;
		} else {
			m_playerTwoDecks = BuildCpuDecks();
			m_screen = FlowScreen::MatchIntroduction;
		}
		return true;
	}
	if(m_screen == FlowScreen::DeckSetupPlayerTwo){
		m_playerTwoDecks = m_editingDeck.Decks();
		m_screen = FlowScreen::MatchIntroduction;
		return true;
	}
	return false;
}

bool MatchFlowModel::ConfirmPrivacyHandoff(){
	if(m_screen != FlowScreen::LocalPrivacyHandoff) return false;
	m_screen = FlowScreen::DeckSetupPlayerTwo;
	return true;
}

bool MatchFlowModel::BeginMatch(PlayerId firstPlayer, std::string* error){
	if(m_screen != FlowScreen::MatchIntroduction || !m_mode) return false;
	const DeckSetup setup = BuildSetup();
	if(!ElemenTacticsRules::ValidateDeckSetup(setup, error)) return false;
	m_match = ElemenTacticsRules::CreateInitialState(setup, firstPlayer);
	m_lastFirstPlayer = firstPlayer;
	m_screen = FlowScreen::BattleBoard;
	return true;
}

bool MatchFlowModel::NotifyRuleStateChanged(){
	if(!m_match) return false;
	if(m_match->result.finished) m_screen = FlowScreen::Result;
	else if(m_match->pendingReorder) m_screen = FlowScreen::CenterReorder;
	else m_screen = FlowScreen::BattleBoard;
	return true;
}

bool MatchFlowModel::Retry(std::string* error){
	if(m_screen != FlowScreen::Result || !m_mode) return false;
	const DeckSetup setup = BuildSetup();
	if(!ElemenTacticsRules::ValidateDeckSetup(setup, error)) return false;
	m_match = ElemenTacticsRules::CreateInitialState(setup, m_lastFirstPlayer);
	m_screen = FlowScreen::BattleBoard;
	return true;
}

void MatchFlowModel::ReturnToTitle(){
	m_screen = FlowScreen::Title;
	m_rulesReturnScreen = FlowScreen::Title;
	m_mode.reset();
	m_match.reset();
	m_playerOneDecks = {};
	m_playerTwoDecks = {};
	m_editingDeck = DeckSetupModel::BalancedDefault();
}

PublicGameView MatchFlowModel::BuildView(PlayerId viewer) const{
	if(m_match) return ElemenTacticsRules::BuildPublicView(*m_match, viewer);
	PublicGameView view;
	view.viewer = viewer;
	return view;
}

DeckSetup MatchFlowModel::BuildSetup() const{
	DeckSetup setup;
	setup.decks[0] = m_playerOneDecks;
	setup.decks[1] = m_playerTwoDecks;
	return setup;
}

std::array<std::vector<ElementType>, 3> MatchFlowModel::BuildCpuDecks(){
	return DeckSetupModel::BalancedDefault().Decks();
}

AiStepResult AiTurnCoordinator::ExecuteNextStep(
	GameState& state,
	PlayerId aiPlayer,
	std::uint64_t seed,
	const std::optional<std::string>& llmResponse){
	AiStepResult result;
	if(state.result.finished){ result.error = "match is already finished"; return result; }
	if(state.currentPlayer != aiPlayer){ result.error = "it is not the AI player's turn"; return result; }

	PublicGameView view = ElemenTacticsRules::BuildPublicView(state, aiPlayer);
	if(state.pendingReorder){
		const PieceId piece = state.pendingReorder->piece;
		const std::vector<ElementType> order = HeuristicAi::ChooseCenterReorder(view, piece, seed);
		result.reorder = ElemenTacticsRules::ResolvePendingReorder(state, order);
		result.applied = result.reorder->applied;
		result.usedFallback = true;
		result.reasoning.currentGoal = "中央再編で次の先頭順を作る";
		result.reasoning.actionReason = "公開情報から期待値の高い順へ残存カードを並べ直す";
		result.reasoning.highlightedPiece = piece;
		result.reasoning.confidence = 0.6;
		if(!result.applied) result.error = result.reorder->error;
		return result;
	}

	const std::vector<GameAction> legal = ElemenTacticsRules::GenerateLegalActions(state);
	if(legal.empty()){ result.error = "no legal action exists"; return result; }
	std::optional<AiDecision> selected;
	if(llmResponse){
		std::string parseError;
		if(const auto llm = LlmDecisionAdapter::ParseAndValidate(view, legal, *llmResponse, &parseError)){
			selected = AiDecision{llm->action, 0.0, llm->publicReasoning};
			result.usedLlm = true;
		} else {
			result.fallbackReason = parseError;
			result.usedFallback = true;
		}
	}
	if(!selected){
		selected = HeuristicAi::ChooseAction(view, legal, seed);
		result.usedFallback = true;
	}
	if(!selected){
		if(result.error.empty()) result.error = "AI could not choose a legal action";
		return result;
	}

	std::string legalError;
	if(!ElemenTacticsRules::IsLegalAction(state, selected->action, &legalError)){
		result.error = legalError;
		return result;
	}
	result.action = ElemenTacticsRules::ApplyAction(state, selected->action);
	result.applied = result.action->applied;
	result.reasoning = selected->reasoning;
	if(!result.applied) result.error = result.action->error;
	return result;
}

} // namespace ElemenTactics
