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
#include <string>
#include <utility>
#include <vector>
#include <windows.h>

namespace ElemenTactics {

// Player-facing keyboard navigation for the Runtime Text UI.
// It drives the controller's existing UiButton commands directly, so keyboard
// and pointer input always use the same validated game-flow path.
class ElemenTacticsKeyboardNavigator final : public CustomScriptComponent {
public:
	ElemenTacticsKeyboardNavigator(){
		scriptName = "ElemenTacticsKeyboardNavigator";
		// The controller builds/rebuilds its Runtime UI at the default priority.
		// Run after it so the current button set can be navigated and highlighted.
		SetExecutionOrder(
			SystemTaskDomain::Frame,
			SystemPhase::Default,
			100
		);
	}

	void OnStart() override{
		m_lifetime = std::make_shared<NavigatorLifetime>();
		m_helpRequested = false;
		m_hasLastScreen = false;
		m_focusIndex = 0;
	}

	void OnUpdate(float) override{
		ElemenTacticsGameController* controller = ResolveController();
		if(!controller || !controller->m_started) return;

		EnsureHelpText();
		UpdateHelpText(controller);

		const FlowScreen screen = controller->m_flow.Screen();
		if(!m_hasLastScreen || screen != m_lastScreen){
			m_lastScreen = screen;
			m_hasLastScreen = true;
			m_focusIndex = 0;
		}

		auto& buttons = controller->m_buttons;
		if(buttons.empty()){
			return;
		}
		if(m_focusIndex >= buttons.size()) m_focusIndex = 0;

		SyncFocusFromPointer(controller);

		if(HandleQuickShortcut(controller)) return;

		if(GetKeyDown(VK_TAB)){
			const bool reverse = GetKey(VK_SHIFT) || GetKey(VK_LSHIFT) || GetKey(VK_RSHIFT);
			MoveSequential(controller, reverse ? -1 : 1);
		}
		if(GetKeyDown(VK_UP) || GetKeyDown('W')) MoveSpatial(controller, 0.0f, -1.0f);
		if(GetKeyDown(VK_DOWN) || GetKeyDown('S')) MoveSpatial(controller, 0.0f, 1.0f);
		if(GetKeyDown(VK_LEFT) || GetKeyDown('A')) MoveSpatial(controller, -1.0f, 0.0f);
		if(GetKeyDown(VK_RIGHT) || GetKeyDown('D')) MoveSpatial(controller, 1.0f, 0.0f);

		if(GetKeyDown(VK_ESCAPE)){
			if(ActivateEscapeCommand(controller)) return;
		}

		if(GetKeyDown(VK_RETURN) || GetKeyDown(VK_SPACE)){
			ActivateFocused(controller);
			return;
		}

		UpdateButtonPresentation(controller);
		if(controller->m_textSystem){
			controller->m_textSystem->ProcessDirtyText();
		}
	}

	void OnStop() override{
		if(m_lifetime){
			m_lifetime->alive = false;
			if(m_lifetime->help.IsValid()){
				QueueDestroyEntity(m_lifetime->help.GetEntityID());
			}
			m_lifetime->help = {};
		}
		m_helpRequested = false;
	}

private:
	struct NavigatorLifetime {
		bool alive = true;
		EntityRef help;
	};

	ElemenTacticsGameController* ResolveController() const{
		const EntityRef self = GetEntityRef();
		SceneContext* context = self.GetScene();
		if(!self.IsValid() || !context || !context->component) return nullptr;
		return context->component->GetComponent<ElemenTacticsGameController>(
			self.GetEntityID()
		);
	}

	static ScreenPoint CenterOf(const ScreenRect& rect) noexcept{
		return ScreenPoint{
			rect.x + rect.width * 0.5f,
			rect.y + rect.height * 0.5f
		};
	}

	void MoveSequential(ElemenTacticsGameController* controller, int direction){
		const std::size_t count = controller->m_buttons.size();
		if(count == 0) return;
		if(direction < 0){
			m_focusIndex = (m_focusIndex + count - 1) % count;
		} else {
			m_focusIndex = (m_focusIndex + 1) % count;
		}
	}

	void MoveSpatial(
		ElemenTacticsGameController* controller,
		float directionX,
		float directionY
	){
		const auto& buttons = controller->m_buttons;
		if(buttons.empty()) return;
		if(m_focusIndex >= buttons.size()) m_focusIndex = 0;

		const ScreenPoint origin = CenterOf(buttons[m_focusIndex].rect);
		std::size_t bestIndex = m_focusIndex;
		float bestScore = std::numeric_limits<float>::max();

		for(std::size_t index = 0; index < buttons.size(); ++index){
			if(index == m_focusIndex) continue;
			const ScreenPoint candidate = CenterOf(buttons[index].rect);
			const float offsetX = candidate.x - origin.x;
			const float offsetY = candidate.y - origin.y;
			const float forward = offsetX * directionX + offsetY * directionY;
			if(forward <= 1.0f) continue;

			const float lateral = std::abs(offsetX * directionY - offsetY * directionX);
			// Strongly prefer candidates aligned with the requested direction while
			// still allowing navigation through irregular board/button layouts.
			const float score = forward + lateral * 2.5f;
			if(score < bestScore){
				bestScore = score;
				bestIndex = index;
			}
		}

		if(bestIndex == m_focusIndex){
			const bool backward = directionX < 0.0f || directionY < 0.0f;
			MoveSequential(controller, backward ? -1 : 1);
			return;
		}
		m_focusIndex = bestIndex;
	}

	void SyncFocusFromPointer(ElemenTacticsGameController* controller){
		SceneContext* context = GetEntityRef().GetScene();
		if(!context || !context->manager || !context->manager->input ||
			!context->manager->hwnd){
			return;
		}

		const ScreenPoint pointer{
			static_cast<float>(context->manager->input->GetMouseX()),
			static_cast<float>(context->manager->input->GetMouseY())
		};
		for(std::size_t index = 0; index < controller->m_buttons.size(); ++index){
			if(controller->m_buttons[index].rect.Contains(pointer)){
				m_focusIndex = index;
				return;
			}
		}
	}

	void ActivateFocused(ElemenTacticsGameController* controller){
		if(controller->m_buttons.empty()) return;
		if(m_focusIndex >= controller->m_buttons.size()) m_focusIndex = 0;
		const FlowScreen before = controller->m_flow.Screen();
		const ElemenTacticsGameController::UiButton button =
			controller->m_buttons[m_focusIndex];
		controller->HandleCommand(button);
		if(controller->m_flow.Screen() != before){
			m_focusIndex = 0;
			m_lastScreen = controller->m_flow.Screen();
		}
	}

	bool ActivateCommand(
		ElemenTacticsGameController* controller,
		ElemenTacticsGameController::UiCommand command
	){
		for(std::size_t index = 0; index < controller->m_buttons.size(); ++index){
			if(controller->m_buttons[index].command != command) continue;
			m_focusIndex = index;
			ActivateFocused(controller);
			return true;
		}
		return false;
	}

	bool HandleQuickShortcut(ElemenTacticsGameController* controller){
		switch(controller->m_flow.Screen()){
		case FlowScreen::ModeSelect:
			if(GetKeyDown('1')) return ActivateCommand(
				controller, ElemenTacticsGameController::UiCommand::ModeLlm);
			if(GetKeyDown('2')) return ActivateCommand(
				controller, ElemenTacticsGameController::UiCommand::ModeLocal);
			if(GetKeyDown('3')) return ActivateCommand(
				controller, ElemenTacticsGameController::UiCommand::OpenRules);
			break;
		case FlowScreen::BattleBoard:
			if(GetKeyDown('1')) return ActivateCommand(
				controller, ElemenTacticsGameController::UiCommand::BattleMoveMode);
			if(GetKeyDown('2')) return ActivateCommand(
				controller, ElemenTacticsGameController::UiCommand::BattleScoutMode);
			break;
		default:
			break;
		}
		return false;
	}

	bool ActivateEscapeCommand(ElemenTacticsGameController* controller){
		using Command = ElemenTacticsGameController::UiCommand;
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

	static std::string StripFocusPrefix(std::string text){
		static const std::array<std::string, 2> prefixes{"▶ ", "  "};
		for(const std::string& prefix : prefixes){
			if(text.starts_with(prefix)){
				text.erase(0, prefix.size());
				break;
			}
		}
		return text;
	}

	void UpdateButtonPresentation(ElemenTacticsGameController* controller){
		SceneContext* context = GetEntityRef().GetScene();
		if(!context || !context->component || !context->manager ||
			controller->m_buttons.empty()){
			return;
		}

		const float viewWidth = std::max(1.0f, context->manager->PlayerScreenSize.x);
		const float viewHeight = std::max(1.0f, context->manager->PlayerScreenSize.y);
		const std::vector<Entity> entities =
			context->component->QueryEntities<
				NameComponent,
				TransformComponent,
				RuntimeTextComponent>();

		for(Entity entity : entities){
			NameComponent* name = context->component->GetComponent<NameComponent>(entity);
			TransformComponent* transform =
				context->component->GetComponent<TransformComponent>(entity);
			RuntimeTextComponent* text =
				context->component->GetComponent<RuntimeTextComponent>(entity);
			if(!name || !transform || !text || name->name != "Button") continue;

			const ScreenPoint entityCenter{
				(transform->position.x() + transform->scale.x() * 0.5f) * viewWidth,
				(transform->position.y() + transform->scale.y() * 0.5f) * viewHeight
			};

			std::size_t nearestIndex = 0;
			float nearestDistance = std::numeric_limits<float>::max();
			for(std::size_t index = 0; index < controller->m_buttons.size(); ++index){
				const ScreenPoint buttonCenter = CenterOf(controller->m_buttons[index].rect);
				const float offsetX = entityCenter.x - buttonCenter.x;
				const float offsetY = entityCenter.y - buttonCenter.y;
				const float distance = offsetX * offsetX + offsetY * offsetY;
				if(distance < nearestDistance){
					nearestDistance = distance;
					nearestIndex = index;
				}
			}

			const bool focused = nearestIndex == m_focusIndex;
			const std::string baseText = StripFocusPrefix(text->Text);
			const std::string desiredText = focused
				? "▶ " + baseText
				: "  " + baseText;
			const float desiredR = focused ? 1.0f : 0.82f;
			const float desiredG = focused ? 0.82f : 0.92f;
			const float desiredB = focused ? 0.32f : 1.0f;

			if(text->Text != desiredText ||
				std::abs(text->ColorR - desiredR) > 0.001f ||
				std::abs(text->ColorG - desiredG) > 0.001f ||
				std::abs(text->ColorB - desiredB) > 0.001f){
				text->Text = desiredText;
				text->ColorR = desiredR;
				text->ColorG = desiredG;
				text->ColorB = desiredB;
				text->MarkDirty();
			}
		}
	}

	void EnsureHelpText(){
		if(m_helpRequested || !m_lifetime || !m_lifetime->alive) return;
		m_helpRequested = true;

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
			[weakLifetime](Entity created, SceneContext& context){
				auto lifetime = weakLifetime.lock();
				if(!lifetime || !lifetime->alive || !context.component ||
					!context.manager){
					return;
				}

				if(NameComponent* name =
					context.component->GetComponent<NameComponent>(created)){
					name->name = "KeyboardHelp";
				}
				if(TransformComponent* transform =
					context.component->GetComponent<TransformComponent>(created)){
					transform->position = Vector3(0.02f, 0.945f, 0.0f);
					transform->scale = Vector3(0.96f, 0.045f, 1.0f);
				}
				if(SpriteRendererComponent* sprite =
					context.component->GetComponent<SpriteRendererComponent>(created)){
					sprite->anchor = Vector2(0.0f, 0.0f);
					sprite->pivot = Vector2(0.0f, 0.0f);
				}
				if(RuntimeTextComponent* text =
					context.component->GetComponent<RuntimeTextComponent>(created)){
					text->Text = "キーボード: 矢印/WASD 選択  Enter/Space 決定  Esc 戻る/解除";
					text->FontSize = 18.0f;
					text->PixelWidth = std::max(
						1,
						static_cast<int>(context.manager->PlayerScreenSize.x * 0.96f)
					);
					text->PixelHeight = 36;
					text->ColorR = 0.72f;
					text->ColorG = 0.80f;
					text->ColorB = 0.94f;
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
					order->order = 200;
				}
				lifetime->help = EntityRef(created, &context);
			});
	}

	void UpdateHelpText(ElemenTacticsGameController* controller){
		if(!m_lifetime || !m_lifetime->help.IsValid()) return;
		SceneContext* context = m_lifetime->help.GetScene();
		if(!context || !context->component || !context->manager) return;

		RuntimeTextComponent* text =
			context->component->GetComponent<RuntimeTextComponent>(
				m_lifetime->help.GetEntityID()
			);
		if(!text) return;

		std::string help = "矢印/WASD: 選択  Enter/Space: 決定";
		switch(controller->m_flow.Screen()){
		case FlowScreen::ModeSelect:
			help += "  1: CPU  2: ローカル  3: ルール  Esc: 戻る";
			break;
		case FlowScreen::BattleBoard:
			help += "  1: 移動/戦闘  2: 偵察  Esc: 選択解除";
			break;
		case FlowScreen::Rules:
		case FlowScreen::CenterReorder:
		case FlowScreen::Result:
			help += "  Esc: 戻る";
			break;
		default:
			break;
		}

		const int desiredWidth = std::max(
			1,
			static_cast<int>(context->manager->PlayerScreenSize.x * 0.96f)
		);
		if(text->Text != help || text->PixelWidth != desiredWidth){
			text->Text = std::move(help);
			text->PixelWidth = desiredWidth;
			text->MarkDirty();
		}
	}

	std::shared_ptr<NavigatorLifetime> m_lifetime;
	FlowScreen m_lastScreen = FlowScreen::Title;
	std::size_t m_focusIndex = 0;
	bool m_helpRequested = false;
	bool m_hasLastScreen = false;
};

} // namespace ElemenTactics
