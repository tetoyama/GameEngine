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
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace ElemenTactics {

// Owns all player input after the controller has produced a screen. The
// controller's original button list is immediately detached, which prevents
// its held-button mouse path from interpreting one click for several frames.
// Deck setup is replaced by a direct manipulation layout with click-to-move,
// drag-and-drop and explicit order controls.
class ElemenTacticsKeyboardNavigator final : public CustomScriptComponent {
public:
	ElemenTacticsKeyboardNavigator(){
		scriptName = "ElemenTacticsKeyboardNavigator";
		SetExecutionOrder(
			SystemTaskDomain::Frame,
			SystemPhase::Default,
			100
		);
	}

	void OnStart() override{
		m_lifetime = std::make_shared<NavigatorLifetime>();
		m_buttons.clear();
		m_focusIndex = 0;
		m_hasLastScreen = false;
		m_pendingDeckRebuild = false;
		m_deckUiActive = false;
		m_pointerWasDown = false;
		m_hasLastPointer = false;
		m_dragSource.reset();
		EnsurePersistentUi();
	}

	void OnUpdate(float) override{
		ElemenTacticsGameController* controller = ResolveController();
		if(!controller || !controller->m_started) return;

		EnsurePersistentUi();
		const FlowScreen screen = controller->m_flow.Screen();
		if(!m_hasLastScreen || screen != m_lastScreen){
			m_lastScreen = screen;
			m_hasLastScreen = true;
			m_focusIndex = 0;
			m_dragSource.reset();
			if(!IsDeckScreen(screen)){
				m_pendingDeckRebuild = false;
				m_deckUiActive = false;
			}
		}

		bool capturedControllerUi = false;
		if(!controller->m_buttons.empty()){
			capturedControllerUi = true;
			AdoptControllerUi(controller);
		}

		// Wait one frame after the controller queued its original deck UI. This
		// lets those deferred entities commit before ClearUi destroys them, rather
		// than leaving unconfigured command-buffer entities behind.
		if(m_pendingDeckRebuild && !capturedControllerUi && IsDeckScreen(screen)){
			RebuildDirectDeckUi(controller);
			m_pendingDeckRebuild = false;
		}

		HandleInput(controller);
		UpdatePersistentUi(controller);
		if(controller->m_textSystem){
			controller->m_textSystem->ProcessDirtyText();
		}
	}

	void OnStop() override{
		m_buttons.clear();
		m_dragSource.reset();
		if(m_lifetime){
			m_lifetime->alive = false;
			DestroyPersistentEntity(m_lifetime->help);
			DestroyPersistentEntity(m_lifetime->focus);
		}
		m_persistentRequested = false;
	}

private:
	using Controller = ElemenTacticsGameController;
	using Command = Controller::UiCommand;
	using Button = Controller::UiButton;

	struct NavigatorLifetime {
		bool alive = true;
		EntityRef help;
		EntityRef focus;
	};

	static bool IsDeckScreen(FlowScreen screen) noexcept{
		return screen == FlowScreen::DeckSetupPlayerOne ||
			screen == FlowScreen::DeckSetupPlayerTwo;
	}

	Controller* ResolveController() const{
		const EntityRef self = GetEntityRef();
		SceneContext* context = self.GetScene();
		if(!self.IsValid() || !context || !context->component) return nullptr;
		return context->component->GetComponent<Controller>(self.GetEntityID());
	}

	void AdoptControllerUi(Controller* controller){
		const FlowScreen screen = controller->m_flow.Screen();
		m_buttons = std::move(controller->m_buttons);
		controller->m_buttons.clear();

		if(IsDeckScreen(screen)){
			// The old deck screen is deliberately non-interactive during the one
			// frame before the direct manipulation replacement is committed.
			m_buttons.clear();
			m_pendingDeckRebuild = true;
			m_deckUiActive = false;
			return;
		}

		m_pendingDeckRebuild = false;
		m_deckUiActive = false;
		if(m_focusIndex >= m_buttons.size()) m_focusIndex = 0;
	}

	void CaptureDirectButtons(Controller* controller){
		m_buttons = std::move(controller->m_buttons);
		controller->m_buttons.clear();
		if(m_buttons.empty()){
			m_focusIndex = 0;
			return;
		}

		if(controller->m_selectedDeckCard){
			for(std::size_t index = 0; index < m_buttons.size(); ++index){
				const Button& button = m_buttons[index];
				if(button.command != Command::DeckCard) continue;
				if(button.primary == static_cast<int>(controller->m_selectedDeckCard->first) &&
					button.secondary == static_cast<int>(controller->m_selectedDeckCard->second)){
					m_focusIndex = index;
					return;
				}
			}
		}
		if(m_focusIndex >= m_buttons.size()) m_focusIndex = 0;
	}

	void RebuildDirectDeckUi(Controller* controller){
		if(!controller || !IsDeckScreen(controller->m_flow.Screen())) return;
		controller->ClearUi();
		BuildDirectDeckUi(controller);
		CaptureDirectButtons(controller);
		controller->m_screenDirty = false;
		m_deckUiActive = true;
	}

	void BuildDirectDeckUi(Controller* controller){
		const bool playerTwo = controller->m_flow.Screen() == FlowScreen::DeckSetupPlayerTwo;
		const float width = controller->ViewWidth();
		const float height = controller->ViewHeight();
		const auto& decks = controller->m_flow.EditingDeck().Decks();

		controller->CreateText(
			"DeckTitle",
			std::string(playerTwo ? "PLAYER 2" : "PLAYER 1") + "  デッキ編成",
			32.0f, 14.0f, width - 64.0f, 52.0f, 39.0f,
			0.95f, 0.86f, 0.42f, 1.0f, 40, std::nullopt, true);
		controller->CreateText(
			"DeckDirectHelp",
			"カードをドラッグして駒と順番を変更  ／  クリック選択後、駒の見出しや下の操作ボタンでも変更",
			32.0f, 66.0f, width - 64.0f, 34.0f, 20.0f,
			0.82f, 0.90f, 1.0f, 1.0f, 40, std::nullopt, true);
		controller->CreateText(
			"DeckRuleHint",
			"各列の一番上が先頭カード。戦闘・偵察では先頭から使用される。0枚の駒は盤面へ出ない。",
			32.0f, 101.0f, width - 64.0f, 31.0f, 18.0f,
			0.68f, 0.76f, 0.90f, 1.0f, 40, std::nullopt, true);

		const float side = 24.0f;
		const float gap = 16.0f;
		const float columnWidth = (width - side * 2.0f - gap * 2.0f) / 3.0f;
		const float headerY = 140.0f;
		const float headerHeight = 48.0f;
		m_deckCardTop = 198.0f;
		m_deckRowHeight = 42.0f;
		const float deckBottom = std::min(height - 184.0f, m_deckCardTop + 8.0f * m_deckRowHeight);

		for(std::size_t slot = 0; slot < decks.size(); ++slot){
			const float x = side + static_cast<float>(slot) * (columnWidth + gap);
			m_deckDropZones[slot] = ScreenRect{
				x,
				headerY,
				columnWidth,
				std::max(headerHeight, deckBottom - headerY)
			};

			std::string header = "駒 " + std::to_string(slot + 1) +
				"  |  " + std::to_string(decks[slot].size()) + "枚";
			if(controller->m_selectedDeckCard){
				header += controller->m_selectedDeckCard->first == slot
					? "  |  選択元"
					: "  |  ここへ移す";
			}
			const ScreenRect headerRect{x, headerY, columnWidth, headerHeight};
			controller->CreateText(
				"DeckPieceHeader", header,
				x, headerY, columnWidth, headerHeight, 23.0f,
				controller->m_selectedDeckCard && controller->m_selectedDeckCard->first != slot ? 1.0f : 0.76f,
				controller->m_selectedDeckCard && controller->m_selectedDeckCard->first != slot ? 0.82f : 0.88f,
				controller->m_selectedDeckCard && controller->m_selectedDeckCard->first != slot ? 0.32f : 1.0f,
				1.0f, 55,
				Button{headerRect, Command::DeckMoveToSlot, static_cast<int>(slot), -1}, true);

			if(decks[slot].empty()){
				const ScreenRect emptyRect{x + 10.0f, m_deckCardTop + 36.0f,
					columnWidth - 20.0f, 92.0f};
				controller->CreateText(
					"DeckEmptyDrop", "カードなし\nここへドラッグ／クリックで移動",
					emptyRect.x, emptyRect.y, emptyRect.width, emptyRect.height, 20.0f,
					0.52f, 0.60f, 0.72f, 1.0f, 45,
					Button{emptyRect, Command::DeckMoveToSlot, static_cast<int>(slot), -1}, true);
			}

			for(std::size_t index = 0; index < decks[slot].size(); ++index){
				const ElementType element = decks[slot][index];
				const auto color = controller->ElementColor(element);
				const bool selected = controller->m_selectedDeckCard &&
					controller->m_selectedDeckCard->first == slot &&
					controller->m_selectedDeckCard->second == index;
				std::string label = selected ? "▶ " : "   ";
				label += index == 0 ? "[先頭] " : ("[" + std::to_string(index + 1) + "] ");
				label += controller->ElementSymbol(element) + "  " + controller->ElementLabel(element);
				const ScreenRect cardRect{
					x + 9.0f,
					m_deckCardTop + static_cast<float>(index) * m_deckRowHeight,
					columnWidth - 18.0f,
					m_deckRowHeight - 4.0f
				};
				controller->CreateText(
					"DeckCard", label,
					cardRect.x, cardRect.y, cardRect.width, cardRect.height, 21.0f,
					selected ? 1.0f : color[0],
					selected ? 0.86f : color[1],
					selected ? 0.36f : color[2],
					1.0f, 60,
					Button{cardRect, Command::DeckCard,
						static_cast<int>(slot), static_cast<int>(index)}, true);
			}
		}

		const float selectedY = height - 174.0f;
		std::string selection = "選択中: なし  —  カードをクリック、またはドラッグしてください";
		if(controller->m_selectedDeckCard){
			const auto [slot, index] = *controller->m_selectedDeckCard;
			if(slot < decks.size() && index < decks[slot].size()){
				selection = "選択中: 駒 " + std::to_string(slot + 1) +
					" / " + std::to_string(index + 1) + "番 / " +
					controller->ElementSymbol(decks[slot][index]) + " " +
					controller->ElementLabel(decks[slot][index]);
			}
		}
		controller->CreateText(
			"DeckSelection", selection,
			24.0f, selectedY, width - 48.0f, 31.0f, 20.0f,
			0.96f, 0.78f, 0.42f, 1.0f, 70, std::nullopt, true);

		const float actionY = height - 138.0f;
		float actionX = 24.0f;
		auto actionButton = [&](const std::string& text, float buttonWidth, Command command, int primary = -1){
			controller->CreateButton(
				text,
				ScreenRect{actionX, actionY, buttonWidth, 40.0f},
				command, primary, -1, 19.0f);
			actionX += buttonWidth + 8.0f;
		};
		actionButton("↑ 前へ", 104.0f, Command::DeckMoveUp);
		actionButton("↓ 後ろへ", 104.0f, Command::DeckMoveDown);
		actionButton("⇈ 先頭", 104.0f, Command::DeckMoveTop);
		actionButton("⇊ 末尾", 104.0f, Command::DeckMoveBottom);
		actionButton("駒1へ", 96.0f, Command::DeckMoveToSlot, 0);
		actionButton("駒2へ", 96.0f, Command::DeckMoveToSlot, 1);
		actionButton("駒3へ", 96.0f, Command::DeckMoveToSlot, 2);
		actionButton("選択解除", 118.0f, Command::DeckClearSelection);

		const float footerY = height - 88.0f;
		controller->CreateButton(
			"おすすめ 3・3・2",
			ScreenRect{24.0f, footerY, 205.0f, 45.0f},
			Command::DeckBalanced, -1, -1, 20.0f);
		controller->CreateButton(
			"一点集中 8・0・0",
			ScreenRect{242.0f, footerY, 205.0f, 45.0f},
			Command::DeckConcentrated, -1, -1, 20.0f);

		const std::string status = controller->m_status.empty()
			? "合計8枚。光♛と闇†を失うと即敗北。"
			: controller->m_status;
		controller->CreateText(
			"DeckStatus", status,
			465.0f, footerY + 4.0f,
			std::max(180.0f, width - 465.0f - 292.0f), 38.0f, 17.0f,
			0.88f, 0.78f, 0.56f, 1.0f, 70, std::nullopt, true);
		controller->CreateButton(
			"この編成で開始",
			ScreenRect{width - 274.0f, footerY - 2.0f, 250.0f, 51.0f},
			Command::DeckConfirm, -1, -1, 25.0f);
	}

	void HandleInput(Controller* controller){
		SceneContext* context = GetEntityRef().GetScene();
		if(!context || !context->manager || !context->manager->input ||
			!context->manager->hwnd){
			return;
		}

		InputService* input = context->manager->input;
		const HWND hwnd = context->manager->hwnd;
		const ScreenPoint pointer{
			static_cast<float>(input->GetMouseX()),
			static_cast<float>(input->GetMouseY())
		};
		SyncFocusFromPointer(pointer);

		const bool pointerDown = input->IsMouseDown(hwnd, 0);
		const bool pointerPressed = input->IsMouse(hwnd, 0);
		bool consumed = false;

		if(pointerPressed){
			const std::optional<std::size_t> hit = FindButtonAt(pointer);
			if(hit){
				m_focusIndex = *hit;
				const Button button = m_buttons[*hit];
				if(button.command == Command::DeckCard){
					m_dragSource = std::pair<std::size_t, std::size_t>{
						static_cast<std::size_t>(button.primary),
						static_cast<std::size_t>(button.secondary)};
					m_pointerPressPoint = pointer;
				} else {
					m_dragSource.reset();
				}
				ExecuteButton(controller, button);
				consumed = true;
			}
		}

		if(m_pointerWasDown && !pointerDown && m_dragSource){
			const float dx = pointer.x - m_pointerPressPoint.x;
			const float dy = pointer.y - m_pointerPressPoint.y;
			if(dx * dx + dy * dy >= 64.0f){
				DropDeckCard(controller, pointer, *m_dragSource);
				consumed = true;
			}
			m_dragSource.reset();
		}
		m_pointerWasDown = pointerDown;

		if(consumed || m_buttons.empty()) return;
		HandleKeyboard(controller);
	}

	void HandleKeyboard(Controller* controller){
		if(HandleQuickShortcut(controller)) return;

		if(GetKeyDown(VK_TAB)){
			const bool reverse = GetKey(VK_SHIFT) || GetKey(VK_LSHIFT) || GetKey(VK_RSHIFT);
			MoveSequential(reverse ? -1 : 1);
		}
		if(GetKeyDown(VK_UP) || GetKeyDown('W')) MoveSpatial(0.0f, -1.0f);
		if(GetKeyDown(VK_DOWN) || GetKeyDown('S')) MoveSpatial(0.0f, 1.0f);
		if(GetKeyDown(VK_LEFT) || GetKeyDown('A')) MoveSpatial(-1.0f, 0.0f);
		if(GetKeyDown(VK_RIGHT) || GetKeyDown('D')) MoveSpatial(1.0f, 0.0f);

		if(GetKeyDown(VK_ESCAPE)){
			if(ActivateEscape(controller)) return;
		}
		if(GetKeyDown(VK_RETURN) || GetKeyDown(VK_SPACE)){
			ActivateFocused(controller);
		}
	}

	bool HandleQuickShortcut(Controller* controller){
		const FlowScreen screen = controller->m_flow.Screen();
		if(IsDeckScreen(screen)){
			if(GetKeyDown('1')) return ExecuteCustomDeckCommand(
				controller, Button{{}, Command::DeckMoveToSlot, 0, -1});
			if(GetKeyDown('2')) return ExecuteCustomDeckCommand(
				controller, Button{{}, Command::DeckMoveToSlot, 1, -1});
			if(GetKeyDown('3')) return ExecuteCustomDeckCommand(
				controller, Button{{}, Command::DeckMoveToSlot, 2, -1});
			if(GetKeyDown(VK_PRIOR)) return ExecuteCustomDeckCommand(
				controller, Button{{}, Command::DeckMoveUp, -1, -1});
			if(GetKeyDown(VK_NEXT)) return ExecuteCustomDeckCommand(
				controller, Button{{}, Command::DeckMoveDown, -1, -1});
			if(GetKeyDown(VK_HOME)) return ExecuteCustomDeckCommand(
				controller, Button{{}, Command::DeckMoveTop, -1, -1});
			if(GetKeyDown(VK_END)) return ExecuteCustomDeckCommand(
				controller, Button{{}, Command::DeckMoveBottom, -1, -1});
			if(GetKeyDown(VK_DELETE)) return ExecuteCustomDeckCommand(
				controller, Button{{}, Command::DeckClearSelection, -1, -1});
			return false;
		}

		switch(screen){
		case FlowScreen::ModeSelect:
			if(GetKeyDown('1')) return ActivateCommand(controller, Command::ModeLlm);
			if(GetKeyDown('2')) return ActivateCommand(controller, Command::ModeLocal);
			if(GetKeyDown('3')) return ActivateCommand(controller, Command::OpenRules);
			break;
		case FlowScreen::BattleBoard:
			if(GetKeyDown('1')) return ActivateCommand(controller, Command::BattleMoveMode);
			if(GetKeyDown('2')) return ActivateCommand(controller, Command::BattleScoutMode);
			break;
		default:
			break;
		}
		return false;
	}

	void ExecuteButton(Controller* controller, const Button& button){
		if(IsDeckScreen(controller->m_flow.Screen()) && ExecuteCustomDeckCommand(controller, button)){
			return;
		}

		const FlowScreen before = controller->m_flow.Screen();
		controller->HandleCommand(button);
		if(controller->m_screenDirty){
			controller->RebuildScreen();
		}
		if(!controller->m_buttons.empty()){
			AdoptControllerUi(controller);
		}
		if(controller->m_flow.Screen() != before){
			m_focusIndex = 0;
			m_lastScreen = controller->m_flow.Screen();
			m_hasLastScreen = true;
		}
	}

	bool ExecuteCustomDeckCommand(Controller* controller, const Button& button){
		if(!IsDeckScreen(controller->m_flow.Screen())) return false;
		switch(button.command){
		case Command::DeckCard:
			SelectDeckCard(controller, button.primary, button.secondary);
			return true;
		case Command::DeckMoveLeft:
			MoveSelectedRelativeSlot(controller, -1);
			return true;
		case Command::DeckMoveRight:
			MoveSelectedRelativeSlot(controller, 1);
			return true;
		case Command::DeckMoveUp:
			MoveSelectedOrder(controller, -1);
			return true;
		case Command::DeckMoveDown:
			MoveSelectedOrder(controller, 1);
			return true;
		case Command::DeckMoveToSlot:
			MoveSelectedToSlot(controller, button.primary);
			return true;
		case Command::DeckMoveTop:
			MoveSelectedExtreme(controller, true);
			return true;
		case Command::DeckMoveBottom:
			MoveSelectedExtreme(controller, false);
			return true;
		case Command::DeckClearSelection:
			controller->m_selectedDeckCard.reset();
			controller->SetStatus("カード選択を解除した");
			RebuildDirectDeckUi(controller);
			return true;
		case Command::DeckBalanced:
			controller->m_flow.EditingDeck() = DeckSetupModel::BalancedDefault();
			controller->m_selectedDeckCard.reset();
			controller->SetStatus("おすすめの3・3・2へ変更した");
			RebuildDirectDeckUi(controller);
			return true;
		case Command::DeckConcentrated:
			controller->m_flow.EditingDeck() = DeckSetupModel::ConcentratedDefault();
			controller->m_selectedDeckCard.reset();
			controller->SetStatus("8・0・0へ変更した。空の駒は盤面へ出ない");
			RebuildDirectDeckUi(controller);
			return true;
		case Command::DeckConfirm:
			ConfirmDeck(controller);
			return true;
		default:
			return false;
		}
	}

	void SelectDeckCard(Controller* controller, int slotValue, int indexValue){
		if(slotValue < 0 || indexValue < 0) return;
		const std::size_t slot = static_cast<std::size_t>(slotValue);
		const std::size_t index = static_cast<std::size_t>(indexValue);
		const auto& decks = controller->m_flow.EditingDeck().Decks();
		if(slot >= decks.size() || index >= decks[slot].size()) return;
		controller->m_selectedDeckCard = std::pair<std::size_t, std::size_t>{slot, index};
		controller->SetStatus(
			"カードを選択。ドラッグするか、駒見出し・下の操作ボタンを選んでください");
		RebuildDirectDeckUi(controller);
	}

	bool ResolveSelectedCard(
		Controller* controller,
		std::size_t& slot,
		std::size_t& index
	){
		if(!controller->m_selectedDeckCard){
			controller->SetStatus("先にカードを選択してください");
			RebuildDirectDeckUi(controller);
			return false;
		}
		slot = controller->m_selectedDeckCard->first;
		index = controller->m_selectedDeckCard->second;
		const auto& decks = controller->m_flow.EditingDeck().Decks();
		if(slot >= decks.size() || index >= decks[slot].size()){
			controller->m_selectedDeckCard.reset();
			controller->SetStatus("選択状態をリセットした");
			RebuildDirectDeckUi(controller);
			return false;
		}
		return true;
	}

	void MoveSelectedRelativeSlot(Controller* controller, int direction){
		std::size_t slot = 0;
		std::size_t index = 0;
		if(!ResolveSelectedCard(controller, slot, index)) return;
		const int destination = static_cast<int>(slot) + direction;
		MoveSelectedToSlot(controller, destination);
	}

	void MoveSelectedToSlot(Controller* controller, int destinationValue){
		std::size_t sourceSlot = 0;
		std::size_t sourceIndex = 0;
		if(!ResolveSelectedCard(controller, sourceSlot, sourceIndex)) return;
		if(destinationValue < 0 || destinationValue >= 3){
			controller->SetStatus("その方向には駒がありません");
			RebuildDirectDeckUi(controller);
			return;
		}
		const std::size_t destinationSlot = static_cast<std::size_t>(destinationValue);
		if(destinationSlot == sourceSlot){
			controller->SetStatus("そのカードはすでに駒 " + std::to_string(sourceSlot + 1) + " にあります");
			RebuildDirectDeckUi(controller);
			return;
		}

		std::string error;
		const std::size_t destinationIndex =
			controller->m_flow.EditingDeck().Decks()[destinationSlot].size();
		if(controller->m_flow.EditingDeck().MoveCard(
			sourceSlot, sourceIndex, destinationSlot, destinationIndex, &error)){
			controller->m_selectedDeckCard =
				std::pair<std::size_t, std::size_t>{destinationSlot, destinationIndex};
			controller->SetStatus("カードを駒 " + std::to_string(destinationSlot + 1) + " の末尾へ移動した");
		} else {
			controller->SetStatus(error.empty() ? "カードを移動できない" : error);
		}
		RebuildDirectDeckUi(controller);
	}

	void MoveSelectedOrder(Controller* controller, int direction){
		std::size_t slot = 0;
		std::size_t index = 0;
		if(!ResolveSelectedCard(controller, slot, index)) return;
		const std::size_t size = controller->m_flow.EditingDeck().Decks()[slot].size();
		if(direction < 0 && index == 0){
			controller->SetStatus("すでに先頭カードです");
			RebuildDirectDeckUi(controller);
			return;
		}
		if(direction > 0 && index + 1 >= size){
			controller->SetStatus("すでに末尾カードです");
			RebuildDirectDeckUi(controller);
			return;
		}

		const std::size_t destination = direction < 0 ? index - 1 : index + 2;
		std::string error;
		if(controller->m_flow.EditingDeck().ReorderCard(slot, index, destination, &error)){
			controller->m_selectedDeckCard = std::pair<std::size_t, std::size_t>{
				slot,
				direction < 0 ? index - 1 : index + 1
			};
			controller->SetStatus(direction < 0
				? "カードを1つ前へ移動した"
				: "カードを1つ後ろへ移動した");
		} else {
			controller->SetStatus(error.empty() ? "順番を変更できない" : error);
		}
		RebuildDirectDeckUi(controller);
	}

	void MoveSelectedExtreme(Controller* controller, bool toFront){
		std::size_t slot = 0;
		std::size_t index = 0;
		if(!ResolveSelectedCard(controller, slot, index)) return;
		const std::size_t size = controller->m_flow.EditingDeck().Decks()[slot].size();
		const std::size_t destination = toFront ? 0 : size;
		std::string error;
		if(controller->m_flow.EditingDeck().ReorderCard(slot, index, destination, &error)){
			controller->m_selectedDeckCard = std::pair<std::size_t, std::size_t>{
				slot,
				toFront ? 0 : size - 1
			};
			controller->SetStatus(toFront
				? "カードを先頭へ移動した"
				: "カードを末尾へ移動した");
		} else {
			controller->SetStatus(error.empty() ? "順番を変更できない" : error);
		}
		RebuildDirectDeckUi(controller);
	}

	void ConfirmDeck(Controller* controller){
		std::string error;
		if(!controller->m_flow.ConfirmCurrentDeck(&error)){
			controller->SetStatus(error.empty() ? "編成を確定できない" : error);
			RebuildDirectDeckUi(controller);
			return;
		}

		controller->m_selectedDeckCard.reset();
		controller->SetStatus({});
		controller->m_screenDirty = true;
		controller->RebuildScreen();
		if(!controller->m_buttons.empty()){
			AdoptControllerUi(controller);
		}
		m_focusIndex = 0;
		m_lastScreen = controller->m_flow.Screen();
	}

	void DropDeckCard(
		Controller* controller,
		ScreenPoint pointer,
		std::pair<std::size_t, std::size_t> source
	){
		if(!m_deckUiActive || !IsDeckScreen(controller->m_flow.Screen())) return;
		std::optional<std::size_t> destinationSlot;
		for(std::size_t slot = 0; slot < m_deckDropZones.size(); ++slot){
			if(m_deckDropZones[slot].Contains(pointer)){
				destinationSlot = slot;
				break;
			}
		}
		if(!destinationSlot){
			controller->SetStatus("ドロップ先が駒の列から外れています");
			RebuildDirectDeckUi(controller);
			return;
		}

		const auto& decks = controller->m_flow.EditingDeck().Decks();
		if(source.first >= decks.size() || source.second >= decks[source.first].size()) return;
		const std::size_t destinationSize = decks[*destinationSlot].size();
		const float rawIndex = (pointer.y - m_deckCardTop) / m_deckRowHeight + 0.5f;
		const int roundedIndex = static_cast<int>(std::floor(rawIndex));
		const std::size_t destinationIndex = static_cast<std::size_t>(std::clamp(
			roundedIndex,
			0,
			static_cast<int>(destinationSize)
		));

		std::string error;
		if(!controller->m_flow.EditingDeck().MoveCard(
			source.first,
			source.second,
			*destinationSlot,
			destinationIndex,
			&error)){
			controller->SetStatus(error.empty() ? "ドロップ位置へ移動できない" : error);
			RebuildDirectDeckUi(controller);
			return;
		}

		std::size_t selectedIndex = destinationIndex;
		if(source.first == *destinationSlot){
			if(destinationIndex == source.second || destinationIndex == source.second + 1){
				selectedIndex = source.second;
			} else if(destinationIndex > source.second){
				selectedIndex = destinationIndex - 1;
			}
		}
		const std::size_t newSize = controller->m_flow.EditingDeck().Decks()[*destinationSlot].size();
		if(newSize > 0) selectedIndex = std::min(selectedIndex, newSize - 1);
		controller->m_selectedDeckCard =
			std::pair<std::size_t, std::size_t>{*destinationSlot, selectedIndex};
		controller->SetStatus(
			"ドラッグで駒 " + std::to_string(*destinationSlot + 1) +
			" の " + std::to_string(selectedIndex + 1) + "番へ移動した");
		RebuildDirectDeckUi(controller);
	}

	std::optional<std::size_t> FindButtonAt(ScreenPoint point) const{
		for(std::size_t reverse = m_buttons.size(); reverse > 0; --reverse){
			const std::size_t index = reverse - 1;
			if(m_buttons[index].rect.Contains(point)) return index;
		}
		return std::nullopt;
	}

	static ScreenPoint CenterOf(const ScreenRect& rect) noexcept{
		return ScreenPoint{
			rect.x + rect.width * 0.5f,
			rect.y + rect.height * 0.5f
		};
	}

	void MoveSequential(int direction){
		const std::size_t count = m_buttons.size();
		if(count == 0) return;
		m_focusIndex = direction < 0
			? (m_focusIndex + count - 1) % count
			: (m_focusIndex + 1) % count;
	}

	void MoveSpatial(float directionX, float directionY){
		if(m_buttons.empty()) return;
		if(m_focusIndex >= m_buttons.size()) m_focusIndex = 0;
		const ScreenPoint origin = CenterOf(m_buttons[m_focusIndex].rect);
		std::size_t bestIndex = m_focusIndex;
		float bestScore = std::numeric_limits<float>::max();

		for(std::size_t index = 0; index < m_buttons.size(); ++index){
			if(index == m_focusIndex) continue;
			const ScreenPoint candidate = CenterOf(m_buttons[index].rect);
			const float offsetX = candidate.x - origin.x;
			const float offsetY = candidate.y - origin.y;
			const float forward = offsetX * directionX + offsetY * directionY;
			if(forward <= 1.0f) continue;
			const float lateral = std::abs(offsetX * directionY - offsetY * directionX);
			const float score = forward + lateral * 2.8f;
			if(score < bestScore){
				bestScore = score;
				bestIndex = index;
			}
		}

		if(bestIndex == m_focusIndex){
			MoveSequential(directionX < 0.0f || directionY < 0.0f ? -1 : 1);
		} else {
			m_focusIndex = bestIndex;
		}
	}

	void SyncFocusFromPointer(ScreenPoint pointer){
		if(m_hasLastPointer){
			const float dx = pointer.x - m_lastPointer.x;
			const float dy = pointer.y - m_lastPointer.y;
			if(dx * dx + dy * dy < 1.0f) return;
		}
		m_lastPointer = pointer;
		m_hasLastPointer = true;
		const std::optional<std::size_t> hit = FindButtonAt(pointer);
		if(hit) m_focusIndex = *hit;
	}

	void ActivateFocused(Controller* controller){
		if(m_buttons.empty()) return;
		if(m_focusIndex >= m_buttons.size()) m_focusIndex = 0;
		const Button button = m_buttons[m_focusIndex];
		ExecuteButton(controller, button);
	}

	bool ActivateCommand(Controller* controller, Command command){
		for(std::size_t index = 0; index < m_buttons.size(); ++index){
			if(m_buttons[index].command != command) continue;
			m_focusIndex = index;
			ActivateFocused(controller);
			return true;
		}
		return false;
	}

	bool ActivateEscape(Controller* controller){
		if(IsDeckScreen(controller->m_flow.Screen())){
			if(controller->m_selectedDeckCard){
				controller->m_selectedDeckCard.reset();
				controller->SetStatus("カード選択を解除した");
				RebuildDirectDeckUi(controller);
				return true;
			}
			return false;
		}

		switch(controller->m_flow.Screen()){
		case FlowScreen::ModeSelect:
			return ActivateCommand(controller, Command::BackToTitle);
		case FlowScreen::Rules:
			return ActivateCommand(controller, Command::BackFromRules);
		case FlowScreen::BattleBoard:
			return ActivateCommand(controller, Command::BattleCancel);
		case FlowScreen::CenterReorder:
			return ActivateCommand(controller, Command::ReorderSkip);
		case FlowScreen::Result:
			return ActivateCommand(controller, Command::ResultTitle);
		default:
			return false;
		}
	}

	void EnsurePersistentUi(){
		if(m_persistentRequested || !m_lifetime || !m_lifetime->alive) return;
		m_persistentRequested = true;
		QueuePersistentText(false);
		QueuePersistentText(true);
	}

	void QueuePersistentText(bool focus){
		const CommandEntity entity = QueueCreateEntity();
		QueueAddComponent<NameComponent>(entity);
		QueueAddComponent<TransformComponent>(entity);
		QueueAddComponent<TextureComponent>(entity);
		QueueAddComponent<RuntimeTextComponent>(entity);
		QueueAddComponent<SpriteRendererComponent>(entity);
		QueueAddComponent<RenderLayerComponent>(entity);
		QueueAddComponent<OrderInLayerComponent>(entity);

		const std::weak_ptr<NavigatorLifetime> weakLifetime = m_lifetime;
		QueueEntitySetup(entity,
			[weakLifetime, focus](Entity created, SceneContext& context){
				auto lifetime = weakLifetime.lock();
				if(!lifetime || !lifetime->alive || !context.component || !context.manager) return;
				if(NameComponent* name = context.component->GetComponent<NameComponent>(created)){
					name->name = focus ? "KeyboardFocusCursor" : "KeyboardHelp";
				}
				if(TransformComponent* transform =
					context.component->GetComponent<TransformComponent>(created)){
					transform->position = focus
						? Vector3(-0.1f, -0.1f, 0.0f)
						: Vector3(0.01f, 0.965f, 0.0f);
					transform->scale = focus
						? Vector3(0.028f, 0.05f, 1.0f)
						: Vector3(0.98f, 0.032f, 1.0f);
				}
				if(SpriteRendererComponent* sprite =
					context.component->GetComponent<SpriteRendererComponent>(created)){
					sprite->anchor = Vector2(0.0f, 0.0f);
					sprite->pivot = Vector2(0.0f, 0.0f);
				}
				if(RuntimeTextComponent* text =
					context.component->GetComponent<RuntimeTextComponent>(created)){
					text->Text = focus ? "▶" : "";
					text->FontSize = focus ? 24.0f : 16.0f;
					text->PixelWidth = focus
						? 36
						: std::max(1, static_cast<int>(context.manager->PlayerScreenSize.x * 0.98f));
					text->PixelHeight = focus ? 42 : 24;
					text->ColorR = focus ? 1.0f : 0.70f;
					text->ColorG = focus ? 0.78f : 0.78f;
					text->ColorB = focus ? 0.26f : 0.92f;
					text->ColorA = 1.0f;
					text->Horizontal = focus
						? RuntimeTextComponent::HorizontalAlignment::Center
						: RuntimeTextComponent::HorizontalAlignment::Center;
					text->Vertical = RuntimeTextComponent::VerticalAlignment::Center;
					text->WordWrap = false;
					text->AutoSizeTransform = true;
					text->MarkDirty();
				}
				if(RenderLayerComponent* layer =
					context.component->GetComponent<RenderLayerComponent>(created)){
					layer->layer = RenderLayer::OverlayUI;
				}
				if(OrderInLayerComponent* order =
					context.component->GetComponent<OrderInLayerComponent>(created)){
					order->order = focus ? 260 : 250;
				}
				if(focus) lifetime->focus = EntityRef(created, &context);
				else lifetime->help = EntityRef(created, &context);
			});
	}

	void UpdatePersistentUi(Controller* controller){
		UpdateFocusCursor(controller);
		UpdateHelpText(controller);
	}

	void UpdateFocusCursor(Controller* controller){
		if(!m_lifetime || !m_lifetime->focus.IsValid()) return;
		SceneContext* context = m_lifetime->focus.GetScene();
		if(!context || !context->component || !context->manager) return;
		TransformComponent* transform = context->component->GetComponent<TransformComponent>(
			m_lifetime->focus.GetEntityID());
		RuntimeTextComponent* text = context->component->GetComponent<RuntimeTextComponent>(
			m_lifetime->focus.GetEntityID());
		if(!transform || !text) return;

		if(m_buttons.empty()){
			if(!text->Text.empty()){
				text->Text.clear();
				text->MarkDirty();
			}
			return;
		}
		if(m_focusIndex >= m_buttons.size()) m_focusIndex = 0;
		if(text->Text != "▶"){
			text->Text = "▶";
			text->MarkDirty();
		}
		const ScreenRect& rect = m_buttons[m_focusIndex].rect;
		const float viewWidth = std::max(1.0f, controller->ViewWidth());
		const float viewHeight = std::max(1.0f, controller->ViewHeight());
		float cursorX = rect.x - 34.0f;
		if(cursorX < 2.0f) cursorX = rect.x + 3.0f;
		transform->position = Vector3(
			cursorX / viewWidth,
			(rect.y + rect.height * 0.5f - 20.0f) / viewHeight,
			0.0f
		);
	}

	void UpdateHelpText(Controller* controller){
		if(!m_lifetime || !m_lifetime->help.IsValid()) return;
		SceneContext* context = m_lifetime->help.GetScene();
		if(!context || !context->component || !context->manager) return;
		TransformComponent* transform = context->component->GetComponent<TransformComponent>(
			m_lifetime->help.GetEntityID());
		RuntimeTextComponent* text = context->component->GetComponent<RuntimeTextComponent>(
			m_lifetime->help.GetEntityID());
		if(!transform || !text) return;

		std::string help;
		const FlowScreen screen = controller->m_flow.Screen();
		if(IsDeckScreen(screen)){
			help = "ドラッグ: 駒/順番変更  |  選択後 1/2/3: 駒へ移動  PgUp/PgDn: 前後  Home/End: 先頭/末尾  Esc: 選択解除";
		} else {
			switch(screen){
			case FlowScreen::Title:
				help = "Enter / Space: はじめる";
				break;
			case FlowScreen::ModeSelect:
				help = "矢印/WASD: 選択  Enter: 決定  1: CPU  2: ローカル  3: ルール  Esc: タイトル";
				break;
			case FlowScreen::Rules:
				help = "Enter / Esc: 戻る";
				break;
			case FlowScreen::LocalPrivacyHandoff:
			case FlowScreen::MatchIntroduction:
				help = "Enter / Space: 続行";
				break;
			case FlowScreen::BattleBoard:{
				const bool scout = controller->m_interaction.Mode() == BattleInputMode::Scout;
				const bool selected = controller->m_interaction.SelectedPiece().has_value();
				help = scout ? "偵察モード: " : "移動/戦闘モード: ";
				help += selected
					? "次に対象マスまたは敵駒を選択"
					: "まず自分の駒を選択";
				help += "  |  1: 移動/戦闘  2: 偵察  Esc: 選択解除";
				break;
			}
			case FlowScreen::CenterReorder:
				help = "カードを選択 → 左右で位置変更  Enter: 決定  Esc: 再編しない";
				break;
			case FlowScreen::Result:
				help = "矢印: 選択  Enter: 決定  Esc: タイトル";
				break;
			default:
				help = "矢印/WASD: 選択  Enter/Space: 決定  Esc: 戻る/解除";
				break;
			}
		}

		const int desiredWidth = std::max(
			1,
			static_cast<int>(context->manager->PlayerScreenSize.x * 0.98f)
		);
		transform->position = Vector3(0.01f, 0.965f, 0.0f);
		transform->scale = Vector3(0.98f, 0.032f, 1.0f);
		if(text->Text != help || text->PixelWidth != desiredWidth){
			text->Text = std::move(help);
			text->PixelWidth = desiredWidth;
			text->MarkDirty();
		}
	}

	void DestroyPersistentEntity(EntityRef& entity){
		if(entity.IsValid()) QueueDestroyEntity(entity.GetEntityID());
		entity = {};
	}

	std::shared_ptr<NavigatorLifetime> m_lifetime;
	std::vector<Button> m_buttons;
	std::array<ScreenRect, 3> m_deckDropZones{};
	std::optional<std::pair<std::size_t, std::size_t>> m_dragSource;
	ScreenPoint m_pointerPressPoint{};
	ScreenPoint m_lastPointer{};
	FlowScreen m_lastScreen = FlowScreen::Title;
	std::size_t m_focusIndex = 0;
	float m_deckCardTop = 198.0f;
	float m_deckRowHeight = 42.0f;
	bool m_persistentRequested = false;
	bool m_pendingDeckRebuild = false;
	bool m_deckUiActive = false;
	bool m_pointerWasDown = false;
	bool m_hasLastPointer = false;
	bool m_hasLastScreen = false;
};

} // namespace ElemenTactics
