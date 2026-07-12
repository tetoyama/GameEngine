#pragma once

#include "ElemenTacticsGameController.h"

#include "Component/2DspriteRendererComponent.h"
#include "Component/RenderLayerComponent.h"
#include "Component/RuntimeTextComponent.h"
#include "Component/entityNameComponent.h"
#include "Component/textureComponent.h"
#include "Component/transformComponent.h"
#include "Registry/componentRegistry.h"
#include "System/Render/Text/RuntimeTextSystem.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <windows.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace ElemenTactics {

// Keyboard-first runtime input. Deck setup, board play, and center reorder use
// dedicated navigation state instead of pretending the keyboard is a mouse.
class ElemenTacticsKeyboardNavigator final : public CustomScriptComponent {
public:
	ElemenTacticsKeyboardNavigator(){
		scriptName = "ElemenTacticsKeyboardNavigator";
		SetExecutionOrder(SystemTaskDomain::Frame, SystemPhase::Default, 100);
	}

	void OnStart() override{
		m_lifetime = std::make_shared<Lifetime>();
		m_hasContext = false;
		m_overlayQueued = false;
		EnsureOverlay();
	}

	void OnUpdate(float) override{
		Controller* controller = ResolveController();
		if(!controller || !controller->m_started) return;

		EnsureOverlay();
		SyncContext(controller);

		if(IsTurnHandoff(controller)){
			HandleGenericKeyboard(controller);
		}else{
			switch(controller->m_flow.Screen()){
			case FlowScreen::DeckSetupPlayerOne:
			case FlowScreen::DeckSetupPlayerTwo:
				HandleDeckKeyboard(controller);
				break;
			case FlowScreen::BattleBoard:
				HandleBattleKeyboard(controller);
				break;
			case FlowScreen::CenterReorder:
				HandleReorderKeyboard(controller);
				break;
			default:
				HandleGenericKeyboard(controller);
				break;
			}
		}

		// A command may have changed the flow screen without changing components.
		SyncContext(controller);
		UpdateCursor(controller);
		UpdateHelp(controller);
		if(controller->m_textSystem){
			controller->m_textSystem->ProcessDirtyText();
		}
	}

	void OnStop() override{
		if(m_lifetime){
			m_lifetime->alive = false;
			DestroyOverlayEntity(m_lifetime->cursor);
			DestroyOverlayEntity(m_lifetime->help);
		}
		m_overlayQueued = false;
	}

private:
	using Controller = ElemenTacticsGameController;
	using Command = Controller::UiCommand;
	using Button = Controller::UiButton;

	struct Lifetime {
		bool alive = true;
		EntityRef cursor;
		EntityRef help;
	};

	Controller* ResolveController() const{
		const EntityRef self = GetEntityRef();
		SceneContext* context = self.GetScene();
		if(!self.IsValid() || !context || !context->component) return nullptr;
		return context->component->GetComponent<Controller>(self.GetEntityID());
	}

	static bool IsDeckScreen(FlowScreen screen) noexcept{
		return screen == FlowScreen::DeckSetupPlayerOne ||
			screen == FlowScreen::DeckSetupPlayerTwo;
	}

	static bool IsTurnHandoff(const Controller* controller) noexcept{
		return controller && controller->m_localTurnHandoff &&
			controller->m_flow.Screen() == FlowScreen::BattleBoard;
	}

	void SyncContext(Controller* controller){
		const FlowScreen screen = controller->m_flow.Screen();
		const bool handoff = IsTurnHandoff(controller);
		if(m_hasContext && screen == m_screen && handoff == m_handoff) return;

		m_screen = screen;
		m_handoff = handoff;
		m_hasContext = true;
		m_focusIndex = DefaultGenericFocus(controller);

		if(IsDeckScreen(screen)) InitializeDeckFocus(controller);
		if(screen == FlowScreen::BattleBoard && !handoff) InitializeBoardFocus(controller);
		if(screen == FlowScreen::CenterReorder) InitializeReorderFocus(controller);
	}

	std::size_t DefaultGenericFocus(const Controller* controller) const{
		if(!controller || controller->m_buttons.empty()) return 0;
		return 0;
	}

	void RebuildController(Controller* controller){
		controller->m_screenDirty = true;
		controller->RebuildScreen();
	}

	void HandleGenericKeyboard(Controller* controller){
		if(HandleGenericShortcut(controller)) return;

		if(GetKeyDown(VK_BACK)){
			HandleGenericBack(controller);
			return;
		}
		if(controller->m_buttons.empty()) return;

		if(GetKeyDown(VK_TAB)){
			const bool reverse = GetKey(VK_SHIFT) || GetKey(VK_LSHIFT) || GetKey(VK_RSHIFT);
			MoveGenericSequential(controller, reverse ? -1 : 1);
			return;
		}
		if(GetKeyDown(VK_UP) || GetKeyDown('W')){
			MoveGenericSpatial(controller, 0.0f, -1.0f);
			return;
		}
		if(GetKeyDown(VK_DOWN) || GetKeyDown('S')){
			MoveGenericSpatial(controller, 0.0f, 1.0f);
			return;
		}
		if(GetKeyDown(VK_LEFT) || GetKeyDown('A')){
			MoveGenericSpatial(controller, -1.0f, 0.0f);
			return;
		}
		if(GetKeyDown(VK_RIGHT) || GetKeyDown('D')){
			MoveGenericSpatial(controller, 1.0f, 0.0f);
			return;
		}
		if(GetKeyDown(VK_RETURN) || GetKeyDown(VK_SPACE)){
			ActivateGenericFocused(controller);
		}
	}

	bool HandleGenericShortcut(Controller* controller){
		switch(controller->m_flow.Screen()){
		case FlowScreen::ModeSelect:
			if(GetKeyDown('1')) return ActivateCommand(controller, Command::ModeLlm);
			if(GetKeyDown('2')) return ActivateCommand(controller, Command::ModeLocal);
			if(GetKeyDown('3')) return ActivateCommand(controller, Command::OpenRules);
			break;
		case FlowScreen::Result:
			if(GetKeyDown('R')) return ActivateCommand(controller, Command::ResultRetry);
			if(GetKeyDown('T')) return ActivateCommand(controller, Command::ResultTitle);
			break;
		default:
			break;
		}
		return false;
	}

	void HandleGenericBack(Controller* controller){
		if(IsTurnHandoff(controller)) return;

		switch(controller->m_flow.Screen()){
		case FlowScreen::ModeSelect:
			controller->m_flow.ReturnToTitle();
			controller->SetStatus({});
			RebuildController(controller);
			break;
		case FlowScreen::Rules:
			if(controller->m_flow.ReturnFromRules()) RebuildController(controller);
			break;
		case FlowScreen::LocalPrivacyHandoff:
			if(controller->m_flow.ReturnFromPrivacyHandoff()){
				controller->SetStatus("PLAYER 1の編成へ戻った");
				RebuildController(controller);
			}
			break;
		case FlowScreen::MatchIntroduction:
			if(controller->m_flow.ReturnFromMatchIntroduction()){
				controller->SetStatus("デッキ編成へ戻った");
				RebuildController(controller);
			}
			break;
		case FlowScreen::Result:
			ActivateCommand(controller, Command::ResultTitle);
			break;
		default:
			break;
		}
	}

	static ScreenPoint Center(const ScreenRect& rect) noexcept{
		return {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
	}

	void ClampGenericFocus(const Controller* controller){
		if(!controller || controller->m_buttons.empty()){
			m_focusIndex = 0;
			return;
		}
		if(m_focusIndex >= controller->m_buttons.size()) m_focusIndex = 0;
	}

	void MoveGenericSequential(const Controller* controller, int direction){
		const std::size_t count = controller->m_buttons.size();
		if(count == 0) return;
		m_focusIndex = direction < 0
			? (m_focusIndex + count - 1) % count
			: (m_focusIndex + 1) % count;
	}

	void MoveGenericSpatial(const Controller* controller, float directionX, float directionY){
		if(controller->m_buttons.empty()) return;
		ClampGenericFocus(controller);
		const ScreenPoint origin = Center(controller->m_buttons[m_focusIndex].rect);
		std::size_t best = m_focusIndex;
		float bestScore = std::numeric_limits<float>::max();

		for(std::size_t i = 0; i < controller->m_buttons.size(); ++i){
			if(i == m_focusIndex) continue;
			const ScreenPoint candidate = Center(controller->m_buttons[i].rect);
			const float dx = candidate.x - origin.x;
			const float dy = candidate.y - origin.y;
			const float forward = dx * directionX + dy * directionY;
			if(forward <= 1.0f) continue;
			const float lateral = std::abs(dx * directionY - dy * directionX);
			const float score = forward + lateral * 2.6f;
			if(score < bestScore){
				bestScore = score;
				best = i;
			}
		}

		if(best == m_focusIndex){
			MoveGenericSequential(controller,
				directionX < 0.0f || directionY < 0.0f ? -1 : 1);
		}else{
			m_focusIndex = best;
		}
	}

	void ActivateGenericFocused(Controller* controller){
		if(controller->m_buttons.empty()) return;
		ClampGenericFocus(controller);
		ExecuteButton(controller, controller->m_buttons[m_focusIndex]);
	}

	bool ActivateCommand(Controller* controller, Command command){
		for(std::size_t i = 0; i < controller->m_buttons.size(); ++i){
			if(controller->m_buttons[i].command != command) continue;
			m_focusIndex = i;
			ExecuteButton(controller, controller->m_buttons[i]);
			return true;
		}
		return false;
	}

	void ExecuteButton(Controller* controller, Button button){
		controller->HandleCommand(button);
		if(controller->m_screenDirty) RebuildController(controller);
	}

	// ---------------------------------------------------------------------
	// Deck setup
	// Browse mode: arrows move the cursor. Enter picks a card up.
	// Holding mode: left/right changes piece, up/down changes order.
	// ---------------------------------------------------------------------
	void InitializeDeckFocus(Controller* controller){
		if(controller->m_selectedDeckCard){
			m_deckFocusSlot = controller->m_selectedDeckCard->first;
			m_deckFocusIndex = controller->m_selectedDeckCard->second;
			ClampDeckFocus(controller);
			return;
		}
		const auto& decks = controller->m_flow.EditingDeck().Decks();
		for(std::size_t slot = 0; slot < decks.size(); ++slot){
			if(!decks[slot].empty()){
				m_deckFocusSlot = slot;
				m_deckFocusIndex = 0;
				return;
			}
		}
		m_deckFocusSlot = 0;
		m_deckFocusIndex = 0;
	}

	void ClampDeckFocus(Controller* controller){
		const auto& decks = controller->m_flow.EditingDeck().Decks();
		if(m_deckFocusSlot >= decks.size()) m_deckFocusSlot = 0;
		if(decks[m_deckFocusSlot].empty()){
			m_deckFocusIndex = 0;
		}else if(m_deckFocusIndex >= decks[m_deckFocusSlot].size()){
			m_deckFocusIndex = decks[m_deckFocusSlot].size() - 1;
		}
	}

	void HandleDeckKeyboard(Controller* controller){
		if(GetKeyDown('B')){
			controller->m_flow.EditingDeck() = DeckSetupModel::BalancedDefault();
			controller->m_selectedDeckCard.reset();
			controller->SetStatus("3・3・2の初期案へ戻した");
			InitializeDeckFocus(controller);
			RebuildController(controller);
			return;
		}
		if(GetKeyDown('X')){
			controller->m_flow.EditingDeck() = DeckSetupModel::ConcentratedDefault();
			controller->m_selectedDeckCard.reset();
			controller->SetStatus("8・0・0へ変更した。0枚の駒は盤面へ出ない");
			InitializeDeckFocus(controller);
			RebuildController(controller);
			return;
		}
		if(GetKeyDown('C')){
			ConfirmDeck(controller);
			return;
		}
		if(GetKeyDown(VK_BACK)){
			if(controller->m_selectedDeckCard){
				ReleaseDeckCard(controller);
			}else if(controller->m_flow.CancelDeckSetup()){
				controller->SetStatus({});
				RebuildController(controller);
			}
			return;
		}

		if(controller->m_selectedDeckCard){
			HandleHeldDeckCard(controller);
		}else{
			HandleDeckBrowse(controller);
		}
	}

	void HandleDeckBrowse(Controller* controller){
		if(GetKeyDown('1')){ FocusDeckSlot(controller, 0); return; }
		if(GetKeyDown('2')){ FocusDeckSlot(controller, 1); return; }
		if(GetKeyDown('3')){ FocusDeckSlot(controller, 2); return; }
		if(GetKeyDown(VK_HOME)){ FocusDeckExtreme(controller, true); return; }
		if(GetKeyDown(VK_END)){ FocusDeckExtreme(controller, false); return; }
		if(GetKeyDown(VK_TAB)){
			const bool reverse = GetKey(VK_SHIFT) || GetKey(VK_LSHIFT) || GetKey(VK_RSHIFT);
			MoveDeckFocusSequential(controller, reverse ? -1 : 1);
			return;
		}
		if(GetKeyDown(VK_LEFT) || GetKeyDown('A')){
			MoveDeckFocusHorizontal(controller, -1);
			return;
		}
		if(GetKeyDown(VK_RIGHT) || GetKeyDown('D')){
			MoveDeckFocusHorizontal(controller, 1);
			return;
		}
		if(GetKeyDown(VK_UP) || GetKeyDown('W')){
			MoveDeckFocusVertical(controller, -1);
			return;
		}
		if(GetKeyDown(VK_DOWN) || GetKeyDown('S')){
			MoveDeckFocusVertical(controller, 1);
			return;
		}
		if(GetKeyDown(VK_RETURN) || GetKeyDown(VK_SPACE)){
			SelectFocusedDeckCard(controller);
		}
	}

	void HandleHeldDeckCard(Controller* controller){
		if(GetKeyDown('1')){ MoveSelectedDeckToSlot(controller, 0); return; }
		if(GetKeyDown('2')){ MoveSelectedDeckToSlot(controller, 1); return; }
		if(GetKeyDown('3')){ MoveSelectedDeckToSlot(controller, 2); return; }
		if(GetKeyDown(VK_HOME)){ MoveSelectedDeckExtreme(controller, true); return; }
		if(GetKeyDown(VK_END)){ MoveSelectedDeckExtreme(controller, false); return; }
		if(GetKeyDown(VK_LEFT) || GetKeyDown('A')){
			MoveSelectedDeckToAdjacentSlot(controller, -1);
			return;
		}
		if(GetKeyDown(VK_RIGHT) || GetKeyDown('D')){
			MoveSelectedDeckToAdjacentSlot(controller, 1);
			return;
		}
		if(GetKeyDown(VK_UP) || GetKeyDown('W') || GetKeyDown('Q') || GetKeyDown(VK_PRIOR)){
			MoveSelectedDeckOrder(controller, -1);
			return;
		}
		if(GetKeyDown(VK_DOWN) || GetKeyDown('S') || GetKeyDown('E') || GetKeyDown(VK_NEXT)){
			MoveSelectedDeckOrder(controller, 1);
			return;
		}
		if(GetKeyDown(VK_RETURN) || GetKeyDown(VK_SPACE)){
			ReleaseDeckCard(controller);
		}
	}

	void FocusDeckSlot(Controller* controller, std::size_t slot){
		m_deckFocusSlot = std::min<std::size_t>(slot, 2);
		ClampDeckFocus(controller);
		controller->SetStatus("駒 " + std::to_string(m_deckFocusSlot + 1) + " のカードを選択中");
	}

	void FocusDeckExtreme(Controller* controller, bool front){
		const auto& deck = controller->m_flow.EditingDeck().Decks()[m_deckFocusSlot];
		if(deck.empty()) return;
		m_deckFocusIndex = front ? 0 : deck.size() - 1;
	}

	void MoveDeckFocusHorizontal(Controller* controller, int direction){
		const int next = static_cast<int>(m_deckFocusSlot) + direction;
		if(next < 0 || next >= 3) return;
		m_deckFocusSlot = static_cast<std::size_t>(next);
		ClampDeckFocus(controller);
	}

	void MoveDeckFocusVertical(Controller* controller, int direction){
		const auto& deck = controller->m_flow.EditingDeck().Decks()[m_deckFocusSlot];
		if(deck.empty()){
			controller->SetStatus("この駒にはカードがない");
			return;
		}
		if(direction < 0){
			if(m_deckFocusIndex > 0) --m_deckFocusIndex;
		}else if(m_deckFocusIndex + 1 < deck.size()){
			++m_deckFocusIndex;
		}
	}

	void MoveDeckFocusSequential(Controller* controller, int direction){
		const auto& decks = controller->m_flow.EditingDeck().Decks();
		std::size_t total = 0;
		for(const auto& deck : decks) total += deck.size();
		if(total == 0) return;

		std::size_t flat = 0;
		bool found = false;
		for(std::size_t slot = 0; slot < decks.size() && !found; ++slot){
			for(std::size_t index = 0; index < decks[slot].size(); ++index, ++flat){
				if(slot == m_deckFocusSlot && index == m_deckFocusIndex){
					found = true;
					break;
				}
			}
		}
		if(!found) flat = 0;
		flat = direction < 0 ? (flat + total - 1) % total : (flat + 1) % total;

		for(std::size_t slot = 0; slot < decks.size(); ++slot){
			if(flat < decks[slot].size()){
				m_deckFocusSlot = slot;
				m_deckFocusIndex = flat;
				return;
			}
			flat -= decks[slot].size();
		}
	}

	void SelectFocusedDeckCard(Controller* controller){
		ClampDeckFocus(controller);
		const auto& deck = controller->m_flow.EditingDeck().Decks()[m_deckFocusSlot];
		if(deck.empty()){
			controller->SetStatus("この駒には選択できるカードがない");
			RebuildController(controller);
			return;
		}
		controller->m_selectedDeckCard = {m_deckFocusSlot, m_deckFocusIndex};
		controller->SetStatus("カード保持中: ←→で駒移動、↑↓で順番変更、Enterで置く");
		RebuildController(controller);
	}

	bool ReadSelectedDeckCard(
		Controller* controller,
		std::size_t& slot,
		std::size_t& index
	){
		if(!controller->m_selectedDeckCard) return false;
		slot = controller->m_selectedDeckCard->first;
		index = controller->m_selectedDeckCard->second;
		const auto& decks = controller->m_flow.EditingDeck().Decks();
		if(slot >= decks.size() || index >= decks[slot].size()){
			controller->m_selectedDeckCard.reset();
			InitializeDeckFocus(controller);
			controller->SetStatus("カード選択をリセットした");
			RebuildController(controller);
			return false;
		}
		return true;
	}

	void MoveSelectedDeckToAdjacentSlot(Controller* controller, int direction){
		std::size_t slot = 0;
		std::size_t index = 0;
		if(!ReadSelectedDeckCard(controller, slot, index)) return;
		const int destination = static_cast<int>(slot) + direction;
		if(destination < 0 || destination >= 3){
			controller->SetStatus("これ以上その方向の駒へ移動できない");
			return;
		}
		MoveSelectedDeckToSlot(controller, static_cast<std::size_t>(destination));
	}

	void MoveSelectedDeckToSlot(Controller* controller, std::size_t destinationSlot){
		std::size_t sourceSlot = 0;
		std::size_t sourceIndex = 0;
		if(!ReadSelectedDeckCard(controller, sourceSlot, sourceIndex)) return;
		if(destinationSlot >= 3) return;
		if(destinationSlot == sourceSlot){
			controller->SetStatus("選択カードはすでに駒 " +
				std::to_string(destinationSlot + 1) + " にある");
			return;
		}

		const std::size_t destinationSize =
			controller->m_flow.EditingDeck().Decks()[destinationSlot].size();
		const std::size_t destinationIndex = std::min(sourceIndex, destinationSize);
		std::string error;
		if(controller->m_flow.EditingDeck().MoveCard(
			sourceSlot, sourceIndex, destinationSlot, destinationIndex, &error)){
			controller->m_selectedDeckCard = {destinationSlot, destinationIndex};
			m_deckFocusSlot = destinationSlot;
			m_deckFocusIndex = destinationIndex;
			controller->SetStatus("カードを駒 " +
				std::to_string(destinationSlot + 1) + " へ移動した");
		}else{
			controller->SetStatus(error.empty() ? "カードを移動できない" : error);
		}
		RebuildController(controller);
	}

	void MoveSelectedDeckOrder(Controller* controller, int direction){
		std::size_t slot = 0;
		std::size_t index = 0;
		if(!ReadSelectedDeckCard(controller, slot, index)) return;
		const std::size_t size = controller->m_flow.EditingDeck().Decks()[slot].size();
		if(direction < 0 && index == 0){
			controller->SetStatus("すでに先頭カードです");
			return;
		}
		if(direction > 0 && index + 1 >= size){
			controller->SetStatus("すでに末尾カードです");
			return;
		}

		std::string error;
		const std::size_t destination = direction < 0 ? index - 1 : index + 2;
		if(controller->m_flow.EditingDeck().ReorderCard(slot, index, destination, &error)){
			const std::size_t nextIndex = direction < 0 ? index - 1 : index + 1;
			controller->m_selectedDeckCard = {slot, nextIndex};
			m_deckFocusSlot = slot;
			m_deckFocusIndex = nextIndex;
			controller->SetStatus(direction < 0
				? "カードを1つ前へ移動した"
				: "カードを1つ後ろへ移動した");
		}else{
			controller->SetStatus(error.empty() ? "順番を変更できない" : error);
		}
		RebuildController(controller);
	}

	void MoveSelectedDeckExtreme(Controller* controller, bool front){
		std::size_t slot = 0;
		std::size_t index = 0;
		if(!ReadSelectedDeckCard(controller, slot, index)) return;
		const std::size_t size = controller->m_flow.EditingDeck().Decks()[slot].size();
		std::string error;
		if(controller->m_flow.EditingDeck().ReorderCard(
			slot, index, front ? 0 : size, &error)){
			const std::size_t nextIndex = front ? 0 : size - 1;
			controller->m_selectedDeckCard = {slot, nextIndex};
			m_deckFocusSlot = slot;
			m_deckFocusIndex = nextIndex;
			controller->SetStatus(front
				? "カードを先頭へ移動した"
				: "カードを末尾へ移動した");
		}else{
			controller->SetStatus(error.empty() ? "順番を変更できない" : error);
		}
		RebuildController(controller);
	}

	void ReleaseDeckCard(Controller* controller){
		if(controller->m_selectedDeckCard){
			m_deckFocusSlot = controller->m_selectedDeckCard->first;
			m_deckFocusIndex = controller->m_selectedDeckCard->second;
		}
		controller->m_selectedDeckCard.reset();
		controller->SetStatus("カードを置いた。矢印で次のカードを選択できる");
		RebuildController(controller);
	}

	void ConfirmDeck(Controller* controller){
		controller->m_selectedDeckCard.reset();
		std::string error;
		if(controller->m_flow.ConfirmCurrentDeck(&error)){
			controller->SetStatus({});
		}else{
			controller->SetStatus(error.empty() ? "編成を確定できない" : error);
		}
		RebuildController(controller);
	}

	// ---------------------------------------------------------------------
	// Battle board: arrows remain inside the 19-cell board.
	// ---------------------------------------------------------------------
	void InitializeBoardFocus(Controller* controller){
		m_boardFocusCell = 0;
		if(!controller->m_flow.Match()) return;
		const GameState& state = *controller->m_flow.Match();
		if(controller->m_interaction.SelectedPiece()){
			const PieceId id = *controller->m_interaction.SelectedPiece();
			const PieceState& piece = state.players[PlayerIndex(id.owner)].pieces[id.slot];
			if(piece.cell){ m_boardFocusCell = *piece.cell; return; }
		}
		const PlayerState& player = state.players[PlayerIndex(controller->ActiveViewer())];
		for(const PieceState& piece : player.pieces){
			if(piece.IsAlive() && piece.cell){ m_boardFocusCell = *piece.cell; return; }
		}
	}

	void HandleBattleKeyboard(Controller* controller){
		if(GetKeyDown('1')){
			controller->m_interaction.SetMode(BattleInputMode::MoveOrBattle);
			controller->SetStatus("移動／戦闘: 自分の駒を選び、次に対象マスを選ぶ");
			RebuildController(controller);
			return;
		}
		if(GetKeyDown('2')){
			controller->m_interaction.SetMode(BattleInputMode::Scout);
			controller->SetStatus("偵察: 自分の駒を選び、次に敵駒を選ぶ");
			RebuildController(controller);
			return;
		}
		if(GetKeyDown(VK_BACK)){
			controller->m_interaction.Cancel();
			controller->SetStatus("選択を解除した");
			RebuildController(controller);
			return;
		}
		if(GetKeyDown(VK_TAB)){
			const bool reverse = GetKey(VK_SHIFT) || GetKey(VK_LSHIFT) || GetKey(VK_RSHIFT);
			MoveBoardSequential(reverse ? -1 : 1);
			return;
		}
		if(GetKeyDown(VK_UP) || GetKeyDown('W')){
			MoveBoardFocus(controller, 0.0f, -1.0f);
			return;
		}
		if(GetKeyDown(VK_DOWN) || GetKeyDown('S')){
			MoveBoardFocus(controller, 0.0f, 1.0f);
			return;
		}
		if(GetKeyDown(VK_LEFT) || GetKeyDown('A')){
			MoveBoardFocus(controller, -1.0f, 0.0f);
			return;
		}
		if(GetKeyDown(VK_RIGHT) || GetKeyDown('D')){
			MoveBoardFocus(controller, 1.0f, 0.0f);
			return;
		}
		if(GetKeyDown(VK_RETURN) || GetKeyDown(VK_SPACE)){
			controller->HandleBoardCell(m_boardFocusCell);
			if(controller->m_screenDirty) RebuildController(controller);
		}
	}

	void MoveBoardSequential(int direction){
		std::size_t current = 0;
		for(std::size_t i = 0; i < BoardCells.size(); ++i){
			if(BoardCells[i].id == m_boardFocusCell){ current = i; break; }
		}
		const std::size_t next = direction < 0
			? (current + BoardCells.size() - 1) % BoardCells.size()
			: (current + 1) % BoardCells.size();
		m_boardFocusCell = BoardCells[next].id;
	}

	void MoveBoardFocus(Controller* controller, float directionX, float directionY){
		const ScreenPoint origin = controller->m_boardLayout.CellCenter(m_boardFocusCell);
		int bestCell = m_boardFocusCell;
		float bestScore = std::numeric_limits<float>::max();
		for(const BoardCell& cell : BoardCells){
			if(cell.id == m_boardFocusCell) continue;
			const ScreenPoint candidate = controller->m_boardLayout.CellCenter(cell.id);
			const float dx = candidate.x - origin.x;
			const float dy = candidate.y - origin.y;
			const float forward = dx * directionX + dy * directionY;
			if(forward <= 1.0f) continue;
			const float lateral = std::abs(dx * directionY - dy * directionX);
			const float score = forward + lateral * 2.2f;
			if(score < bestScore){
				bestScore = score;
				bestCell = cell.id;
			}
		}
		m_boardFocusCell = bestCell;
	}

	// ---------------------------------------------------------------------
	// Center reorder: cursor and held-card state are independent from buttons.
	// ---------------------------------------------------------------------
	void InitializeReorderFocus(Controller* controller){
		m_reorderFocusIndex = controller->m_selectedReorderCard
			? *controller->m_selectedReorderCard
			: 0;
		ClampReorderFocus(controller);
	}

	void ClampReorderFocus(Controller* controller){
		if(controller->m_reorderOrder.empty()){
			m_reorderFocusIndex = 0;
			return;
		}
		if(m_reorderFocusIndex >= controller->m_reorderOrder.size()){
			m_reorderFocusIndex = controller->m_reorderOrder.size() - 1;
		}
	}

	void HandleReorderKeyboard(Controller* controller){
		if(GetKeyDown('C')){
			controller->ResolveHumanReorder(true);
			return;
		}
		if(GetKeyDown('X')){
			controller->ResolveHumanReorder(false);
			return;
		}
		if(GetKeyDown(VK_BACK)){
			if(controller->m_selectedReorderCard){
				controller->m_selectedReorderCard.reset();
				controller->SetStatus("カード保持を解除した");
				RebuildController(controller);
			}else{
				controller->ResolveHumanReorder(false);
			}
			return;
		}
		if(controller->m_reorderOrder.empty()) return;

		if(controller->m_selectedReorderCard){
			if(GetKeyDown(VK_HOME)){
				MoveSelectedReorderExtreme(controller, true);
				return;
			}
			if(GetKeyDown(VK_END)){
				MoveSelectedReorderExtreme(controller, false);
				return;
			}
			if(GetKeyDown(VK_LEFT) || GetKeyDown('A') ||
				GetKeyDown(VK_UP) || GetKeyDown('W') || GetKeyDown('Q')){
				MoveSelectedReorder(controller, -1);
				return;
			}
			if(GetKeyDown(VK_RIGHT) || GetKeyDown('D') ||
				GetKeyDown(VK_DOWN) || GetKeyDown('S') || GetKeyDown('E')){
				MoveSelectedReorder(controller, 1);
				return;
			}
			if(GetKeyDown(VK_RETURN) || GetKeyDown(VK_SPACE)){
				controller->m_selectedReorderCard.reset();
				controller->SetStatus("カードを置いた");
				RebuildController(controller);
			}
			return;
		}

		if(GetKeyDown(VK_TAB)){
			const bool reverse = GetKey(VK_SHIFT) || GetKey(VK_LSHIFT) || GetKey(VK_RSHIFT);
			MoveReorderFocus(controller, reverse ? -1 : 1);
			return;
		}
		if(GetKeyDown(VK_LEFT) || GetKeyDown('A') || GetKeyDown(VK_UP) || GetKeyDown('W')){
			MoveReorderFocus(controller, -1);
			return;
		}
		if(GetKeyDown(VK_RIGHT) || GetKeyDown('D') || GetKeyDown(VK_DOWN) || GetKeyDown('S')){
			MoveReorderFocus(controller, 1);
			return;
		}
		if(GetKeyDown(VK_RETURN) || GetKeyDown(VK_SPACE)){
			controller->m_selectedReorderCard = m_reorderFocusIndex;
			controller->SetStatus("カード保持中: 矢印で並べ替え、Enterで置く");
			RebuildController(controller);
		}
	}

	void MoveReorderFocus(Controller* controller, int direction){
		if(controller->m_reorderOrder.empty()) return;
		const std::size_t count = controller->m_reorderOrder.size();
		m_reorderFocusIndex = direction < 0
			? (m_reorderFocusIndex + count - 1) % count
			: (m_reorderFocusIndex + 1) % count;
	}

	void MoveSelectedReorder(Controller* controller, int direction){
		if(!controller->m_selectedReorderCard) return;
		const std::size_t index = *controller->m_selectedReorderCard;
		if(direction < 0 && index == 0){
			controller->SetStatus("すでに左端です");
			return;
		}
		if(direction > 0 && index + 1 >= controller->m_reorderOrder.size()){
			controller->SetStatus("すでに右端です");
			return;
		}
		const std::size_t next = direction < 0 ? index - 1 : index + 1;
		std::swap(controller->m_reorderOrder[index], controller->m_reorderOrder[next]);
		controller->m_selectedReorderCard = next;
		m_reorderFocusIndex = next;
		controller->SetStatus(direction < 0
			? "カードを左へ移動した"
			: "カードを右へ移動した");
		RebuildController(controller);
	}

	void MoveSelectedReorderExtreme(Controller* controller, bool front){
		if(!controller->m_selectedReorderCard || controller->m_reorderOrder.empty()) return;
		const std::size_t index = *controller->m_selectedReorderCard;
		const ElementType value = controller->m_reorderOrder[index];
		controller->m_reorderOrder.erase(
			controller->m_reorderOrder.begin() + static_cast<std::ptrdiff_t>(index));
		if(front){
			controller->m_reorderOrder.insert(controller->m_reorderOrder.begin(), value);
			m_reorderFocusIndex = 0;
		}else{
			controller->m_reorderOrder.push_back(value);
			m_reorderFocusIndex = controller->m_reorderOrder.size() - 1;
		}
		controller->m_selectedReorderCard = m_reorderFocusIndex;
		controller->SetStatus(front ? "カードを先頭へ移動した" : "カードを末尾へ移動した");
		RebuildController(controller);
	}

	// ---------------------------------------------------------------------
	// Persistent keyboard cursor/help overlay
	// ---------------------------------------------------------------------
	void EnsureOverlay(){
		if(m_overlayQueued || !m_lifetime || !m_lifetime->alive) return;
		m_overlayQueued = true;
		QueueOverlayEntity(true);
		QueueOverlayEntity(false);
	}

	void QueueOverlayEntity(bool cursor){
		const CommandEntity entity = QueueCreateEntity();
		QueueAddComponent<NameComponent>(entity);
		QueueAddComponent<TransformComponent>(entity);
		QueueAddComponent<TextureComponent>(entity);
		QueueAddComponent<RuntimeTextComponent>(entity);
		QueueAddComponent<SpriteRendererComponent>(entity);
		QueueAddComponent<RenderLayerComponent>(entity);
		QueueAddComponent<OrderInLayerComponent>(entity);

		const std::weak_ptr<Lifetime> weak = m_lifetime;
		QueueEntitySetup(entity, [weak, cursor](Entity created, SceneContext& context){
			auto lifetime = weak.lock();
			if(!lifetime || !lifetime->alive || !context.component || !context.manager) return;

			if(NameComponent* name = context.component->GetComponent<NameComponent>(created)){
				name->name = cursor ? "KeyboardFocusCursor" : "KeyboardHelp";
			}
			if(TransformComponent* transform = context.component->GetComponent<TransformComponent>(created)){
				transform->position = cursor
					? Vector3(-0.1f, -0.1f, 0.0f)
					: Vector3(0.01f, 0.94f, 0.0f);
				transform->scale = cursor
					? Vector3(0.035f, 0.06f, 1.0f)
					: Vector3(0.98f, 0.05f, 1.0f);
			}
			if(SpriteRendererComponent* sprite = context.component->GetComponent<SpriteRendererComponent>(created)){
				sprite->anchor = Vector2(0.0f, 0.0f);
				sprite->pivot = Vector2(0.0f, 0.0f);
			}
			if(RuntimeTextComponent* text = context.component->GetComponent<RuntimeTextComponent>(created)){
				text->Text = cursor ? "▶" : "";
				text->FontSize = cursor ? 25.0f : 15.0f;
				text->PixelWidth = cursor
					? 48
					: std::max(1, static_cast<int>(context.manager->PlayerScreenSize.x * 0.98f));
				text->PixelHeight = cursor ? 48 : 38;
				text->ColorR = cursor ? 1.0f : 0.72f;
				text->ColorG = cursor ? 0.80f : 0.84f;
				text->ColorB = cursor ? 0.26f : 0.96f;
				text->ColorA = 1.0f;
				text->Horizontal = RuntimeTextComponent::HorizontalAlignment::Center;
				text->Vertical = RuntimeTextComponent::VerticalAlignment::Center;
				text->WordWrap = false;
				text->AutoSizeTransform = true;
				text->MarkDirty();
			}
			if(RenderLayerComponent* layer = context.component->GetComponent<RenderLayerComponent>(created)){
				layer->layer = RenderLayer::OverlayUI;
			}
			if(OrderInLayerComponent* order = context.component->GetComponent<OrderInLayerComponent>(created)){
				order->order = cursor ? 280 : 270;
			}
			if(cursor) lifetime->cursor = EntityRef(created, &context);
			else lifetime->help = EntityRef(created, &context);
		});
	}

	void UpdateCursor(Controller* controller){
		if(!m_lifetime || !m_lifetime->cursor.IsValid()) return;
		SceneContext* context = m_lifetime->cursor.GetScene();
		if(!context || !context->component) return;
		TransformComponent* transform =
			context->component->GetComponent<TransformComponent>(m_lifetime->cursor.GetEntityID());
		RuntimeTextComponent* text =
			context->component->GetComponent<RuntimeTextComponent>(m_lifetime->cursor.GetEntityID());
		if(!transform || !text) return;

		float pixelX = -100.0f;
		float pixelY = -100.0f;
		std::string symbol = "▶";
		bool visible = true;

		if(IsTurnHandoff(controller)){
			visible = GenericCursorPosition(controller, pixelX, pixelY);
		}else{
			switch(controller->m_flow.Screen()){
			case FlowScreen::DeckSetupPlayerOne:
			case FlowScreen::DeckSetupPlayerTwo:
				DeckCursorPosition(controller, pixelX, pixelY);
				symbol = controller->m_selectedDeckCard ? "◆" : "▶";
				break;
			case FlowScreen::BattleBoard:
				{
					const ScreenPoint center = controller->m_boardLayout.CellCenter(m_boardFocusCell);
					pixelX = center.x - 58.0f;
					pixelY = center.y - 24.0f;
				}
				break;
			case FlowScreen::CenterReorder:
				ReorderCursorPosition(controller, pixelX, pixelY);
				symbol = controller->m_selectedReorderCard ? "◆" : "▶";
				break;
			default:
				visible = GenericCursorPosition(controller, pixelX, pixelY);
				break;
			}
		}

		if(!visible){
			if(!text->Text.empty()){
				text->Text.clear();
				text->MarkDirty();
			}
			return;
		}
		if(text->Text != symbol){
			text->Text = std::move(symbol);
			text->MarkDirty();
		}
		const float width = std::max(1.0f, controller->ViewWidth());
		const float height = std::max(1.0f, controller->ViewHeight());
		transform->position = Vector3(pixelX / width, pixelY / height, 0.0f);
	}

	bool GenericCursorPosition(Controller* controller, float& x, float& y){
		if(controller->m_buttons.empty()) return false;
		ClampGenericFocus(controller);
		const ScreenRect& rect = controller->m_buttons[m_focusIndex].rect;
		x = rect.x - 36.0f;
		if(x < 2.0f) x = rect.x + 2.0f;
		y = rect.y + rect.height * 0.5f - 22.0f;
		return true;
	}

	void DeckCursorPosition(Controller* controller, float& x, float& y){
		ClampDeckFocus(controller);
		const auto& decks = controller->m_flow.EditingDeck().Decks();
		const float columnWidth =
			std::min(300.0f, (controller->ViewWidth() - 160.0f) / 3.0f);
		const float gap =
			(controller->ViewWidth() - columnWidth * 3.0f) / 4.0f;
		x = gap + (columnWidth + gap) * static_cast<float>(m_deckFocusSlot) - 20.0f;
		y = decks[m_deckFocusSlot].empty()
			? 202.0f
			: 178.0f + static_cast<float>(m_deckFocusIndex) * 45.0f;
	}

	void ReorderCursorPosition(Controller* controller, float& x, float& y){
		ClampReorderFocus(controller);
		if(controller->m_reorderOrder.empty()){
			x = -100.0f;
			y = -100.0f;
			return;
		}
		const float cardWidth = 118.0f;
		const float totalWidth =
			cardWidth * static_cast<float>(controller->m_reorderOrder.size());
		const float startX = controller->ViewWidth() * 0.5f - totalWidth * 0.5f;
		x = startX + cardWidth * static_cast<float>(m_reorderFocusIndex) - 32.0f;
		y = 300.0f;
	}

	void UpdateHelp(Controller* controller){
		if(!m_lifetime || !m_lifetime->help.IsValid()) return;
		SceneContext* context = m_lifetime->help.GetScene();
		if(!context || !context->component || !context->manager) return;
		RuntimeTextComponent* text =
			context->component->GetComponent<RuntimeTextComponent>(m_lifetime->help.GetEntityID());
		if(!text) return;

		std::string value;
		if(IsTurnHandoff(controller)){
			value = "Enter / Space: 次のプレイヤーへ続行";
		}else if(IsDeckScreen(controller->m_flow.Screen())){
			value = controller->m_selectedDeckCard
				? "カード保持中  ←/→: 駒移動  ↑/↓: 順番変更  1/2/3: 駒指定  Home/End: 先頭/末尾  Enter/Backspace: 置く  C: 確定"
				: "矢印/WASD: カード移動  Enter: カードを持つ  1/2/3: 駒列へ  B: 3・3・2  X: 8・0・0  C: 確定  Backspace: 戻る";
		}else{
			switch(controller->m_flow.Screen()){
			case FlowScreen::Title:
				value = "Enter / Space: はじめる";
				break;
			case FlowScreen::ModeSelect:
				value = "矢印/WASD: 選択  Enter: 決定  1: CPU  2: ローカル  3: ルール  Backspace: タイトル";
				break;
			case FlowScreen::Rules:
				value = "Enter / Backspace: 戻る";
				break;
			case FlowScreen::LocalPrivacyHandoff:
				value = "Enter / Space: PLAYER 2へ続行  Backspace: PLAYER 1の編成へ戻る";
				break;
			case FlowScreen::MatchIntroduction:
				value = "Enter / Space: 対戦開始  Backspace: デッキ編成へ戻る";
				break;
			case FlowScreen::BattleBoard:
				value = controller->IsHumanTurn()
					? "矢印/WASD: 盤面セル移動  Enter: 選択/実行  1: 移動・戦闘  2: 偵察  Backspace: 選択解除"
					: "CPUの手番。入力待機中";
				break;
			case FlowScreen::CenterReorder:
				value = controller->m_selectedReorderCard
					? "カード保持中  矢印/Q/E: 並べ替え  Home/End: 先頭/末尾  Enter/Backspace: 置く  C: 確定  X: 再編しない"
					: "矢印/WASD: カード移動  Enter: カードを持つ  C: 確定  X/Backspace: 再編しない";
				break;
			case FlowScreen::Result:
				value = "矢印/WASD: 選択  Enter: 決定  R: 再戦  T/Backspace: タイトル";
				break;
			default:
				value = "矢印/WASD: 選択  Enter/Space: 決定  Tab: 順送り  Backspace: 戻る";
				break;
			}
		}

		const int desiredWidth = std::max(
			1, static_cast<int>(context->manager->PlayerScreenSize.x * 0.98f));
		if(text->Text != value || text->PixelWidth != desiredWidth){
			text->Text = std::move(value);
			text->PixelWidth = desiredWidth;
			text->MarkDirty();
		}
	}

	void DestroyOverlayEntity(EntityRef& entity){
		if(entity.IsValid()) QueueDestroyEntity(entity.GetEntityID());
		entity = {};
	}

	std::shared_ptr<Lifetime> m_lifetime;
	FlowScreen m_screen = FlowScreen::Title;
	std::size_t m_focusIndex = 0;
	std::size_t m_deckFocusSlot = 0;
	std::size_t m_deckFocusIndex = 0;
	std::size_t m_reorderFocusIndex = 0;
	int m_boardFocusCell = 0;
	bool m_hasContext = false;
	bool m_handoff = false;
	bool m_overlayQueued = false;
};

} // namespace ElemenTactics
