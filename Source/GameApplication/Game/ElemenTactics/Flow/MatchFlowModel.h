#pragma once

#include "DeckSetupModel.h"
#include "../AI/LlmDecisionAdapter.h"

#include <cstdint>
#include <optional>
#include <string>

struct SceneContext;

namespace ElemenTactics {

class ElemenTacticsLlmFacade;

enum class GameMode : std::uint8_t {
	HumanVsLlm,
	LocalHumanVsHuman
};

enum class FlowScreen : std::uint8_t {
	Title,
	ModeSelect,
	Rules,
	DeckSetupPlayerOne,
	LocalPrivacyHandoff,
	DeckSetupPlayerTwo,
	MatchIntroduction,
	BattleBoard,
	CenterReorder,
	Result
};

class MatchFlowModel final {
public:
	FlowScreen Screen() const noexcept{ return m_screen; }
	std::optional<GameMode> Mode() const noexcept{ return m_mode; }
	const DeckSetupModel& EditingDeck() const noexcept{ return m_editingDeck; }
	DeckSetupModel& EditingDeck() noexcept{ return m_editingDeck; }
	const std::optional<GameState>& Match() const noexcept{ return m_match; }

	bool OpenModeSelect();
	bool OpenRules();
	bool ReturnFromRules();
	bool SelectMode(GameMode mode);
	bool ConfirmCurrentDeck(std::string* error = nullptr);
	bool CancelDeckSetup();
	bool ConfirmPrivacyHandoff();
	bool ReturnFromPrivacyHandoff();
	bool BeginMatch(PlayerId firstPlayer = PlayerId::One, std::string* error = nullptr);
	bool ReturnFromMatchIntroduction();
	bool NotifyRuleStateChanged();
	bool Retry(std::string* error = nullptr);
	void ReturnToTitle();

	GameState* MutableMatch() noexcept{ return m_match ? &*m_match : nullptr; }
	PublicGameView BuildView(PlayerId viewer) const;

private:
	DeckSetup BuildSetup() const;
	static std::array<std::vector<ElementType>, 3> BuildCpuDecks();

	FlowScreen m_screen = FlowScreen::Title;
	FlowScreen m_rulesReturnScreen = FlowScreen::Title;
	std::optional<GameMode> m_mode;
	DeckSetupModel m_editingDeck = DeckSetupModel::BalancedDefault();
	std::array<std::vector<ElementType>, 3> m_playerOneDecks;
	std::array<std::vector<ElementType>, 3> m_playerTwoDecks;
	std::optional<GameState> m_match;
	PlayerId m_lastFirstPlayer = PlayerId::One;
};

struct AiStepResult {
	bool applied = false;
	bool usedLlm = false;
	bool usedFallback = false;
	bool pending = false;
	std::string error;
	std::string fallbackReason;
	PublicReasoning reasoning;
	std::optional<ActionResult> action;
	std::optional<ReorderResult> reorder;
};

class AiTurnCoordinator final {
public:
	// Executes at most one rule mutation. Calling this again after success
	// rebuilds the public view and legal list from the resulting state.
	static AiStepResult ExecuteNextStep(
		GameState& state,
		PlayerId aiPlayer,
		std::uint64_t seed,
		const std::optional<std::string>& llmResponse = std::nullopt);

	// Runtime-only bridge. Pure tests continue to use ExecuteNextStep above.
	static AiStepResult ExecuteNextStepWithFacade(
		ElemenTacticsLlmFacade& facade,
		::SceneContext* sceneContext,
		float deltaTime,
		GameState& state,
		PlayerId aiPlayer,
		std::uint64_t seed);
};

} // namespace ElemenTactics
