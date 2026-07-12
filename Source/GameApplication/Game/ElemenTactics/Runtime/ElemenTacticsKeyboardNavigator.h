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

// Keyboard-first player input. This component never steals the controller's
// button list and never depends on mouse coordinates. Every runtime screen can
// be completed with the keyboard alone.
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
		m_focusIndex = 0;
		m_hasLastScreen = false;
		m_persistentRequested = false;
		EnsurePersistentUi();
	}

	void OnUpdate(float) override{
		Controller* controller = ResolveController();
		if(!controller || !controller->m_started) return;

		EnsurePersistentUi();
		SyncScreen(controller);
		ClampFocus(controller);
		HandleKeyboard(controller);
		ClampFocus(controller);
		UpdateFocusCursor(controller);
		UpdateHelpText(controller);

		if(controller->m_textSystem){
			controller->m_textSystem->ProcessDirtyText();
		}
	}

	void OnStop() override{
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

	void SyncScreen(Controller* controller){
		const FlowScreen screen = controller->m_flow.Screen();
		if(m_hasLastScreen && screen == m_lastScreen) return;
		m_lastScreen = screen;
		m_hasLastScreen = true;
		m_focusIndex = FindDefaultFocus(controller);
	}

	std::size_t FindDefaultFocus(const Controller* controller) const{
		if(!controller || controller->m_buttons.empty()) return 0;

		if(IsDeckScreen(controller->m_flow.Screen())){
			if(controller->m_selectedDeckCard){
				for(std::size_t index = 0; index < controller->m_buttons.size(); ++index){
					const Button& button = controller->m_buttons[index];
					if(button.command == Command::DeckCard &&
						button.primary == static_cast<int>(controller->m_selectedDeckCard->first) &&
						button.secondary == static_cast<int>(controller->m_selectedDeckCard->second)){
						return index;
					}
				}
			}
			for(std::size_t index = 0; index < controller->m_buttons.size(); ++index){
				if(controller->m_buttons[index].command == Command::DeckCard) return index;
			}
		}

		if(controller->m_flow.Screen() == FlowScreen::BattleBoard && controller->m_flow.Match()){
			const GameState& state = *controller->m_flow.Match();
			const PlayerId viewer = controller->ActiveViewer();
			for(const PieceState& piece : state.pieces[PlayerIndex(viewer)]){
				if(!piece.alive) continue;
				for(std::size_t index = 0; index < controller->m_buttons.size(); ++index){
					const Button& button = controller->m_buttons[index];
					if(button.command == Command::BoardCell && button.primary == piece.cell){
						return index;
					}
				}
			}
		}

		return 0;
	}

	void ClampFocus(const Controller* controller){
		if(!controller || controller->m_buttons.empty()){
			m_focusIndex = 0;
			return;
		}
		if(m_focusIndex >= controller->m_buttons.size()) m_focusIndex = 0;
	}

	void HandleKeyboard(Controller* controller){
		if(!controller || controller->m_buttons.empty()) return;

		if(HandleScreenShortcuts(controller)) return;

		bool moved = false;
		if(GetKeyDown(VK_TAB)){
			const bool reverse = GetKey(VK_SHIFT) || GetKey(VK_LSHIFT) || GetKey(VK_RSHIFT);
			MoveSequential(controller, reverse ? -1 : 1);
			moved = true;
		}
		if(GetKeyDown(VK_UP) || GetKeyDown('W')){
			MoveSpatial(controller, 0.0f, -1.0f);
			moved = true;
		}
		if(GetKeyDown(VK_DOWN) || GetKeyDown('S')){
			MoveSpatial(controller, 0.0f, 1.0f);
			moved = true;
		}
		if(GetKeyDown(VK_LEFT) || GetKeyDown('A')){
			MoveSpatial(controller, -1.0f, 0.0f);
			moved = true;
		}
		if(GetKeyDown(VK_RIGHT) || GetKeyDown('D')){
			MoveSpatial(controller, 1.0f, 0.0f);
			moved = true;
		}

		if(moved) return;

		if(GetKeyDown(VK_BACK)){
			HandleBack(controller);
			return;
		}

		if(GetKeyDown(VK_RETURN) || GetKeyDown(VK_SPACE)){
			ActivateFocused(controller);
		}
	}

	bool HandleScreenShortcuts(Controller* controller){
		const FlowScreen screen = controller->m_flow.Screen();

		if(IsDeckScreen(screen)){
			if(GetKeyDown('1')) return MoveSelectedToSlot(controller, 0);
			if(GetKeyDown('2')) return MoveSelectedToSlot(controller, 1);
			if(GetKeyDown('3')) return MoveSelectedToSlot(controller, 2);
			if(GetKeyDown('Q') || GetKeyDown(VK_PRIOR)) return MoveSelectedOrder(controller, -1);
			if(GetKeyDown('E') || GetKeyDown(VK_NEXT)) return MoveSelectedOrder(controller, 1);
			if(GetKeyDown(VK_HOME)) return MoveSelectedExtreme(controller, true);
			if(GetKeyDown(VK_END)) return MoveSelectedExtreme(controller, false);
			if(GetKeyDown('B')) return ActivateCommand(controller, Command::DeckBalanced);
			if(GetKeyDown('X')) return ActivateCommand(controller, Command::DeckConcentrated);
			if(GetKeyDown('C')) return ActivateCommand(controller, Command::DeckConfirm);
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
		case FlowScreen::CenterReorder:
			if(GetKeyDown('Q')) return ActivateCommand(controller, Command::ReorderLeft);
			if(GetKeyDown('E')) return ActivateCommand(controller, Command::ReorderRight);
			if(GetKeyDown('C')) return ActivateCommand(controller, Command::ReorderConfirm);
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

	void HandleBack(Controller* controller){
		if(!controller) return;
		if(IsDeckScreen(controller->m_flow.Screen())){
			if(controller->m_selectedDeckCard){
				controller->m_selectedDeckCard.reset();
				controller->SetStatus("カード選択を解除した");
				RebuildController(controller);
			}
			return;
		}

		switch(controller->m_flow.Screen()){
		case FlowScreen::ModeSelect:
			ActivateCommand(controller, Command::BackToTitle);
			break;
		case FlowScreen::Rules:
			ActivateCommand(controller, Command::BackFromRules);
			break;
		case FlowScreen::BattleBoard:
			ActivateCommand(controller, Command::BattleCancel);
			break;
		case FlowScreen::CenterReorder:
			ActivateCommand(controller, Command::ReorderSkip);
			break;
		case FlowScreen::Result:
			ActivateCommand(controller, Command::ResultTitle);
			break;
		default:
			break;
		}
	}

	static ScreenPoint CenterOf(const ScreenRect& rect) noexcept{
		return ScreenPoint{
			rect.x + rect.width * 0.5f,
			rect.y + rect.height * 0.5f
		};
	}

	void MoveSequential(const Controller* controller, int direction){
		const std::size_t count = controller ? controller->m_buttons.size() : 0;
		if(count == 0) return;
		m_focusIndex = direction < 0
			? (m_focusIndex + count - 1) % count
			: (m_focusIndex + 1) % count;
	}

	void MoveSpatial(const Controller* controller, float directionX, float directionY){
		if(!controller || controller->m_buttons.empty()) return;
		ClampFocus(controller);

		const ScreenPoint origin = CenterOf(controller->m_buttons[m_focusIndex].rect);
		std::size_t bestIndex = m_focusIndex;
		float bestScore = std::numeric_limits<float>::max();

		for(std::size_t index = 0; index < controller->m_buttons.size(); ++index){
			if(index == m_focusIndex) continue;
			const ScreenPoint candidate = CenterOf(controller->m_buttons[index].rect);
			const float offsetX = candidate.x - origin.x;
			const float offsetY = candidate.y - origin.y;
			const float forward = offsetX * directionX + offsetY * directionY;
			if(forward <= 1.0f) continue;

			const float lateral = std::abs(offsetX * directionY - offsetY * directionX);
			const float score = forward + lateral * 2.6f;
			if(score < bestScore){
				bestScore = score;
				bestIndex = index;
			}
		}

		if(bestIndex == m_focusIndex){
			MoveSequential(controller, directionX < 0.0f || directionY < 0.0f ? -1 : 1);
		}else{
			m_focusIndex = bestIndex;
		}
	}

	void ActivateFocused(Controller* controller){
		if(!controller || controller->m_buttons.empty()) return;
		ClampFocus(controller);
		ExecuteButton(controller, controller->m_buttons[m_focusIndex]);
	}

	bool ActivateCommand(Controller* controller, Command command){
		if(!controller) return false;
		for(std::size_t index = 0; index < controller->m_buttons.size(); ++index){
			if(controller->m_buttons[index].command != command) continue;
			m_focusIndex = index;
			ExecuteButton(controller, controller->m_buttons[index]);
			return true;
		}
		return false;
	}

	void ExecuteButton(Controller* controller, const Button& button){
		if(!controller) return;
		const FlowScreen before = controller->m_flow.Screen();
		controller->HandleCommand(button);
		if(controller->m_screenDirty){
			controller->RebuildScreen();
		}
		if(controller->m_flow.Screen() != before){
			m_lastScreen = controller->m_flow.Screen();
			m_hasLastScreen = true;
			m_focusIndex = FindDefaultFocus(controller);
		}else{
			ClampFocus(controller);
		}
	}

	bool ResolveSelectedCard(
		Controller* controller,
		std::size_t& slot,
		std::size_t& index
	){
		if(!controller || !controller->m_selectedDeckCard){
			if(controller){
				controller->SetStatus("先にカードを選択してください");
				RebuildController(controller);
			}
			return false;
		}

		slot = controller->m_selectedDeckCard->first;
		index = controller->m_selectedDeckCard->second;
		const auto& decks = controller->m_flow.EditingDeck().Decks();
		if(slot >= decks.size() || index >= decks[slot].size()){
			controller->m_selectedDeckCard.reset();
			controller->SetStatus("カード選択をリセットした");
			RebuildController(controller);
			return false;
		}
		return true;
	}

	bool MoveSelectedToSlot(Controller* controller, int destinationValue){
		std::size_t sourceSlot = 0;
		std::size_t sourceIndex = 0;
		if(!ResolveSelectedCard(controller, sourceSlot, sourceIndex)) return true;
		if(destinationValue < 0 || destinationValue >= 3) return true;

		const std::size_t destinationSlot = static_cast<std::size_t>(destinationValue);
		if(destinationSlot == sourceSlot){
			controller->SetStatus("選択カードはすでに駒 " + std::to_string(destinationSlot + 1) + " にある");
			RebuildController(controller);
			return true;
		}

		std::string error;
		const std::size_t destinationIndex =
			controller->m_flow.EditingDeck().Decks()[destinationSlot].size();
		if(controller->m_flow.EditingDeck().MoveCard(
			sourceSlot,
			sourceIndex,
			destinationSlot,
			destinationIndex,
			&error)){
			controller->m_selectedDeckCard =
				std::pair<std::size_t, std::size_t>{destinationSlot, destinationIndex};
			controller->SetStatus("カードを駒 " + std::to_string(destinationSlot + 1) + " へ移動した");
		}else{
			controller->SetStatus(error.empty() ? "カードを移動できない" : error);
		}
		RebuildController(controller);
		return true;
	}

	bool MoveSelectedOrder(Controller* controller, int direction){
		std::size_t slot = 0;
		std::size_t index = 0;
		if(!ResolveSelectedCard(controller, slot, index)) return true;

		const std::size_t size = controller->m_flow.EditingDeck().Decks()[slot].size();
		if(direction < 0 && index == 0){
			controller->SetStatus("すでに先頭カードです");
			RebuildController(controller);
			return true;
		}
		if(direction > 0 && index + 1 >= size){
			controller->SetStatus("すでに末尾カードです");
			RebuildController(controller);
			return true;
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
		}else{
			controller->SetStatus(error.empty() ? "順番を変更できない" : error);
		}
		RebuildController(controller);
		return true;
	}

	bool MoveSelectedExtreme(Controller* controller, bool toFront){
		std::size_t slot = 0;
		std::size_t index = 0;
		if(!ResolveSelectedCard(controller, slot, index)) return true;

		const std::size_t size = controller->m_flow.EditingDeck().Decks()[slot].size();
		const std::size_t destination = toFront ? 0 : size;
		std::string error;
		if(controller->m_flow.EditingDeck().ReorderCard(slot, index, destination, &error)){
			controller->m_selectedDeckCard = std::pair<std::size_t, std::size_t>{
				slot,
				toFront ? 0 : size - 1
			};
			controller->SetStatus(toFront ? "カードを先頭へ移動した" : "カードを末尾へ移動した");
		}else{
			controller->SetStatus(error.empty() ? "順番を変更できない" : error);
		}
		RebuildController(controller);
		return true;
	}

	void RebuildController(Controller* controller){
		if(!controller) return;
		controller->m_screenDirty = true;
		controller->RebuildScreen();
		m_focusIndex = FindDefaultFocus(controller);
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
						? Vector3(0.03f, 0.055f, 1.0f)
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
					text->FontSize = focus ? 25.0f : 16.0f;
					text->PixelWidth = focus
						? 40
						: std::max(1, static_cast<int>(context.manager->PlayerScreenSize.x * 0.98f));
					text->PixelHeight = focus ? 44 : 24;
					text->ColorR = focus ? 1.0f : 0.72f;
					text->ColorG = focus ? 0.80f : 0.80f;
					text->ColorB = focus ? 0.26f : 0.94f;
					text->ColorA = 1.0f;
					text->Horizontal = RuntimeTextComponent::HorizontalAlignment::Center;
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

	void UpdateFocusCursor(Controller* controller){
		if(!controller || !m_lifetime || !m_lifetime->focus.IsValid()) return;
		SceneContext* context = m_lifetime->focus.GetScene();
		if(!context || !context->component) return;

		TransformComponent* transform = context->component->GetComponent<TransformComponent>(
			m_lifetime->focus.GetEntityID());
		RuntimeTextComponent* text = context->component->GetComponent<RuntimeTextComponent>(
			m_lifetime->focus.GetEntityID());
		if(!transform || !text) return;

		if(controller->m_buttons.empty()){
			if(!text->Text.empty()){
				text->Text.clear();
				text->MarkDirty();
			}
			return;
		}

		ClampFocus(controller);
		if(text->Text != "▶"){
			text->Text = "▶";
			text->MarkDirty();
		}

		const ScreenRect& rect = controller->m_buttons[m_focusIndex].rect;
		const float viewWidth = std::max(1.0f, controller->ViewWidth());
		const float viewHeight = std::max(1.0f, controller->ViewHeight());
		float cursorX = rect.x - 36.0f;
		if(cursorX < 2.0f) cursorX = rect.x + 2.0f;
		transform->position = Vector3(
			cursorX / viewWidth,
			(rect.y + rect.height * 0.5f - 21.0f) / viewHeight,
			0.0f
		);
	}

	void UpdateHelpText(Controller* controller){
		if(!controller || !m_lifetime || !m_lifetime->help.IsValid()) return;
		SceneContext* context = m_lifetime->help.GetScene();
		if(!context || !context->component || !context->manager) return;

		RuntimeTextComponent* text = context->component->GetComponent<RuntimeTextComponent>(
			m_lifetime->help.GetEntityID());
		if(!text) return;

		std::string help;
		const FlowScreen screen = controller->m_flow.Screen();
		if(IsDeckScreen(screen)){
			help = "矢印/WASD: 選択  Enter: カード選択  1/2/3: 駒へ移動  Q/E: 前後  Home/End: 先頭/末尾  B/X: プリセット  C: 確定  Backspace: 解除";
		}else{
			switch(screen){
			case FlowScreen::Title:
				help = "Enter / Space: はじめる";
				break;
			case FlowScreen::ModeSelect:
				help = "矢印/WASD: 選択  Enter: 決定  1: CPU  2: ローカル  3: ルール  Backspace: タイトル";
				break;
			case FlowScreen::Rules:
				help = "Enter / Backspace: 戻る";
				break;
			case FlowScreen::LocalPrivacyHandoff:
			case FlowScreen::MatchIntroduction:
				help = "Enter / Space: 続行";
				break;
			case FlowScreen::BattleBoard:
				help = controller->m_interaction.SelectedPiece()
					? "矢印/WASD: 対象マス選択  Enter: 実行  1: 移動/戦闘  2: 偵察  Backspace: 選択解除"
					: "矢印/WASD: 自分の駒を選択  Enter: 決定  1: 移動/戦闘  2: 偵察";
				break;
			case FlowScreen::CenterReorder:
				help = "矢印/WASD: カード選択  Enter: 選択  Q/E: 左右へ移動  C: 確定  Backspace: 再編しない";
				break;
			case FlowScreen::Result:
				help = "矢印/WASD: 選択  Enter: 決定  R: 再戦  T/Backspace: タイトル";
				break;
			default:
				help = "矢印/WASD: 選択  Enter/Space: 決定  Tab: 順送り  Backspace: 戻る/解除";
				break;
			}
		}

		const int desiredWidth = std::max(
			1,
			static_cast<int>(context->manager->PlayerScreenSize.x * 0.98f)
		);
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
	FlowScreen m_lastScreen = FlowScreen::Title;
	std::size_t m_focusIndex = 0;
	bool m_hasLastScreen = false;
	bool m_persistentRequested = false;
};

} // namespace ElemenTactics
