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

class ElemenTacticsGameController final : public CustomScriptComponent {
public:
	ElemenTacticsGameController();
	~ElemenTacticsGameController() override;

	void OnStart() override;
	void OnUpdate(float dt) override;
	void OnStop() override;

	YAML::Node encode() override;
	bool decode(SceneContext* context, const YAML::Node& node) override;
	void inspector(SceneContext* context) override;

private:
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

	// 既存cppのコンストラクタ互換。実際のシリアライズ名は基底scriptNameへdecodeされる。
	std::string ScriptName;
	MatchFlowModel m_flow;
	BattleInteractionModel m_interaction;
	ElemenTacticsLlmFacade m_llm;
	std::unique_ptr<RuntimeTextSystem> m_textSystem;
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
