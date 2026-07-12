#pragma once

#include "Component/CustomScriptComponent.h"
#include "Reference/EntityRef.h"
#include "../AI/ElemenTacticsLlmFacade.h"
#include "../Flow/MatchFlowModel.h"
#include "BattleInteractionModel.h"
#include "BoardLayout.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class RuntimeTextSystem;

namespace ElemenTactics {

class ElemenTacticsKeyboardNavigator;
class ElemenTacticsVisualGuide;

// ComponentRegistry constructs a temporary component and moves it into its
// selected storage. ElemenTacticsLlmFacade is intentionally non-copyable, so
// keep it behind a unique owner that can be transferred during ECS insertion.
// Runtime code retains the existing dot-call surface used by the controller.
class ElemenTacticsLlmOwner final {
public:
	ElemenTacticsLlmOwner()
		: m_value(std::make_unique<ElemenTacticsLlmFacade>()){}

	ElemenTacticsLlmOwner(const ElemenTacticsLlmOwner&) = delete;
	ElemenTacticsLlmOwner& operator=(const ElemenTacticsLlmOwner&) = delete;
	ElemenTacticsLlmOwner(ElemenTacticsLlmOwner&&) noexcept = default;
	ElemenTacticsLlmOwner& operator=(ElemenTacticsLlmOwner&&) noexcept = default;

	void Shutdown() noexcept{
		if(m_value) m_value->Shutdown();
	}

	bool ResetForNewMatch(std::string* error = nullptr){
		if(!m_value){
			if(error) *error = "ElemenTactics LLM owner has been moved";
			return false;
		}
		return m_value->ResetForNewMatch(error);
	}

	LlmDecisionPollResult Cancel(){
		return m_value ? m_value->Cancel() : LlmDecisionPollResult{};
	}

	ElemenTacticsLlmFacade& Get(){
		return *m_value;
	}

	const ElemenTacticsLlmFacade& Get() const{
		return *m_value;
	}

	operator ElemenTacticsLlmFacade&(){
		return Get();
	}

private:
	std::unique_ptr<ElemenTacticsLlmFacade> m_value;
};

class ElemenTacticsGameController final : public CustomScriptComponent {
public:
	ElemenTacticsGameController();
	~ElemenTacticsGameController() override;

	ElemenTacticsGameController(const ElemenTacticsGameController&) = delete;
	ElemenTacticsGameController& operator=(const ElemenTacticsGameController&) = delete;

	// SparseStorage::Add receives components by value and then moves them into
	// the node. Explicit transfer keeps runtime ownership unique and prevents
	// the moved-from temporary from executing OnStop in its destructor.
	ElemenTacticsGameController(ElemenTacticsGameController&& other) noexcept
		: CustomScriptComponent(),
		  ScriptName(scriptName),
		  m_flow(std::move(other.m_flow)),
		  m_interaction(std::move(other.m_interaction)),
		  m_llm(std::move(other.m_llm)),
		  m_textSystem(std::move(other.m_textSystem)),
		  m_uiLifetime(std::move(other.m_uiLifetime)),
		  m_buttons(std::move(other.m_buttons)),
		  m_boardLayout(std::move(other.m_boardLayout)),
		  m_selectedDeckCard(std::move(other.m_selectedDeckCard)),
		  m_reorderOrder(std::move(other.m_reorderOrder)),
		  m_selectedReorderCard(std::move(other.m_selectedReorderCard)),
		  m_reorderPiece(std::move(other.m_reorderPiece)),
		  m_status(std::move(other.m_status)),
		  m_lastAiReasoning(std::move(other.m_lastAiReasoning)),
		  m_screenDirty(other.m_screenDirty),
		  m_localTurnHandoff(other.m_localTurnHandoff),
		  m_started(other.m_started),
		  m_aiDelay(other.m_aiDelay),
		  m_cachedViewWidth(other.m_cachedViewWidth),
		  m_cachedViewHeight(other.m_cachedViewHeight){
		scriptName = std::move(other.scriptName);
		executionSettings = other.executionSettings;
		isInitialized = other.isInitialized;
		m_ref = other.m_ref;
		other.isInitialized = false;
		other.m_ref = {};
		other.m_started = false;
	}

	ElemenTacticsGameController& operator=(ElemenTacticsGameController&& other) noexcept{
		if(this == &other) return *this;
		if(m_started) OnStop();

		scriptName = std::move(other.scriptName);
		executionSettings = other.executionSettings;
		isInitialized = other.isInitialized;
		m_ref = other.m_ref;
		m_flow = std::move(other.m_flow);
		m_interaction = std::move(other.m_interaction);
		m_llm = std::move(other.m_llm);
		m_textSystem = std::move(other.m_textSystem);
		m_uiLifetime = std::move(other.m_uiLifetime);
		m_buttons = std::move(other.m_buttons);
		m_boardLayout = std::move(other.m_boardLayout);
		m_selectedDeckCard = std::move(other.m_selectedDeckCard);
		m_reorderOrder = std::move(other.m_reorderOrder);
		m_selectedReorderCard = std::move(other.m_selectedReorderCard);
		m_reorderPiece = std::move(other.m_reorderPiece);
		m_status = std::move(other.m_status);
		m_lastAiReasoning = std::move(other.m_lastAiReasoning);
		m_screenDirty = other.m_screenDirty;
		m_localTurnHandoff = other.m_localTurnHandoff;
		m_started = other.m_started;
		m_aiDelay = other.m_aiDelay;
		m_cachedViewWidth = other.m_cachedViewWidth;
		m_cachedViewHeight = other.m_cachedViewHeight;

		other.isInitialized = false;
		other.m_ref = {};
		other.m_started = false;
		return *this;
	}

	void OnStart() override;
	void OnUpdate(float dt) override;
	void OnStop() override;

	YAML::Node encode() override;
	bool decode(SceneContext* context, const YAML::Node& node) override;
	void inspector(SceneContext* context) override;

private:
	friend class ElemenTacticsKeyboardNavigator;
	friend class ElemenTacticsVisualGuide;

	enum class UiCommand : std::uint8_t {
		None,
		TitleStart,
		ModeLlm,
		ModeLocal,
		OpenRules,
		BackFromRules,
		BackToTitle,
		DeckCard,
		DeckMoveLeft,
		DeckMoveRight,
		DeckMoveUp,
		DeckMoveDown,
		DeckMoveToSlot,
		DeckMoveTop,
		DeckMoveBottom,
		DeckClearSelection,
		DeckBalanced,
		DeckConcentrated,
		DeckConfirm,
		PrivacyContinue,
		MatchBegin,
		BoardCell,
		BattleMoveMode,
		BattleScoutMode,
		BattleCancel,
		ReorderCard,
		ReorderLeft,
		ReorderRight,
		ReorderConfirm,
		ReorderSkip,
		ResultRetry,
		ResultTitle
	};

	struct UiButton {
		ScreenRect rect;
		UiCommand command = UiCommand::None;
		int primary = -1;
		int secondary = -1;
	};

	struct UiLifetime {
		bool alive = true;
		std::vector<EntityRef> entities;
	};

	void RebuildScreen();
	void ClearUi();
	void BuildTitle();
	void BuildModeSelect();
	void BuildRules();
	void BuildDeckSetup();
	void BuildPrivacyHandoff(bool betweenTurns);
	void BuildMatchIntroduction();
	void BuildBattleBoard();
	void BuildCenterReorder();
	void BuildResult();

	void CreateText(
		std::string name,
		std::string text,
		float x,
		float y,
		float width,
		float height,
		float fontSize,
		float red = 1.0f,
		float green = 1.0f,
		float blue = 1.0f,
		float alpha = 1.0f,
		int order = 0,
		std::optional<UiButton> button = std::nullopt,
		bool centered = false);
	void CreateButton(
		std::string text,
		ScreenRect rect,
		UiCommand command,
		int primary = -1,
		int secondary = -1,
		float fontSize = 28.0f);

	void HandlePointerDown(ScreenPoint point);
	void HandleCommand(const UiButton& button);
	void HandleBoardCell(int cellId);
	void ApplyHumanAction(const GameAction& action);
	void ResolveHumanReorder(bool applyOrder);
	void ProcessAi(float dt);
	void UpdateLocalTurnHandoff(PlayerId previousPlayer);
	void SetStatus(std::string status);

	std::string ElementLabel(ElementType element) const;
	std::string ElementSymbol(ElementType element) const;
	std::string PieceLabel(PieceId piece) const;
	std::string PublicEventLabel(const PublicEvent& event) const;
	std::string BuildDeckLine(const std::vector<ElementType>& deck) const;
	std::array<float, 3> ElementColor(ElementType element) const;

	float ViewWidth() const noexcept;
	float ViewHeight() const noexcept;
	bool IsLocalMode() const noexcept;
	bool IsHumanTurn() const noexcept;
	PlayerId ActiveViewer() const noexcept;

	// Compatibility name used by the existing cpp constructor. It is always
	// rebound to this instance's base scriptName by the explicit move constructor.
	std::string& ScriptName = scriptName;
	MatchFlowModel m_flow;
	BattleInteractionModel m_interaction;
	ElemenTacticsLlmOwner m_llm;
	// shared_ptr supports move assignment while RuntimeTextSystem is forward declared.
	// The controller remains the sole logical owner and resets it during OnStop.
	std::shared_ptr<RuntimeTextSystem> m_textSystem;
	std::shared_ptr<UiLifetime> m_uiLifetime;
	std::vector<UiButton> m_buttons;
	BoardLayout m_boardLayout;

	std::optional<std::pair<std::size_t, std::size_t>> m_selectedDeckCard;
	std::vector<ElementType> m_reorderOrder;
	std::optional<std::size_t> m_selectedReorderCard;
	std::optional<PieceId> m_reorderPiece;
	std::string m_status;
	PublicReasoning m_lastAiReasoning;

	bool m_screenDirty = true;
	bool m_localTurnHandoff = false;
	bool m_started = false;
	float m_aiDelay = 0.0f;
	float m_cachedViewWidth = 1280.0f;
	float m_cachedViewHeight = 720.0f;
};

} // namespace ElemenTactics
