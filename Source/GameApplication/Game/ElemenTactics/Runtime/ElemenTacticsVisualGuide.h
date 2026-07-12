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
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ElemenTactics {

// Adds an image-like presentation layer without requiring authored texture files.
// Every emblem, card accent, token, selection ring and relationship diagram is
// generated from RuntimeText/Sprite entities, so the game remains self-contained.
class ElemenTacticsVisualGuide final : public CustomScriptComponent {
public:
	ElemenTacticsVisualGuide(){
		scriptName = "ElemenTacticsVisualGuide";
		SetExecutionOrder(SystemTaskDomain::Frame, SystemPhase::Default, 120);
	}

	void OnStart() override{
		m_lifetime = std::make_shared<VisualLifetime>();
		m_signature = std::numeric_limits<std::uint64_t>::max();
	}

	void OnUpdate(float) override{
		ElemenTacticsGameController* controller = ResolveController();
		if(!controller || !controller->m_started) return;

		const std::uint64_t signature = BuildSignature(*controller);
		if(signature != m_signature){
			m_signature = signature;
			Rebuild(controller);
		}

		if(controller->m_textSystem){
			controller->m_textSystem->ProcessDirtyText();
		}
	}

	void OnStop() override{
		ClearVisuals();
		m_signature = std::numeric_limits<std::uint64_t>::max();
	}

private:
	struct VisualLifetime {
		bool alive = true;
		std::vector<EntityRef> entities;
	};

	struct Color {
		float r = 1.0f;
		float g = 1.0f;
		float b = 1.0f;
	};

	ElemenTacticsGameController* ResolveController() const{
		const EntityRef self = GetEntityRef();
		SceneContext* context = self.GetScene();
		if(!self.IsValid() || !context || !context->component) return nullptr;
		return context->component->GetComponent<ElemenTacticsGameController>(self.GetEntityID());
	}

	static void HashValue(std::uint64_t& hash, std::uint64_t value) noexcept{
		constexpr std::uint64_t prime = 1099511628211ULL;
		for(int byte = 0; byte < 8; ++byte){
			hash ^= static_cast<unsigned char>((value >> (byte * 8)) & 0xffULL);
			hash *= prime;
		}
	}

	std::uint64_t BuildSignature(const ElemenTacticsGameController& controller) const{
		std::uint64_t hash = 1469598103934665603ULL;
		HashValue(hash, static_cast<std::uint64_t>(controller.m_flow.Screen()));
		HashValue(hash, controller.m_localTurnHandoff ? 1ULL : 0ULL);
		HashValue(hash, static_cast<std::uint64_t>(std::llround(controller.ViewWidth())));
		HashValue(hash, static_cast<std::uint64_t>(std::llround(controller.ViewHeight())));
		HashValue(hash, static_cast<std::uint64_t>(controller.m_interaction.Mode()));

		if(controller.m_selectedDeckCard){
			HashValue(hash, 100ULL + controller.m_selectedDeckCard->first);
			HashValue(hash, 200ULL + controller.m_selectedDeckCard->second);
		}
		if(controller.m_selectedReorderCard){
			HashValue(hash, 300ULL + *controller.m_selectedReorderCard);
		}
		if(controller.m_interaction.SelectedPiece()){
			const PieceId piece = *controller.m_interaction.SelectedPiece();
			HashValue(hash, 400ULL + PlayerIndex(piece.owner));
			HashValue(hash, 500ULL + piece.slot);
		}

		if(const GameState* state = controller.m_flow.Match()){
			HashValue(hash, state->actionSerial);
			HashValue(hash, static_cast<std::uint64_t>(state->actionsRemaining));
			HashValue(hash, static_cast<std::uint64_t>(state->currentPlayer));
			HashValue(hash, state->result.finished ? 1ULL : 0ULL);
			if(state->pendingReorder){
				HashValue(hash, 600ULL + PlayerIndex(state->pendingReorder->piece.owner));
				HashValue(hash, 700ULL + state->pendingReorder->piece.slot);
			}
		}

		if(controller.m_flow.Screen() == FlowScreen::DeckSetupPlayerOne ||
			controller.m_flow.Screen() == FlowScreen::DeckSetupPlayerTwo){
			const auto& decks = controller.m_flow.EditingDeck().Decks();
			for(const auto& deck : decks){
				HashValue(hash, deck.size());
				for(ElementType element : deck){
					HashValue(hash, static_cast<std::uint64_t>(element));
				}
			}
		}
		return hash == 0 ? 1 : hash;
	}

	void Rebuild(ElemenTacticsGameController* controller){
		ClearVisuals();
		m_lifetime = std::make_shared<VisualLifetime>();

		if(controller->m_localTurnHandoff && controller->m_flow.Screen() == FlowScreen::BattleBoard){
			BuildPrivacy(controller);
			return;
		}

		switch(controller->m_flow.Screen()){
		case FlowScreen::Title:
			BuildTitle(controller);
			break;
		case FlowScreen::ModeSelect:
			BuildModeSelect(controller);
			break;
		case FlowScreen::Rules:
			BuildRules(controller);
			break;
		case FlowScreen::DeckSetupPlayerOne:
		case FlowScreen::DeckSetupPlayerTwo:
			BuildDeckSetup(controller);
			break;
		case FlowScreen::LocalPrivacyHandoff:
			BuildPrivacy(controller);
			break;
		case FlowScreen::MatchIntroduction:
			BuildIntroduction(controller);
			break;
		case FlowScreen::BattleBoard:
			BuildBattle(controller);
			break;
		case FlowScreen::CenterReorder:
			BuildReorder(controller);
			break;
		case FlowScreen::Result:
			BuildResult(controller);
			break;
		default:
			break;
		}
	}

	void ClearVisuals(){
		if(!m_lifetime) return;
		m_lifetime->alive = false;
		for(const EntityRef& entity : m_lifetime->entities){
			if(entity.IsValid()) QueueDestroyEntity(entity.GetEntityID());
		}
		m_lifetime->entities.clear();
	}

	static Color Scale(Color color, float multiplier) noexcept{
		return {
			std::clamp(color.r * multiplier, 0.0f, 1.0f),
			std::clamp(color.g * multiplier, 0.0f, 1.0f),
			std::clamp(color.b * multiplier, 0.0f, 1.0f)
		};
	}

	Color ElementColor(const ElemenTacticsGameController* controller, ElementType element) const{
		const auto color = controller->ElementColor(element);
		return {color[0], color[1], color[2]};
	}

	void CreateVisualText(
		std::string name,
		std::string text,
		float x,
		float y,
		float width,
		float height,
		float fontSize,
		Color color,
		float alpha,
		int order,
		bool centered = true,
		std::string fontFamily = "Yu Gothic UI"
	){
		if(!m_lifetime || !m_lifetime->alive || width <= 0.0f || height <= 0.0f) return;

		const CommandEntity entity = QueueCreateEntity();
		QueueAddComponent<NameComponent>(entity);
		QueueAddComponent<TransformComponent>(entity);
		QueueAddComponent<TextureComponent>(entity);
		QueueAddComponent<RuntimeTextComponent>(entity);
		QueueAddComponent<SpriteRendererComponent>(entity);
		QueueAddComponent<RenderLayerComponent>(entity);
		QueueAddComponent<OrderInLayerComponent>(entity);

		const float viewWidth = std::max(1.0f, ResolveViewWidth());
		const float viewHeight = std::max(1.0f, ResolveViewHeight());
		const std::weak_ptr<VisualLifetime> weak = m_lifetime;
		QueueEntitySetup(entity,
			[weak, name = std::move(name), text = std::move(text), fontFamily = std::move(fontFamily),
			x, y, width, height, fontSize, color, alpha, order, centered, viewWidth, viewHeight]
			(Entity created, SceneContext& context){
				auto lifetime = weak.lock();
				if(!lifetime || !lifetime->alive || !context.component) return;

				if(NameComponent* component = context.component->GetComponent<NameComponent>(created)){
					component->name = name;
				}
				if(TransformComponent* transform = context.component->GetComponent<TransformComponent>(created)){
					transform->position = Vector3(x / viewWidth, y / viewHeight, 0.0f);
					transform->scale = Vector3(width / viewWidth, height / viewHeight, 1.0f);
				}
				if(SpriteRendererComponent* sprite = context.component->GetComponent<SpriteRendererComponent>(created)){
					sprite->anchor = Vector2(0.0f, 0.0f);
					sprite->pivot = Vector2(0.0f, 0.0f);
				}
				if(RuntimeTextComponent* runtimeText = context.component->GetComponent<RuntimeTextComponent>(created)){
					runtimeText->Text = text;
					runtimeText->FontFamily = fontFamily;
					runtimeText->FontSize = fontSize;
					runtimeText->PixelWidth = std::max(1, static_cast<int>(std::round(width)));
					runtimeText->PixelHeight = std::max(1, static_cast<int>(std::round(height)));
					runtimeText->ColorR = color.r;
					runtimeText->ColorG = color.g;
					runtimeText->ColorB = color.b;
					runtimeText->ColorA = std::clamp(alpha, 0.0f, 1.0f);
					runtimeText->Horizontal = centered
						? RuntimeTextComponent::HorizontalAlignment::Center
						: RuntimeTextComponent::HorizontalAlignment::Leading;
					runtimeText->Vertical = RuntimeTextComponent::VerticalAlignment::Center;
					runtimeText->WordWrap = false;
					runtimeText->AutoSizeTransform = true;
					runtimeText->MarkDirty();
				}
				if(RenderLayerComponent* layer = context.component->GetComponent<RenderLayerComponent>(created)){
					layer->layer = RenderLayer::OverlayUI;
				}
				if(OrderInLayerComponent* orderComponent = context.component->GetComponent<OrderInLayerComponent>(created)){
					orderComponent->order = order;
				}
				lifetime->entities.emplace_back(created, &context);
			});
	}

	float ResolveViewWidth() const{
		if(const ElemenTacticsGameController* controller = ResolveController()) return controller->ViewWidth();
		return 1280.0f;
	}

	float ResolveViewHeight() const{
		if(const ElemenTacticsGameController* controller = ResolveController()) return controller->ViewHeight();
		return 720.0f;
	}

	void CreateElementBadge(
		ElemenTacticsGameController* controller,
		ElementType element,
		float centerX,
		float centerY,
		float size,
		int order,
		bool label
	){
		const Color color = ElementColor(controller, element);
		CreateVisualText("ElementBadgeOuter", "⬢",
			centerX - size * 0.5f, centerY - size * 0.5f,
			size, size, size * 0.82f, Scale(color, 0.45f), 0.92f, order, true);
		CreateVisualText("ElementBadgeInner", controller->ElementSymbol(element),
			centerX - size * 0.34f, centerY - size * 0.34f,
			size * 0.68f, size * 0.68f, size * 0.34f, color, 1.0f, order + 1, true);
		if(label){
			CreateVisualText("ElementBadgeLabel", controller->ElementLabel(element),
				centerX - size * 0.5f, centerY + size * 0.34f,
				size, 24.0f, 16.0f, {0.90f, 0.93f, 1.0f}, 1.0f, order + 2, true);
		}
	}

	void CreateAffinityStrip(ElemenTacticsGameController* controller, float centerX, float y, float scale){
		const float badge = 42.0f * scale;
		const float step = 78.0f * scale;
		const std::array<ElementType, 5> elements{
			ElementType::Fire, ElementType::Wood, ElementType::Water,
			ElementType::Dark, ElementType::Light
		};
		const float startX = centerX - step * 2.0f;
		for(std::size_t index = 0; index < elements.size(); ++index){
			CreateElementBadge(controller, elements[index], startX + step * static_cast<float>(index), y,
				badge, 72, false);
		}
		CreateVisualText("AffinityArrow", "▶", startX + step * 0.43f, y - 16.0f, 28.0f, 32.0f,
			18.0f, {0.95f, 0.85f, 0.40f}, 0.95f, 74, true);
		CreateVisualText("AffinityArrow", "▶", startX + step * 1.43f, y - 16.0f, 28.0f, 32.0f,
			18.0f, {0.95f, 0.85f, 0.40f}, 0.95f, 74, true);
		CreateVisualText("AffinityDivider", "│", startX + step * 2.47f, y - 25.0f, 20.0f, 50.0f,
			28.0f, {0.50f, 0.58f, 0.72f}, 0.75f, 73, true);
		CreateVisualText("AffinityArrow", "▶", startX + step * 3.43f, y - 16.0f, 28.0f, 32.0f,
			18.0f, {0.95f, 0.85f, 0.40f}, 0.95f, 74, true);
	}

	void BuildTitle(ElemenTacticsGameController* controller){
		const float center = controller->ViewWidth() * 0.5f;
		const float spacing = 132.0f;
		const std::array<ElementType, 5> elements{
			ElementType::Fire, ElementType::Water, ElementType::Wood,
			ElementType::Dark, ElementType::Light
		};
		for(std::size_t index = 0; index < elements.size(); ++index){
			CreateElementBadge(controller, elements[index],
				center + (static_cast<float>(index) - 2.0f) * spacing,
				375.0f, 92.0f, 18, false);
		}
		CreateVisualText("TitleFlow", "火  ◀  水  ◀  木     闇  ▶  光",
			center - 310.0f, 420.0f, 620.0f, 38.0f, 20.0f,
			{0.82f, 0.88f, 1.0f}, 0.92f, 22, true);
	}

	void BuildModeSelect(ElemenTacticsGameController* controller){
		const float x = controller->ViewWidth() * 0.5f - 285.0f;
		CreateVisualText("ModeCpuBadge", "◇\nCPU", x, 198.0f, 64.0f, 76.0f, 22.0f,
			{0.34f, 0.78f, 1.0f}, 1.0f, 58, true);
		CreateVisualText("ModeLocalBadge", "● ●", x, 282.0f, 64.0f, 58.0f, 23.0f,
			{1.0f, 0.62f, 0.42f}, 1.0f, 58, true);
		CreateVisualText("ModeRulesBadge", "?", x, 378.0f, 64.0f, 64.0f, 44.0f,
			{0.76f, 0.90f, 0.50f}, 1.0f, 58, true);
	}

	void BuildRules(ElemenTacticsGameController* controller){
		const float right = controller->ViewWidth() - 218.0f;
		CreateElementBadge(controller, ElementType::Fire, right, 160.0f, 54.0f, 10, false);
		CreateVisualText("RuleArrow", "▶", right + 42.0f, 145.0f, 28.0f, 30.0f, 18.0f,
			{0.95f, 0.85f, 0.40f}, 0.9f, 12, true);
		CreateElementBadge(controller, ElementType::Wood, right + 84.0f, 160.0f, 54.0f, 10, false);

		CreateElementBadge(controller, ElementType::Wood, right, 232.0f, 54.0f, 10, false);
		CreateVisualText("RuleArrow", "▶", right + 42.0f, 217.0f, 28.0f, 30.0f, 18.0f,
			{0.95f, 0.85f, 0.40f}, 0.9f, 12, true);
		CreateElementBadge(controller, ElementType::Water, right + 84.0f, 232.0f, 54.0f, 10, false);

		CreateElementBadge(controller, ElementType::Water, right, 304.0f, 54.0f, 10, false);
		CreateVisualText("RuleArrow", "▶", right + 42.0f, 289.0f, 28.0f, 30.0f, 18.0f,
			{0.95f, 0.85f, 0.40f}, 0.9f, 12, true);
		CreateElementBadge(controller, ElementType::Fire, right + 84.0f, 304.0f, 54.0f, 10, false);

		CreateVisualText("RuleActions", "①  移動  ➜\n②  戦闘  ⚔\n③  偵察  ◎\n④  再編  ↻",
			right - 18.0f, 380.0f, 170.0f, 170.0f, 20.0f,
			{0.76f, 0.88f, 1.0f}, 0.92f, 12, false);
	}

	void BuildDeckSetup(ElemenTacticsGameController* controller){
		const auto& decks = controller->m_flow.EditingDeck().Decks();
		const float columnWidth = std::min(300.0f, (controller->ViewWidth() - 160.0f) / 3.0f);
		const float gap = (controller->ViewWidth() - columnWidth * 3.0f) / 4.0f;

		for(std::size_t slot = 0; slot < decks.size(); ++slot){
			const float x = gap + (columnWidth + gap) * static_cast<float>(slot);
			CreateVisualText("DeckPieceToken", slot == 0 ? "Ⅰ" : (slot == 1 ? "Ⅱ" : "Ⅲ"),
				x + columnWidth * 0.5f - 32.0f, 123.0f, 64.0f, 52.0f, 33.0f,
				{0.34f + static_cast<float>(slot) * 0.16f, 0.78f, 1.0f - static_cast<float>(slot) * 0.16f},
				0.72f, 16, true);

			const std::string meter = std::string(decks[slot].size(), '■') +
				std::string(8U - decks[slot].size(), '□');
			CreateVisualText("DeckCountMeter", meter,
				x + 18.0f, 161.0f, columnWidth - 36.0f, 22.0f, 16.0f,
				{0.50f, 0.72f, 0.96f}, 0.80f, 19, true);

			for(std::size_t index = 0; index < decks[slot].size(); ++index){
				const ElementType element = decks[slot][index];
				const Color color = ElementColor(controller, element);
				const float y = 180.0f + static_cast<float>(index) * 45.0f;
				const bool selected = controller->m_selectedDeckCard &&
					controller->m_selectedDeckCard->first == slot &&
					controller->m_selectedDeckCard->second == index;

				CreateVisualText("DeckCardPlate", "▰",
					x + 10.0f, y - 3.0f, columnWidth - 20.0f, 43.0f, 40.0f,
					Scale(color, selected ? 0.95f : 0.52f), selected ? 0.52f : 0.22f,
					selected ? 29 : 21, true);
				CreateVisualText("DeckCardEmblem", controller->ElementSymbol(element),
					x + columnWidth - 54.0f, y + 2.0f, 36.0f, 34.0f, 22.0f,
					color, 0.95f, 28, true);
				if(index == 0){
					CreateVisualText("DeckFrontMarker", "▲ 先頭",
						x + columnWidth - 96.0f, y - 18.0f, 80.0f, 20.0f, 13.0f,
						{1.0f, 0.82f, 0.32f}, 0.95f, 32, true);
				}
			}
		}

		const float legendY = controller->ViewHeight() - 205.0f;
		CreateAffinityStrip(controller, controller->ViewWidth() * 0.5f, legendY, 0.85f);
	}

	void BuildPrivacy(ElemenTacticsGameController* controller){
		const float center = controller->ViewWidth() * 0.5f;
		CreateVisualText("PrivacyCardBack", "▣", center - 100.0f, 300.0f, 200.0f, 170.0f, 150.0f,
			{0.24f, 0.32f, 0.48f}, 0.86f, 12, true);
		CreateVisualText("PrivacyLock", "×", center - 45.0f, 342.0f, 90.0f, 90.0f, 58.0f,
			{1.0f, 0.62f, 0.34f}, 0.95f, 14, true);
	}

	void BuildIntroduction(ElemenTacticsGameController* controller){
		const float center = controller->ViewWidth() * 0.5f;
		CreateVisualText("ActionPips", "●   ●", center - 110.0f, 374.0f, 220.0f, 68.0f, 46.0f,
			{1.0f, 0.82f, 0.30f}, 0.95f, 20, true);
		CreateVisualText("ActionFlow", "駒  ➜  マス     駒  ⚔  駒     駒  ◎  駒",
			center - 330.0f, 405.0f, 660.0f, 42.0f, 22.0f,
			{0.78f, 0.88f, 1.0f}, 0.96f, 21, true);
	}

	void BuildBattle(ElemenTacticsGameController* controller){
		if(!controller->m_flow.Match()) return;
		const GameState& state = *controller->m_flow.Match();
		const PlayerId viewer = controller->ActiveViewer();
		const PublicGameView view = ElemenTacticsRules::BuildPublicView(state, viewer);

		for(const BoardCell& cell : BoardCells){
			const ScreenPoint center = controller->m_boardLayout.CellCenter(cell.id);
			CreateVisualText("BoardCellRing", "⬡",
				center.x - 43.0f, center.y - 45.0f, 86.0f, 90.0f, 64.0f,
				cell.center ? Color{1.0f, 0.78f, 0.24f} : Color{0.28f, 0.46f, 0.72f},
				cell.center ? 0.94f : 0.44f, 18, true);

			const auto& occupant = view.occupancy[static_cast<std::size_t>(cell.id)];
			if(occupant){
				const bool playerOne = occupant->owner == PlayerId::One;
				CreateVisualText("PieceToken", playerOne ? "●" : "◆",
					center.x - 31.0f, center.y - 32.0f, 62.0f, 64.0f, 49.0f,
					playerOne ? Color{0.26f, 0.78f, 1.0f} : Color{1.0f, 0.40f, 0.30f},
					0.92f, 39, true);
			}
		}

		if(controller->m_interaction.SelectedPiece()){
			const PieceState* selected = ElemenTacticsRules::FindPiece(
				state, *controller->m_interaction.SelectedPiece());
			if(selected && selected->cell){
				const ScreenPoint center = controller->m_boardLayout.CellCenter(*selected->cell);
				CreateVisualText("SelectedPieceRing", "◎",
					center.x - 47.0f, center.y - 48.0f, 94.0f, 96.0f, 76.0f,
					{1.0f, 0.86f, 0.28f}, 0.96f, 44, true);
			}
		}

		const float pipX = controller->ViewWidth() * 0.5f + 265.0f;
		CreateVisualText("RemainingActions",
			state.actionsRemaining >= 2 ? "● ●" : (state.actionsRemaining == 1 ? "● ○" : "○ ○"),
			pipX, 11.0f, 100.0f, 42.0f, 24.0f,
			{1.0f, 0.82f, 0.30f}, 1.0f, 55, true);

		const ScreenRect left = controller->m_boardLayout.LeftHudBounds();
		float y = left.y + 86.0f;
		for(const PublicPieceView& piece : view.pieces[PlayerIndex(viewer)]){
			if(!piece.alive) continue;
			float x = left.x + 12.0f;
			for(ElementType element : piece.visibleDeck){
				const Color color = ElementColor(controller, element);
				CreateVisualText("VisibleDeckIcon", controller->ElementSymbol(element),
					x, y, 25.0f, 25.0f, 16.0f, color, 1.0f, 37, true);
				x += 24.0f;
			}
			y += 82.0f;
		}

		const ScreenRect footer = controller->m_boardLayout.FooterBounds();
		const bool scout = controller->m_interaction.Mode() == BattleInputMode::Scout;
		CreateVisualText("BattleModeEmblem", scout ? "◎" : "⚔",
			footer.x + 210.0f, footer.y + 1.0f, 58.0f, 42.0f, 28.0f,
			scout ? Color{0.48f, 0.92f, 0.58f} : Color{1.0f, 0.56f, 0.30f},
			1.0f, 48, true);
		CreateAffinityStrip(controller, controller->ViewWidth() * 0.5f, 70.0f, 0.62f);
	}

	void BuildReorder(ElemenTacticsGameController* controller){
		if(controller->m_reorderOrder.empty()) return;
		const float cardWidth = 118.0f;
		const float totalWidth = cardWidth * static_cast<float>(controller->m_reorderOrder.size());
		const float startX = controller->ViewWidth() * 0.5f - totalWidth * 0.5f;
		for(std::size_t index = 0; index < controller->m_reorderOrder.size(); ++index){
			const ElementType element = controller->m_reorderOrder[index];
			const Color color = ElementColor(controller, element);
			const bool selected = controller->m_selectedReorderCard && *controller->m_selectedReorderCard == index;
			CreateVisualText("ReorderCardPlate", "▰",
				startX + cardWidth * static_cast<float>(index), 250.0f,
				cardWidth - 8.0f, 140.0f, 105.0f,
				Scale(color, selected ? 0.85f : 0.48f), selected ? 0.60f : 0.28f,
				selected ? 34 : 29, true);
			if(index == 0){
				CreateVisualText("ReorderFront", "▲ 新しい先頭",
					startX - 10.0f, 218.0f, cardWidth + 12.0f, 28.0f, 15.0f,
					{1.0f, 0.84f, 0.30f}, 1.0f, 38, true);
			}
		}
		CreateVisualText("ReorderCycle", "↺   並べ替え   ↻",
			controller->ViewWidth() * 0.5f - 170.0f, 392.0f, 340.0f, 42.0f, 25.0f,
			{0.72f, 0.88f, 1.0f}, 0.95f, 34, true);
	}

	void BuildResult(ElemenTacticsGameController* controller){
		if(!controller->m_flow.Match()) return;
		const GameResult& result = controller->m_flow.Match()->result;
		const bool playerOne = result.winner == PlayerId::One;
		CreateVisualText("WinnerMedal", playerOne ? "●" : "◆",
			controller->ViewWidth() * 0.5f - 90.0f, 180.0f, 180.0f, 180.0f, 138.0f,
			playerOne ? Color{0.28f, 0.80f, 1.0f} : Color{1.0f, 0.42f, 0.30f},
			0.42f, 20, true);
		CreateVisualText("ResultCritical", result.reason == GameEndReason::KingLost ? "♛" : "†",
			controller->ViewWidth() * 0.5f - 55.0f, 225.0f, 110.0f, 110.0f, 72.0f,
			{1.0f, 0.82f, 0.28f}, 0.95f, 52, true);
	}

	std::shared_ptr<VisualLifetime> m_lifetime;
	std::uint64_t m_signature = std::numeric_limits<std::uint64_t>::max();
};

} // namespace ElemenTactics
