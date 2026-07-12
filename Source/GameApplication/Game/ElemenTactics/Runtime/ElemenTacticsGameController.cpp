#include "ElemenTacticsGameController.h"

#include "Component/2DspriteRendererComponent.h"
#include "Component/RenderLayerComponent.h"
#include "Component/RuntimeTextComponent.h"
#include "Component/entityNameComponent.h"
#include "Component/textureComponent.h"
#include "Component/transformComponent.h"
#include "Registry/componentRegistry.h"
#include "Service/Platform/InputSystem/InputSystem.h"
#include "System/Render/Text/RuntimeTextSystem.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace ElemenTactics {

namespace {

constexpr float Margin = 24.0f;
constexpr float ButtonHeight = 52.0f;

std::string Percent(double value){
	return std::to_string(static_cast<int>(std::clamp(value, 0.0, 1.0) * 100.0)) + "%";
}

} // namespace

ElemenTacticsGameController::ElemenTacticsGameController(){
	ScriptName = "ElemenTacticsGameController";
}

ElemenTacticsGameController::~ElemenTacticsGameController(){
	if(m_started) OnStop();
}

void ElemenTacticsGameController::OnStart(){
	if(m_started) return;
	SceneContext* context = GetEntityRef().GetScene();
	if(!context || !context->manager) return;

	m_cachedViewWidth = std::max(640.0f, context->manager->PlayerScreenSize.x);
	m_cachedViewHeight = std::max(360.0f, context->manager->PlayerScreenSize.y);
	m_boardLayout = BoardLayout::Create(m_cachedViewWidth, m_cachedViewHeight);
	m_uiLifetime = std::make_shared<UiLifetime>();
	m_textSystem = std::make_unique<RuntimeTextSystem>(context->manager);
	m_textSystem->Initialize();
	m_started = true;
	m_screenDirty = true;
	RebuildScreen();
}

void ElemenTacticsGameController::OnUpdate(float dt){
	if(!m_started) return;
	SceneContext* context = GetEntityRef().GetScene();
	if(!context || !context->manager) return;

	const float width = std::max(640.0f, context->manager->PlayerScreenSize.x);
	const float height = std::max(360.0f, context->manager->PlayerScreenSize.y);
	if(std::abs(width - m_cachedViewWidth) > 0.5f ||
		std::abs(height - m_cachedViewHeight) > 0.5f){
		m_cachedViewWidth = width;
		m_cachedViewHeight = height;
		m_boardLayout = BoardLayout::Create(width, height);
		m_screenDirty = true;
	}

	ProcessAi(dt);
	if(m_screenDirty) RebuildScreen();

	if(context->manager->input && context->manager->hwnd &&
		context->manager->input->IsMouseDown(context->manager->hwnd, 0)){
		HandlePointerDown(ScreenPoint{
			static_cast<float>(context->manager->input->GetMouseX()),
			static_cast<float>(context->manager->input->GetMouseY())
		});
	}

	if(m_screenDirty) RebuildScreen();
	if(m_textSystem) m_textSystem->ProcessDirtyText();
}

void ElemenTacticsGameController::OnStop(){
	if(!m_started) return;
	m_llm.Shutdown();
	if(m_uiLifetime){
		m_uiLifetime->alive = false;
		for(const EntityRef& entity : m_uiLifetime->entities){
			if(entity.IsValid()) QueueDestroyEntity(entity.GetEntityID());
		}
		m_uiLifetime->entities.clear();
	}
	m_buttons.clear();
	if(m_textSystem){
		m_textSystem->Finalize();
		m_textSystem.reset();
	}
	m_started = false;
}

YAML::Node ElemenTacticsGameController::encode(){
	YAML::Node node = CustomScriptComponent::encode();
	node["InitialScreen"] = "Title";
	return node;
}

bool ElemenTacticsGameController::decode(SceneContext* context, const YAML::Node& node){
	CustomScriptComponent::decode(context, node);
	return true;
}

void ElemenTacticsGameController::inspector(SceneContext* context){
	CustomScriptComponent::inspector(context);
	ImGui::Text("Runtime screen: %d", static_cast<int>(m_flow.Screen()));
	ImGui::TextUnformatted(m_status.c_str());
}

void ElemenTacticsGameController::RebuildScreen(){
	if(!m_started) return;
	ClearUi();
	m_boardLayout = BoardLayout::Create(ViewWidth(), ViewHeight());

	if(m_localTurnHandoff && m_flow.Screen() == FlowScreen::BattleBoard){
		BuildPrivacyHandoff(true);
		m_screenDirty = false;
		return;
	}

	switch(m_flow.Screen()){
	case FlowScreen::Title: BuildTitle(); break;
	case FlowScreen::ModeSelect: BuildModeSelect(); break;
	case FlowScreen::Rules: BuildRules(); break;
	case FlowScreen::DeckSetupPlayerOne:
	case FlowScreen::DeckSetupPlayerTwo: BuildDeckSetup(); break;
	case FlowScreen::LocalPrivacyHandoff: BuildPrivacyHandoff(false); break;
	case FlowScreen::MatchIntroduction: BuildMatchIntroduction(); break;
	case FlowScreen::BattleBoard: BuildBattleBoard(); break;
	case FlowScreen::CenterReorder: BuildCenterReorder(); break;
	case FlowScreen::Result: BuildResult(); break;
	default: BuildTitle(); break;
	}
	m_screenDirty = false;
}

void ElemenTacticsGameController::ClearUi(){
	if(m_uiLifetime){
		m_uiLifetime->alive = false;
		for(const EntityRef& entity : m_uiLifetime->entities){
			if(entity.IsValid()) QueueDestroyEntity(entity.GetEntityID());
		}
	}
	m_uiLifetime = std::make_shared<UiLifetime>();
	m_buttons.clear();
}

void ElemenTacticsGameController::BuildTitle(){
	CreateText("Title", "ElemenTactics", 140.0f, 115.0f,
		ViewWidth() - 280.0f, 120.0f, 72.0f,
		0.95f, 0.86f, 0.42f, 1.0f, 20, std::nullopt, true);
	CreateText("Subtitle",
		"属性を読み、順番を崩し、王と暗殺者を守れ",
		180.0f, 245.0f, ViewWidth() - 360.0f, 70.0f, 30.0f,
		0.82f, 0.88f, 1.0f, 1.0f, 20, std::nullopt, true);
	CreateText("Symbols", "炎　滴　葉　†　♛", 250.0f, 335.0f,
		ViewWidth() - 500.0f, 80.0f, 48.0f,
		1.0f, 1.0f, 1.0f, 1.0f, 20, std::nullopt, true);
	CreateButton("▶ はじめる",
		ScreenRect{ViewWidth() * 0.5f - 170.0f, 485.0f, 340.0f, ButtonHeight},
		UiCommand::TitleStart, -1, -1, 34.0f);
}

void ElemenTacticsGameController::BuildModeSelect(){
	CreateText("ModeTitle", "モード選択", 160.0f, 80.0f,
		ViewWidth() - 320.0f, 80.0f, 48.0f,
		0.95f, 0.86f, 0.42f, 1.0f, 20, std::nullopt, true);
	CreateButton("1人対LLM CPU",
		ScreenRect{ViewWidth() * 0.5f - 210.0f, 210.0f, 420.0f, ButtonHeight},
		UiCommand::ModeLlm, -1, -1, 32.0f);
	CreateButton("ローカル2人対戦",
		ScreenRect{ViewWidth() * 0.5f - 210.0f, 290.0f, 420.0f, ButtonHeight},
		UiCommand::ModeLocal, -1, -1, 32.0f);
	CreateButton("ルール／チュートリアル",
		ScreenRect{ViewWidth() * 0.5f - 210.0f, 390.0f, 420.0f, ButtonHeight},
		UiCommand::OpenRules, -1, -1, 28.0f);
	CreateButton("タイトルへ戻る",
		ScreenRect{ViewWidth() * 0.5f - 160.0f, 510.0f, 320.0f, 46.0f},
		UiCommand::BackToTitle, -1, -1, 25.0f);
}

void ElemenTacticsGameController::BuildRules(){
	CreateText("RulesTitle", "ルール", Margin, 24.0f,
		ViewWidth() - Margin * 2.0f, 58.0f, 42.0f,
		0.95f, 0.86f, 0.42f, 1.0f, 20, std::nullopt, true);
	const std::string rules =
		"各プレイヤーは 炎2・滴2・葉2・闇1・光1 の8枚を、最大3個の駒へ自由に配る。\n"
		"0枚の駒は盤面へ出ず、8・0・0も使用できる。各手番は必ず2行動。\n\n"
		"炎は葉に勝つ／葉は滴に勝つ／滴は炎に勝つ。\n"
		"光♛は通常属性に勝つが闇†に負ける。闇†は光♛に勝つが通常属性に負ける。\n\n"
		"敵駒へ行動すると双方の先頭カードで戦闘。勝者は末尾へ循環、敗者はカードを失う。\n"
		"偵察は双方の先頭を公開し、双方とも末尾へ送る。情報取得と順番妨害を同時に行う。\n\n"
		"中央へ新たに到達した駒は残存カードを任意順へ再編できる。\n"
		"自分の光または闇を失った瞬間に敗北する。";
	CreateText("RulesBody", rules, 75.0f, 100.0f,
		ViewWidth() - 150.0f, ViewHeight() - 200.0f, 25.0f,
		0.92f, 0.94f, 1.0f, 1.0f, 20);
	CreateButton("戻る", ScreenRect{50.0f, ViewHeight() - 72.0f, 180.0f, 45.0f},
		UiCommand::BackFromRules, -1, -1, 25.0f);
}

void ElemenTacticsGameController::BuildDeckSetup(){
	const bool playerTwo = m_flow.Screen() == FlowScreen::DeckSetupPlayerTwo;
	CreateText("DeckTitle",
		std::string(playerTwo ? "PLAYER 2" : "PLAYER 1") + " デッキ編成",
		40.0f, 22.0f, ViewWidth() - 80.0f, 60.0f, 40.0f,
		0.95f, 0.86f, 0.42f, 1.0f, 20, std::nullopt, true);
	CreateText("DeckHelp",
		"カードを選択し、左右で駒間移動、上下で順番変更。先頭は各列の上。",
		70.0f, 78.0f, ViewWidth() - 140.0f, 42.0f, 22.0f,
		0.80f, 0.86f, 0.98f, 1.0f, 20, std::nullopt, true);

	const auto& decks = m_flow.EditingDeck().Decks();
	const float columnWidth = std::min(300.0f, (ViewWidth() - 160.0f) / 3.0f);
	const float gap = (ViewWidth() - columnWidth * 3.0f) / 4.0f;
	for(std::size_t slot = 0; slot < decks.size(); ++slot){
		const float x = gap + (columnWidth + gap) * static_cast<float>(slot);
		CreateText("PieceColumn", "駒 " + std::to_string(slot + 1) +
			"　" + std::to_string(decks[slot].size()) + "枚",
			x, 130.0f, columnWidth, 42.0f, 27.0f,
			0.76f, 0.86f, 1.0f, 1.0f, 20, std::nullopt, true);
		if(decks[slot].empty()){
			CreateText("EmptyDeck", "（盤面へ配置しない）", x, 205.0f,
				columnWidth, 40.0f, 20.0f,
				0.56f, 0.60f, 0.68f, 1.0f, 20, std::nullopt, true);
		}
		for(std::size_t index = 0; index < decks[slot].size(); ++index){
			const ElementType element = decks[slot][index];
			const auto color = ElementColor(element);
			const bool selected = m_selectedDeckCard &&
				m_selectedDeckCard->first == slot && m_selectedDeckCard->second == index;
			const std::string label = std::string(selected ? "▶ " : "  ") +
				std::to_string(index + 1) + ". " + ElementSymbol(element) + " " + ElementLabel(element);
			CreateText("DeckCard", label, x + 20.0f,
				180.0f + static_cast<float>(index) * 45.0f,
				columnWidth - 40.0f, 40.0f, 24.0f,
				color[0], color[1], color[2], 1.0f, 30,
				UiButton{ScreenRect{x + 15.0f, 176.0f + static_cast<float>(index) * 45.0f,
					columnWidth - 30.0f, 43.0f}, UiCommand::DeckCard,
					static_cast<int>(slot), static_cast<int>(index)});
		}
	}

	const float controlsY = ViewHeight() - 142.0f;
	CreateButton("← 駒", ScreenRect{80.0f, controlsY, 130.0f, 44.0f}, UiCommand::DeckMoveLeft, -1, -1, 23.0f);
	CreateButton("駒 →", ScreenRect{225.0f, controlsY, 130.0f, 44.0f}, UiCommand::DeckMoveRight, -1, -1, 23.0f);
	CreateButton("↑ 順番", ScreenRect{380.0f, controlsY, 130.0f, 44.0f}, UiCommand::DeckMoveUp, -1, -1, 23.0f);
	CreateButton("↓ 順番", ScreenRect{525.0f, controlsY, 130.0f, 44.0f}, UiCommand::DeckMoveDown, -1, -1, 23.0f);
	CreateButton("3・3・2", ScreenRect{700.0f, controlsY, 130.0f, 44.0f}, UiCommand::DeckBalanced, -1, -1, 22.0f);
	CreateButton("8・0・0", ScreenRect{845.0f, controlsY, 130.0f, 44.0f}, UiCommand::DeckConcentrated, -1, -1, 22.0f);
	CreateButton("編成を確定", ScreenRect{ViewWidth() - 250.0f, controlsY - 4.0f, 200.0f, 52.0f},
		UiCommand::DeckConfirm, -1, -1, 27.0f);
	CreateText("DeckStatus", m_status, 80.0f, ViewHeight() - 82.0f,
		ViewWidth() - 160.0f, 34.0f, 20.0f,
		0.95f, 0.72f, 0.48f, 1.0f, 30, std::nullopt, true);
}

void ElemenTacticsGameController::BuildPrivacyHandoff(bool betweenTurns){
	std::string title;
	std::string body;
	if(betweenTurns && m_flow.Match()){
		title = std::string(m_flow.Match()->currentPlayer == PlayerId::One ? "PLAYER 1" : "PLAYER 2") + " の手番";
		body = "相手へ画面を渡した後、本人が確認して続行する。\nデッキ順と現在の先頭は続行するまで表示しない。";
	} else {
		title = "PLAYER 2へ交代";
		body = "PLAYER 1の編成は非表示になった。\n画面をPLAYER 2へ渡してから続行する。";
	}
	CreateText("PrivacyTitle", title, 120.0f, 150.0f,
		ViewWidth() - 240.0f, 90.0f, 52.0f,
		0.95f, 0.86f, 0.42f, 1.0f, 20, std::nullopt, true);
	CreateText("PrivacyBody", body, 180.0f, 275.0f,
		ViewWidth() - 360.0f, 110.0f, 27.0f,
		0.88f, 0.92f, 1.0f, 1.0f, 20, std::nullopt, true);
	CreateButton("本人が確認して続行",
		ScreenRect{ViewWidth() * 0.5f - 210.0f, 455.0f, 420.0f, ButtonHeight},
		UiCommand::PrivacyContinue, betweenTurns ? 1 : 0, -1, 29.0f);
}

void ElemenTacticsGameController::BuildMatchIntroduction(){
	CreateText("IntroTitle", "対戦開始", 160.0f, 110.0f,
		ViewWidth() - 320.0f, 90.0f, 54.0f,
		0.95f, 0.86f, 0.42f, 1.0f, 20, std::nullopt, true);
	CreateText("IntroBody",
		"各手番は必ず2行動。\n相手の公開履歴と残り枚数を読み、光♛と闇†を守れ。",
		200.0f, 245.0f, ViewWidth() - 400.0f, 120.0f, 29.0f,
		0.86f, 0.91f, 1.0f, 1.0f, 20, std::nullopt, true);
	CreateButton("MATCH START",
		ScreenRect{ViewWidth() * 0.5f - 190.0f, 450.0f, 380.0f, 58.0f},
		UiCommand::MatchBegin, -1, -1, 34.0f);
}

void ElemenTacticsGameController::BuildBattleBoard(){
	if(!m_flow.Match()) return;
	const GameState& state = *m_flow.Match();
	const PlayerId viewer = ActiveViewer();
	const PublicGameView view = ElemenTacticsRules::BuildPublicView(state, viewer);

	CreateText("Turn",
		std::string("手番: ") + (state.currentPlayer == PlayerId::One ? "PLAYER 1" : "PLAYER 2") +
		"　残り行動: " + std::to_string(state.actionsRemaining),
		ViewWidth() * 0.5f - 260.0f, 12.0f, 520.0f, 44.0f, 27.0f,
		0.95f, 0.86f, 0.42f, 1.0f, 50, std::nullopt, true);

	for(const BoardCell& cell : BoardCells){
		const ScreenPoint center = m_boardLayout.CellCenter(cell.id);
		const bool selectedCell = m_interaction.SelectedPiece() &&
			ElemenTacticsRules::FindPiece(state, *m_interaction.SelectedPiece())->cell == cell.id;
		const float red = cell.center ? 1.0f : (selectedCell ? 0.45f : 0.34f);
		const float green = cell.center ? 0.76f : (selectedCell ? 0.82f : 0.50f);
		const float blue = cell.center ? 0.24f : (selectedCell ? 1.0f : 0.72f);
		CreateText("BoardCell", "⬢",
			center.x - 42.0f, center.y - 44.0f, 84.0f, 88.0f, 62.0f,
			red, green, blue, 0.86f, 20,
			UiButton{ScreenRect{center.x - 39.0f, center.y - 39.0f, 78.0f, 78.0f},
				UiCommand::BoardCell, cell.id, -1}, true);
		const auto& occupant = view.occupancy[static_cast<std::size_t>(cell.id)];
		if(occupant){
			const PublicPieceView& piece = view.pieces[PlayerIndex(occupant->owner)][occupant->slot];
			const bool own = occupant->owner == viewer;
			CreateText("Piece", PieceLabel(*occupant) + "\n" + std::to_string(piece.remainingCards),
				center.x - 35.0f, center.y - 29.0f, 70.0f, 58.0f, 19.0f,
				own ? 0.92f : 1.0f, own ? 0.98f : 0.56f, own ? 1.0f : 0.56f,
				1.0f, 45, std::nullopt, true);
		}
	}

	const ScreenRect left = m_boardLayout.LeftHudBounds();
	CreateText("OwnInfoTitle", "自軍情報", left.x, left.y, left.width, 38.0f, 25.0f,
		0.72f, 0.88f, 1.0f, 1.0f, 30, std::nullopt, true);
	float y = left.y + 48.0f;
	for(const PublicPieceView& piece : view.pieces[PlayerIndex(viewer)]){
		if(!piece.alive) continue;
		CreateText("OwnPieceInfo", PieceLabel(piece.id) + " " + std::to_string(piece.remainingCards) + "枚\n" +
			BuildDeckLine(piece.visibleDeck),
			left.x + 8.0f, y, left.width - 16.0f, 72.0f, 18.0f,
			0.88f, 0.94f, 1.0f, 1.0f, 30);
		y += 82.0f;
	}
	CreateText("CriticalStatus", "光♛: 生存　闇†: 生存",
		left.x + 8.0f, left.y + left.height - 48.0f, left.width - 16.0f, 36.0f, 18.0f,
		0.95f, 0.86f, 0.42f, 1.0f, 30, std::nullopt, true);

	const ScreenRect right = m_boardLayout.RightHudBounds();
	CreateText("HistoryTitle", IsLocalMode() ? "公開履歴" : "CPU公開思考／履歴",
		right.x, right.y, right.width, 40.0f, 23.0f,
		0.72f, 0.88f, 1.0f, 1.0f, 30, std::nullopt, true);
	y = right.y + 48.0f;
	if(!IsLocalMode() && !m_lastAiReasoning.actionReason.empty()){
		const std::string reasoning =
			"狙い: " + m_lastAiReasoning.currentGoal + "\n" +
			"理由: " + m_lastAiReasoning.actionReason + "\n" +
			"自信度: " + Percent(m_lastAiReasoning.confidence);
		CreateText("AiReasoning", reasoning, right.x + 6.0f, y,
			right.width - 12.0f, 126.0f, 17.0f,
			0.95f, 0.84f, 0.56f, 1.0f, 30);
		y += 136.0f;
	}
	const std::size_t historyStart = view.history.size() > 7 ? view.history.size() - 7 : 0;
	for(std::size_t index = historyStart; index < view.history.size(); ++index){
		CreateText("HistoryLine", PublicEventLabel(view.history[index]),
			right.x + 6.0f, y, right.width - 12.0f, 40.0f, 16.0f,
			0.82f, 0.86f, 0.94f, 1.0f, 30);
		y += 42.0f;
	}

	const ScreenRect footer = m_boardLayout.FooterBounds();
	const std::string modeText = m_interaction.Mode() == BattleInputMode::Scout ? "偵察モード" : "移動／戦闘モード";
	CreateText("InputMode", modeText, footer.x + 10.0f, footer.y + 4.0f,
		260.0f, 34.0f, 20.0f, 0.92f, 0.94f, 1.0f, 1.0f, 40);
	CreateButton("移動／戦闘", ScreenRect{footer.x + 285.0f, footer.y + 2.0f, 170.0f, 42.0f},
		UiCommand::BattleMoveMode, -1, -1, 21.0f);
	CreateButton("偵察", ScreenRect{footer.x + 470.0f, footer.y + 2.0f, 120.0f, 42.0f},
		UiCommand::BattleScoutMode, -1, -1, 21.0f);
	CreateButton("選択解除", ScreenRect{footer.x + 605.0f, footer.y + 2.0f, 150.0f, 42.0f},
		UiCommand::BattleCancel, -1, -1, 21.0f);
	CreateText("Status", m_status, footer.x + 10.0f, footer.y + 48.0f,
		footer.width - 20.0f, 34.0f, 19.0f,
		0.95f, 0.72f, 0.48f, 1.0f, 40, std::nullopt, true);
}

void ElemenTacticsGameController::BuildCenterReorder(){
	if(!m_flow.Match() || !m_flow.Match()->pendingReorder) return;
	const GameState& state = *m_flow.Match();
	const PieceId piece = state.pendingReorder->piece;
	if(!IsLocalMode() && piece.owner == PlayerId::Two){
		CreateText("AiReorder", "CPUが中央再編を検討中…", 170.0f, 270.0f,
			ViewWidth() - 340.0f, 80.0f, 38.0f,
			0.95f, 0.86f, 0.42f, 1.0f, 30, std::nullopt, true);
		return;
	}

	if(m_reorderPiece != piece || m_reorderOrder.empty()){
		const PublicGameView view = ElemenTacticsRules::BuildPublicView(state, piece.owner);
		m_reorderOrder = view.pieces[PlayerIndex(piece.owner)][piece.slot].visibleDeck;
		m_reorderPiece = piece;
		m_selectedReorderCard.reset();
	}
	CreateText("ReorderTitle", "中央再編 — " + PieceLabel(piece), 80.0f, 70.0f,
		ViewWidth() - 160.0f, 70.0f, 46.0f,
		0.95f, 0.86f, 0.42f, 1.0f, 30, std::nullopt, true);
	CreateText("ReorderHelp", "残存カードを任意順へ並べ直す。左端が新しい先頭。",
		120.0f, 150.0f, ViewWidth() - 240.0f, 50.0f, 24.0f,
		0.84f, 0.90f, 1.0f, 1.0f, 30, std::nullopt, true);

	const float cardWidth = 118.0f;
	const float totalWidth = cardWidth * static_cast<float>(m_reorderOrder.size());
	const float startX = ViewWidth() * 0.5f - totalWidth * 0.5f;
	for(std::size_t index = 0; index < m_reorderOrder.size(); ++index){
		const auto color = ElementColor(m_reorderOrder[index]);
		const bool selected = m_selectedReorderCard && *m_selectedReorderCard == index;
		CreateText("ReorderCard", std::string(selected ? "▶\n" : "") +
			ElementSymbol(m_reorderOrder[index]) + "\n" + ElementLabel(m_reorderOrder[index]),
			startX + cardWidth * static_cast<float>(index), 260.0f,
			cardWidth - 8.0f, 120.0f, 27.0f,
			color[0], color[1], color[2], 1.0f, 35,
			UiButton{ScreenRect{startX + cardWidth * static_cast<float>(index), 250.0f,
				cardWidth - 8.0f, 140.0f}, UiCommand::ReorderCard,
				static_cast<int>(index), -1}, true);
	}
	CreateButton("← 入れ替え", ScreenRect{ViewWidth() * 0.5f - 280.0f, 430.0f, 190.0f, 48.0f},
		UiCommand::ReorderLeft, -1, -1, 24.0f);
	CreateButton("入れ替え →", ScreenRect{ViewWidth() * 0.5f - 70.0f, 430.0f, 190.0f, 48.0f},
		UiCommand::ReorderRight, -1, -1, 24.0f);
	CreateButton("この順番で確定", ScreenRect{ViewWidth() * 0.5f - 260.0f, 530.0f, 300.0f, 54.0f},
		UiCommand::ReorderConfirm, -1, -1, 27.0f);
	CreateButton("再編しない", ScreenRect{ViewWidth() * 0.5f + 70.0f, 530.0f, 210.0f, 54.0f},
		UiCommand::ReorderSkip, -1, -1, 25.0f);
	CreateText("ReorderStatus", m_status, 100.0f, 620.0f,
		ViewWidth() - 200.0f, 36.0f, 20.0f,
		0.95f, 0.72f, 0.48f, 1.0f, 40, std::nullopt, true);
}

void ElemenTacticsGameController::BuildResult(){
	if(!m_flow.Match()) return;
	const GameResult& result = m_flow.Match()->result;
	const std::string winner = result.winner == PlayerId::One ? "PLAYER 1" : "PLAYER 2";
	const std::string reason = result.reason == GameEndReason::KingLost
		? "王（光）が倒された"
		: "暗殺者（闇）が倒された";
	CreateText("GameSet", "GAME SET", 140.0f, 100.0f,
		ViewWidth() - 280.0f, 90.0f, 62.0f,
		1.0f, 0.56f, 0.32f, 1.0f, 50, std::nullopt, true);
	CreateText("Winner", winner + " WIN", 160.0f, 235.0f,
		ViewWidth() - 320.0f, 90.0f, 48.0f,
		0.95f, 0.86f, 0.42f, 1.0f, 50, std::nullopt, true);
	CreateText("ResultReason", reason, 200.0f, 340.0f,
		ViewWidth() - 400.0f, 60.0f, 29.0f,
		0.88f, 0.92f, 1.0f, 1.0f, 50, std::nullopt, true);
	CreateButton("再戦", ScreenRect{ViewWidth() * 0.5f - 230.0f, 485.0f, 190.0f, 54.0f},
		UiCommand::ResultRetry, -1, -1, 29.0f);
	CreateButton("タイトルへ", ScreenRect{ViewWidth() * 0.5f + 40.0f, 485.0f, 220.0f, 54.0f},
		UiCommand::ResultTitle, -1, -1, 27.0f);
}

void ElemenTacticsGameController::CreateText(
	std::string name,
	std::string text,
	float x,
	float y,
	float width,
	float height,
	float fontSize,
	float red,
	float green,
	float blue,
	float alpha,
	int order,
	std::optional<UiButton> button,
	bool centered){
	if(!m_started || ViewWidth() <= 0.0f || ViewHeight() <= 0.0f) return;
	const CommandEntity entity = QueueCreateEntity();
	QueueAddComponent<NameComponent>(entity);
	QueueAddComponent<TransformComponent>(entity);
	QueueAddComponent<TextureComponent>(entity);
	QueueAddComponent<RuntimeTextComponent>(entity);
	QueueAddComponent<SpriteRendererComponent>(entity);
	QueueAddComponent<RenderLayerComponent>(entity);
	QueueAddComponent<OrderInLayerComponent>(entity);

	std::weak_ptr<UiLifetime> weakLifetime = m_uiLifetime;
	const float viewWidth = ViewWidth();
	const float viewHeight = ViewHeight();
	QueueEntitySetup(entity,
		[weakLifetime, name = std::move(name), text = std::move(text),
		x, y, width, height, fontSize, red, green, blue, alpha, order, centered,
		viewWidth, viewHeight](Entity created, SceneContext& context){
			auto lifetime = weakLifetime.lock();
			if(!lifetime || !lifetime->alive || !context.component) return;
			if(auto* component = context.component->GetComponent<NameComponent>(created)){
				component->name = name;
			}
			if(auto* transform = context.component->GetComponent<TransformComponent>(created)){
				transform->position = Vector3(x / viewWidth, y / viewHeight, 0.0f);
				transform->scale = Vector3(width / viewWidth, height / viewHeight, 1.0f);
			}
			if(auto* sprite = context.component->GetComponent<SpriteRendererComponent>(created)){
				sprite->anchor = Vector2(0.0f, 0.0f);
				sprite->pivot = Vector2(0.0f, 0.0f);
			}
			if(auto* runtimeText = context.component->GetComponent<RuntimeTextComponent>(created)){
				runtimeText->Text = text;
				runtimeText->FontSize = fontSize;
				runtimeText->PixelWidth = std::max(1, static_cast<int>(width));
				runtimeText->PixelHeight = std::max(1, static_cast<int>(height));
				runtimeText->ColorR = red;
				runtimeText->ColorG = green;
				runtimeText->ColorB = blue;
				runtimeText->ColorA = alpha;
				runtimeText->Horizontal = centered
					? RuntimeTextComponent::HorizontalAlignment::Center
					: RuntimeTextComponent::HorizontalAlignment::Leading;
				runtimeText->Vertical = centered
					? RuntimeTextComponent::VerticalAlignment::Center
					: RuntimeTextComponent::VerticalAlignment::Near;
				runtimeText->WordWrap = true;
				runtimeText->AutoSizeTransform = true;
				runtimeText->MarkDirty();
			}
			if(auto* layer = context.component->GetComponent<RenderLayerComponent>(created)){
				layer->layer = RenderLayer::OverlayUI;
			}
			if(auto* orderComponent = context.component->GetComponent<OrderInLayerComponent>(created)){
				orderComponent->order = order;
			}
			lifetime->entities.emplace_back(created, &context);
		});
	if(button) m_buttons.push_back(*button);
}

void ElemenTacticsGameController::CreateButton(
	std::string text,
	ScreenRect rect,
	UiCommand command,
	int primary,
	int secondary,
	float fontSize){
	CreateText("Button", "[ " + text + " ]", rect.x, rect.y, rect.width, rect.height,
		fontSize, 0.82f, 0.92f, 1.0f, 1.0f, 60,
		UiButton{rect, command, primary, secondary}, true);
}

void ElemenTacticsGameController::HandlePointerDown(ScreenPoint point){
	for(auto iterator = m_buttons.rbegin(); iterator != m_buttons.rend(); ++iterator){
		if(iterator->rect.Contains(point)){
			HandleCommand(*iterator);
			return;
		}
	}
}

void ElemenTacticsGameController::HandleCommand(const UiButton& button){
	std::string error;
	switch(button.command){
	case UiCommand::TitleStart:
		if(m_flow.OpenModeSelect()) m_screenDirty = true;
		break;
	case UiCommand::ModeLlm:
		if(m_flow.SelectMode(GameMode::HumanVsLlm)){
			m_selectedDeckCard.reset();
			SetStatus("8枚を自由に配分し、各駒内の順番を決める");
			m_screenDirty = true;
		}
		break;
	case UiCommand::ModeLocal:
		if(m_flow.SelectMode(GameMode::LocalHumanVsHuman)){
			m_selectedDeckCard.reset();
			SetStatus("PLAYER 1の編成。確定後は内容を隠して交代する");
			m_screenDirty = true;
		}
		break;
	case UiCommand::OpenRules:
		if(m_flow.OpenRules()) m_screenDirty = true;
		break;
	case UiCommand::BackFromRules:
		if(m_flow.ReturnFromRules()) m_screenDirty = true;
		break;
	case UiCommand::BackToTitle:
		m_flow.ReturnToTitle();
		m_screenDirty = true;
		break;
	case UiCommand::DeckCard:
		m_selectedDeckCard = std::pair<std::size_t, std::size_t>{
			static_cast<std::size_t>(button.primary),
			static_cast<std::size_t>(button.secondary)};
		SetStatus("カードを選択した。左右で駒間移動、上下で順番変更");
		m_screenDirty = true;
		break;
	case UiCommand::DeckMoveLeft:
	case UiCommand::DeckMoveRight:
		if(!m_selectedDeckCard){ SetStatus("先にカードを選択する"); m_screenDirty = true; break; }
		{
			const std::size_t sourceSlot = m_selectedDeckCard->first;
			const std::size_t sourceIndex = m_selectedDeckCard->second;
			const int direction = button.command == UiCommand::DeckMoveLeft ? -1 : 1;
			const int destinationSigned = static_cast<int>(sourceSlot) + direction;
			if(destinationSigned < 0 || destinationSigned >= 3){
				SetStatus("これ以上その方向へ移動できない");
				m_screenDirty = true;
				break;
			}
			const std::size_t destination = static_cast<std::size_t>(destinationSigned);
			const std::size_t destinationIndex = m_flow.EditingDeck().Decks()[destination].size();
			if(m_flow.EditingDeck().MoveCard(sourceSlot, sourceIndex, destination, destinationIndex, &error)){
				m_selectedDeckCard = std::pair<std::size_t, std::size_t>{destination, destinationIndex};
				SetStatus("カードを別の駒へ移動した");
			} else SetStatus(error);
			m_screenDirty = true;
		}
		break;
	case UiCommand::DeckMoveUp:
	case UiCommand::DeckMoveDown:
		if(!m_selectedDeckCard){ SetStatus("先にカードを選択する"); m_screenDirty = true; break; }
		{
			const std::size_t slot = m_selectedDeckCard->first;
			const std::size_t index = m_selectedDeckCard->second;
			const std::size_t deckSize = m_flow.EditingDeck().Decks()[slot].size();
			if(button.command == UiCommand::DeckMoveUp){
				if(index == 0){ SetStatus("すでに先頭にある"); m_screenDirty = true; break; }
				if(m_flow.EditingDeck().ReorderCard(slot, index, index - 1, &error)){
					m_selectedDeckCard = std::pair<std::size_t, std::size_t>{slot, index - 1};
				}
			} else {
				if(index + 1 >= deckSize){ SetStatus("すでに末尾にある"); m_screenDirty = true; break; }
				if(m_flow.EditingDeck().ReorderCard(slot, index, index + 2, &error)){
					m_selectedDeckCard = std::pair<std::size_t, std::size_t>{slot, index + 1};
				}
			}
			SetStatus(error.empty() ? "カード順を変更した" : error);
			m_screenDirty = true;
		}
		break;
	case UiCommand::DeckBalanced:
		m_flow.EditingDeck() = DeckSetupModel::BalancedDefault();
		m_selectedDeckCard.reset();
		SetStatus("3・3・2の初期案へ戻した");
		m_screenDirty = true;
		break;
	case UiCommand::DeckConcentrated:
		m_flow.EditingDeck() = DeckSetupModel::ConcentratedDefault();
		m_selectedDeckCard.reset();
		SetStatus("8・0・0へ変更した。0枚の駒は盤面へ出ない");
		m_screenDirty = true;
		break;
	case UiCommand::DeckConfirm:
		if(m_flow.ConfirmCurrentDeck(&error)){
			m_selectedDeckCard.reset();
			SetStatus({});
			m_screenDirty = true;
		} else {
			SetStatus(error.empty() ? "編成を確定できない" : error);
			m_screenDirty = true;
		}
		break;
	case UiCommand::PrivacyContinue:
		if(button.primary == 1){
			m_localTurnHandoff = false;
			m_interaction.Cancel();
			m_screenDirty = true;
		} else if(m_flow.ConfirmPrivacyHandoff()){
			SetStatus("PLAYER 2の編成。PLAYER 1の内容は表示しない");
			m_screenDirty = true;
		}
		break;
	case UiCommand::MatchBegin:
		if(m_flow.BeginMatch(PlayerId::One, &error)){
			m_interaction.Cancel();
			m_localTurnHandoff = false;
			m_aiDelay = 0.5f;
			SetStatus("自分の駒を選び、移動先または敵駒を選択する");
			m_screenDirty = true;
		} else {
			SetStatus(error.empty() ? "対戦を開始できない" : error);
			m_screenDirty = true;
		}
		break;
	case UiCommand::BoardCell:
		HandleBoardCell(button.primary);
		break;
	case UiCommand::BattleMoveMode:
		m_interaction.SetMode(BattleInputMode::MoveOrBattle);
		SetStatus("移動／戦闘モード。自分の駒→空きマスまたは敵駒の順に選択");
		m_screenDirty = true;
		break;
	case UiCommand::BattleScoutMode:
		m_interaction.SetMode(BattleInputMode::Scout);
		SetStatus("偵察モード。自分の駒→敵駒の順に選択");
		m_screenDirty = true;
		break;
	case UiCommand::BattleCancel:
		m_interaction.Cancel();
		SetStatus("選択を解除した");
		m_screenDirty = true;
		break;
	case UiCommand::ReorderCard:
		m_selectedReorderCard = static_cast<std::size_t>(button.primary);
		SetStatus("カードを選択した。左右で位置を入れ替える");
		m_screenDirty = true;
		break;
	case UiCommand::ReorderLeft:
		if(m_selectedReorderCard && *m_selectedReorderCard > 0){
			std::swap(m_reorderOrder[*m_selectedReorderCard], m_reorderOrder[*m_selectedReorderCard - 1]);
			--*m_selectedReorderCard;
			SetStatus("カードを左へ移動した");
		} else SetStatus("左へ移動できるカードを選択する");
		m_screenDirty = true;
		break;
	case UiCommand::ReorderRight:
		if(m_selectedReorderCard && *m_selectedReorderCard + 1 < m_reorderOrder.size()){
			std::swap(m_reorderOrder[*m_selectedReorderCard], m_reorderOrder[*m_selectedReorderCard + 1]);
			++*m_selectedReorderCard;
			SetStatus("カードを右へ移動した");
		} else SetStatus("右へ移動できるカードを選択する");
		m_screenDirty = true;
		break;
	case UiCommand::ReorderConfirm:
		ResolveHumanReorder(true);
		break;
	case UiCommand::ReorderSkip:
		ResolveHumanReorder(false);
		break;
	case UiCommand::ResultRetry:
		if(m_flow.Retry(&error)){
			m_llm.ResetForNewMatch(nullptr);
			m_interaction.Cancel();
			m_lastAiReasoning = {};
			m_localTurnHandoff = false;
			m_aiDelay = 0.5f;
			SetStatus("再戦を開始した");
			m_screenDirty = true;
		} else {
			SetStatus(error.empty() ? "再戦できない" : error);
			m_screenDirty = true;
		}
		break;
	case UiCommand::ResultTitle:
		m_llm.Cancel();
		m_flow.ReturnToTitle();
		m_interaction.Cancel();
		m_lastAiReasoning = {};
		m_localTurnHandoff = false;
		SetStatus({});
		m_screenDirty = true;
		break;
	default:
		break;
	}
}

void ElemenTacticsGameController::HandleBoardCell(int cellId){
	if(!m_flow.Match() || !IsHumanTurn()) return;
	const BattleInteractionResult interaction = m_interaction.HandleCell(*m_flow.Match(), cellId);
	if(interaction.type == InteractionResultType::ActionReady && interaction.action){
		ApplyHumanAction(*interaction.action);
		return;
	}
	if(interaction.type == InteractionResultType::SelectionChanged){
		SetStatus("駒を選択した。対象マスまたは敵駒を選択する");
	} else if(interaction.type == InteractionResultType::Rejected){
		SetStatus(interaction.message);
	}
	m_screenDirty = true;
}

void ElemenTacticsGameController::ApplyHumanAction(const GameAction& action){
	GameState* state = m_flow.MutableMatch();
	if(!state) return;
	const PlayerId previousPlayer = state->currentPlayer;
	const ActionResult result = ElemenTacticsRules::ApplyAction(*state, action);
	if(!result.applied){
		SetStatus(result.error);
		m_screenDirty = true;
		return;
	}
	m_interaction.Cancel();
	if(result.battle){
		SetStatus("戦闘: " + ElementLabel(result.battle->attackerElement) + " vs " +
			ElementLabel(result.battle->defenderElement));
	} else if(result.scout){
		SetStatus("偵察: 双方の先頭を公開し、末尾へ送った");
	} else {
		SetStatus("駒を移動した");
	}
	m_flow.NotifyRuleStateChanged();
	UpdateLocalTurnHandoff(previousPlayer);
	m_aiDelay = 0.45f;
	m_screenDirty = true;
}

void ElemenTacticsGameController::ResolveHumanReorder(bool applyOrder){
	GameState* state = m_flow.MutableMatch();
	if(!state) return;
	const PlayerId previousPlayer = state->currentPlayer;
	const ReorderResult result = ElemenTacticsRules::ResolvePendingReorder(
		*state,
		applyOrder ? std::optional<std::vector<ElementType>>(m_reorderOrder) : std::nullopt);
	if(!result.applied){
		SetStatus(result.error);
		m_screenDirty = true;
		return;
	}
	SetStatus(applyOrder ? "中央再編を確定した" : "中央再編を行わなかった");
	m_reorderOrder.clear();
	m_selectedReorderCard.reset();
	m_reorderPiece.reset();
	m_flow.NotifyRuleStateChanged();
	UpdateLocalTurnHandoff(previousPlayer);
	m_aiDelay = 0.45f;
	m_screenDirty = true;
}

void ElemenTacticsGameController::ProcessAi(float dt){
	if(!m_flow.Match() || m_flow.Mode() != GameMode::HumanVsLlm) return;
	GameState* state = m_flow.MutableMatch();
	if(!state || state->result.finished || state->currentPlayer != PlayerId::Two) return;
	if(m_flow.Screen() != FlowScreen::BattleBoard && m_flow.Screen() != FlowScreen::CenterReorder) return;
	m_aiDelay -= dt;
	if(m_aiDelay > 0.0f) return;

	const AiStepResult step = AiTurnCoordinator::ExecuteNextStep(
		*state, PlayerId::Two, state->actionSerial + 0xE1E6A71CULL);
	if(!step.applied){
		SetStatus("CPU行動失敗: " + step.error);
		m_aiDelay = 0.8f;
		m_screenDirty = true;
		return;
	}
	m_lastAiReasoning = step.reasoning;
	if(step.usedFallback && m_lastAiReasoning.currentGoal.empty()){
		m_lastAiReasoning.currentGoal = "合法手からヒューリスティック評価で選択";
		m_lastAiReasoning.actionReason = "LLM未使用時の安全なフォールバック";
		m_lastAiReasoning.confidence = 0.5;
	}
	SetStatus(step.reorder ? "CPUが中央再編を完了した" : "CPUが行動した");
	m_flow.NotifyRuleStateChanged();
	m_aiDelay = 0.65f;
	m_screenDirty = true;
}

void ElemenTacticsGameController::UpdateLocalTurnHandoff(PlayerId previousPlayer){
	if(!IsLocalMode() || !m_flow.Match() || m_flow.Match()->result.finished) return;
	if(m_flow.Match()->currentPlayer != previousPlayer){
		m_localTurnHandoff = true;
	}
}

void ElemenTacticsGameController::SetStatus(std::string status){
	m_status = std::move(status);
}

std::string ElemenTacticsGameController::ElementLabel(ElementType element) const{
	switch(element){
	case ElementType::Fire: return "火";
	case ElementType::Water: return "水";
	case ElementType::Wood: return "木";
	case ElementType::Dark: return "闇";
	case ElementType::Light: return "光";
	default: return "?";
	}
}

std::string ElemenTacticsGameController::ElementSymbol(ElementType element) const{
	switch(element){
	case ElementType::Fire: return "炎";
	case ElementType::Water: return "滴";
	case ElementType::Wood: return "葉";
	case ElementType::Dark: return "†";
	case ElementType::Light: return "♛";
	default: return "?";
	}
}

std::string ElemenTacticsGameController::PieceLabel(PieceId piece) const{
	return std::string(piece.owner == PlayerId::One ? "P1-" : "P2-") +
		std::to_string(static_cast<int>(piece.slot) + 1);
}

std::string ElemenTacticsGameController::PublicEventLabel(const PublicEvent& event) const{
	std::ostringstream stream;
	switch(event.type){
	case PublicEventType::Move:
		stream << (event.primaryPiece ? PieceLabel(*event.primaryPiece) : "駒") << " → マス" << event.cell;
		break;
	case PublicEventType::Battle:
		stream << "戦闘 " << (event.primaryElement ? ElementLabel(*event.primaryElement) : "?")
			<< " vs " << (event.secondaryElement ? ElementLabel(*event.secondaryElement) : "?");
		break;
	case PublicEventType::Scout:
		stream << "偵察 " << (event.primaryElement ? ElementLabel(*event.primaryElement) : "?")
			<< " / " << (event.secondaryElement ? ElementLabel(*event.secondaryElement) : "?");
		break;
	case PublicEventType::CardLost:
		stream << (event.primaryPiece ? PieceLabel(*event.primaryPiece) : "駒") << "が"
			<< (event.primaryElement ? ElementLabel(*event.primaryElement) : "カード") << "を失った";
		break;
	case PublicEventType::PieceDefeated:
		stream << (event.primaryPiece ? PieceLabel(*event.primaryPiece) : "駒") << " 撃破";
		break;
	case PublicEventType::CenterReordered:
		stream << (event.primaryPiece ? PieceLabel(*event.primaryPiece) : "駒") << " 中央再編";
		break;
	case PublicEventType::GameSet:
		stream << "GAME SET";
		break;
	case PublicEventType::CardRotated:
		stream << (event.primaryPiece ? PieceLabel(*event.primaryPiece) : "駒") << " 末尾へ循環";
		break;
	default:
		stream << "公開イベント";
		break;
	}
	return stream.str();
}

std::string ElemenTacticsGameController::BuildDeckLine(const std::vector<ElementType>& deck) const{
	std::string result;
	for(std::size_t index = 0; index < deck.size(); ++index){
		if(index) result += "→";
		result += ElementSymbol(deck[index]);
	}
	return result.empty() ? "（空）" : result;
}

std::array<float, 3> ElemenTacticsGameController::ElementColor(ElementType element) const{
	switch(element){
	case ElementType::Fire: return {1.0f, 0.42f, 0.25f};
	case ElementType::Water: return {0.30f, 0.72f, 1.0f};
	case ElementType::Wood: return {0.42f, 0.90f, 0.48f};
	case ElementType::Dark: return {0.74f, 0.48f, 0.96f};
	case ElementType::Light: return {1.0f, 0.86f, 0.30f};
	default: return {1.0f, 1.0f, 1.0f};
	}
}

float ElemenTacticsGameController::ViewWidth() const noexcept{
	return std::max(640.0f, m_cachedViewWidth);
}

float ElemenTacticsGameController::ViewHeight() const noexcept{
	return std::max(360.0f, m_cachedViewHeight);
}

bool ElemenTacticsGameController::IsLocalMode() const noexcept{
	return m_flow.Mode() && *m_flow.Mode() == GameMode::LocalHumanVsHuman;
}

bool ElemenTacticsGameController::IsHumanTurn() const noexcept{
	if(!m_flow.Match()) return false;
	return IsLocalMode() || m_flow.Match()->currentPlayer == PlayerId::One;
}

PlayerId ElemenTacticsGameController::ActiveViewer() const noexcept{
	if(!m_flow.Match()) return PlayerId::One;
	return IsLocalMode() ? m_flow.Match()->currentPlayer : PlayerId::One;
}

} // namespace ElemenTactics
