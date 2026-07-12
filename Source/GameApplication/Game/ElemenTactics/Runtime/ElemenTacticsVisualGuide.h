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

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace ElemenTactics {

// Code-generated presentation layer. It produces card accents, element badges,
// piece tokens, selection rings and compact diagrams with RuntimeText/Sprite
// entities. No external image asset is required.
class ElemenTacticsVisualGuide final : public CustomScriptComponent {
public:
	ElemenTacticsVisualGuide(){
		scriptName = "ElemenTacticsVisualGuide";
		SetExecutionOrder(SystemTaskDomain::Frame, SystemPhase::Default, 120);
	}

	void OnStart() override{
		m_lifetime = std::make_shared<Lifetime>();
		m_signature = InvalidSignature;
	}

	void OnUpdate(float) override{
		ElemenTacticsGameController* controller = ResolveController();
		if(!controller || !controller->m_started) return;

		const std::uint64_t nextSignature = CalculateSignature(*controller);
		if(nextSignature != m_signature){
			m_signature = nextSignature;
			Rebuild(*controller);
		}

		if(controller->m_textSystem){
			controller->m_textSystem->ProcessDirtyText();
		}
	}

	void OnStop() override{
		Clear();
		m_signature = InvalidSignature;
	}

private:
	static constexpr std::uint64_t InvalidSignature =
		std::numeric_limits<std::uint64_t>::max();

	struct Lifetime {
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
		return context->component->GetComponent<ElemenTacticsGameController>(
			self.GetEntityID());
	}

	static void Hash(std::uint64_t& value, std::uint64_t part) noexcept{
		constexpr std::uint64_t prime = 1099511628211ULL;
		for(int byte = 0; byte < 8; ++byte){
			value ^= static_cast<unsigned char>((part >> (byte * 8)) & 0xffULL);
			value *= prime;
		}
	}

	std::uint64_t CalculateSignature(
		const ElemenTacticsGameController& controller
	) const{
		std::uint64_t value = 1469598103934665603ULL;
		Hash(value, static_cast<std::uint64_t>(controller.m_flow.Screen()));
		Hash(value, controller.m_localTurnHandoff ? 1ULL : 0ULL);
		Hash(value, static_cast<std::uint64_t>(std::llround(controller.ViewWidth())));
		Hash(value, static_cast<std::uint64_t>(std::llround(controller.ViewHeight())));
		Hash(value, static_cast<std::uint64_t>(controller.m_interaction.Mode()));

		if(controller.m_selectedDeckCard){
			Hash(value, 100ULL + controller.m_selectedDeckCard->first);
			Hash(value, 200ULL + controller.m_selectedDeckCard->second);
		}
		if(controller.m_selectedReorderCard){
			Hash(value, 300ULL + *controller.m_selectedReorderCard);
		}
		if(controller.m_interaction.SelectedPiece()){
			const PieceId piece = *controller.m_interaction.SelectedPiece();
			Hash(value, 400ULL + PlayerIndex(piece.owner));
			Hash(value, 500ULL + piece.slot);
		}

		if(controller.m_flow.Match()){
			const GameState& state = *controller.m_flow.Match();
			Hash(value, state.actionSerial);
			Hash(value, static_cast<std::uint64_t>(state.actionsRemaining));
			Hash(value, static_cast<std::uint64_t>(state.currentPlayer));
			Hash(value, state.result.finished ? 1ULL : 0ULL);
			if(state.pendingReorder){
				Hash(value, 600ULL + PlayerIndex(state.pendingReorder->piece.owner));
				Hash(value, 700ULL + state.pendingReorder->piece.slot);
			}
		}

		const FlowScreen screen = controller.m_flow.Screen();
		if(screen == FlowScreen::DeckSetupPlayerOne ||
			screen == FlowScreen::DeckSetupPlayerTwo){
			const auto& decks = controller.m_flow.EditingDeck().Decks();
			for(const auto& deck : decks){
				Hash(value, static_cast<std::uint64_t>(deck.size()));
				for(ElementType element : deck){
					Hash(value, static_cast<std::uint64_t>(element));
				}
			}
		}
		return value == 0 ? 1 : value;
	}

	void Rebuild(ElemenTacticsGameController& controller){
		Clear();
		m_lifetime = std::make_shared<Lifetime>();
		m_viewWidth = std::max(1.0f, controller.ViewWidth());
		m_viewHeight = std::max(1.0f, controller.ViewHeight());

		if(controller.m_localTurnHandoff &&
			controller.m_flow.Screen() == FlowScreen::BattleBoard){
			BuildPrivacy(controller);
			return;
		}

		switch(controller.m_flow.Screen()){
		case FlowScreen::Title: BuildTitle(controller); break;
		case FlowScreen::ModeSelect: BuildModeSelect(controller); break;
		case FlowScreen::Rules: BuildRules(controller); break;
		case FlowScreen::DeckSetupPlayerOne:
		case FlowScreen::DeckSetupPlayerTwo: BuildDeckSetup(controller); break;
		case FlowScreen::LocalPrivacyHandoff: BuildPrivacy(controller); break;
		case FlowScreen::MatchIntroduction: BuildIntroduction(controller); break;
		case FlowScreen::BattleBoard: BuildBattle(controller); break;
		case FlowScreen::CenterReorder: BuildReorder(controller); break;
		case FlowScreen::Result: BuildResult(controller); break;
		default: break;
		}
	}

	void Clear(){
		if(!m_lifetime) return;
		m_lifetime->alive = false;
		for(const EntityRef& entity : m_lifetime->entities){
			if(entity.IsValid()) QueueDestroyEntity(entity.GetEntityID());
		}
		m_lifetime->entities.clear();
	}

	static Color Dim(Color color, float scale) noexcept{
		return {
			std::clamp(color.r * scale, 0.0f, 1.0f),
			std::clamp(color.g * scale, 0.0f, 1.0f),
			std::clamp(color.b * scale, 0.0f, 1.0f)
		};
	}

	Color GetElementColor(
		const ElemenTacticsGameController& controller,
		ElementType element
	) const{
		const auto color = controller.ElementColor(element);
		return {color[0], color[1], color[2]};
	}

	void Create(
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
		if(!m_lifetime || !m_lifetime->alive || width <= 0.0f || height <= 0.0f){
			return;
		}

		const CommandEntity entity = QueueCreateEntity();
		QueueAddComponent<NameComponent>(entity);
		QueueAddComponent<TransformComponent>(entity);
		QueueAddComponent<TextureComponent>(entity);
		QueueAddComponent<RuntimeTextComponent>(entity);
		QueueAddComponent<SpriteRendererComponent>(entity);
		QueueAddComponent<RenderLayerComponent>(entity);
		QueueAddComponent<OrderInLayerComponent>(entity);

		const std::weak_ptr<Lifetime> weak = m_lifetime;
		const float viewWidth = m_viewWidth;
		const float viewHeight = m_viewHeight;
		QueueEntitySetup(entity,
			[weak, name = std::move(name), text = std::move(text),
			fontFamily = std::move(fontFamily), x, y, width, height,
			fontSize, color, alpha, order, centered, viewWidth, viewHeight]
			(Entity created, SceneContext& context){
				auto lifetime = weak.lock();
				if(!lifetime || !lifetime->alive || !context.component) return;

				if(NameComponent* component =
					context.component->GetComponent<NameComponent>(created)){
					component->name = name;
				}
				if(TransformComponent* transform =
					context.component->GetComponent<TransformComponent>(created)){
					transform->position = Vector3(x / viewWidth, y / viewHeight, 0.0f);
					transform->scale = Vector3(width / viewWidth, height / viewHeight, 1.0f);
				}
				if(SpriteRendererComponent* sprite =
					context.component->GetComponent<SpriteRendererComponent>(created)){
					sprite->anchor = Vector2(0.0f, 0.0f);
					sprite->pivot = Vector2(0.0f, 0.0f);
				}
				if(RuntimeTextComponent* runtimeText =
					context.component->GetComponent<RuntimeTextComponent>(created)){
					runtimeText->Text = text;
					runtimeText->FontFamily = fontFamily;
					runtimeText->FontSize = fontSize;
					runtimeText->PixelWidth =
						std::max(1, static_cast<int>(std::round(width)));
					runtimeText->PixelHeight =
						std::max(1, static_cast<int>(std::round(height)));
					runtimeText->ColorR = color.r;
					runtimeText->ColorG = color.g;
					runtimeText->ColorB = color.b;
					runtimeText->ColorA = std::clamp(alpha, 0.0f, 1.0f);
					runtimeText->Horizontal = centered
						? RuntimeTextComponent::HorizontalAlignment::Center
						: RuntimeTextComponent::HorizontalAlignment::Leading;
					runtimeText->Vertical =
						RuntimeTextComponent::VerticalAlignment::Center;
					runtimeText->WordWrap = false;
					runtimeText->AutoSizeTransform = true;
					runtimeText->MarkDirty();
				}
				if(RenderLayerComponent* layer =
					context.component->GetComponent<RenderLayerComponent>(created)){
					layer->layer = RenderLayer::OverlayUI;
				}
				if(OrderInLayerComponent* orderComponent =
					context.component->GetComponent<OrderInLayerComponent>(created)){
					orderComponent->order = order;
				}
				lifetime->entities.emplace_back(created, &context);
			});
	}

	void Badge(
		ElemenTacticsGameController& controller,
		ElementType element,
		float centerX,
		float centerY,
		float size,
		int order,
		bool showLabel = false
	){
		const Color color = GetElementColor(controller, element);
		Create("ElementBadgeOuter", "⬢",
			centerX - size * 0.5f, centerY - size * 0.5f,
			size, size, size * 0.82f, Dim(color, 0.45f), 0.92f, order);
		Create("ElementBadgeInner", controller.ElementSymbol(element),
			centerX - size * 0.33f, centerY - size * 0.33f,
			size * 0.66f, size * 0.66f, size * 0.34f,
			color, 1.0f, order + 1);
		if(showLabel){
			Create("ElementBadgeLabel", controller.ElementLabel(element),
				centerX - size * 0.5f, centerY + size * 0.34f,
				size, 22.0f, 15.0f, {0.90f, 0.94f, 1.0f}, 1.0f,
				order + 2);
		}
	}

	void AffinityStrip(
		ElemenTacticsGameController& controller,
		float centerX,
		float y,
		float scale
	){
		const float badgeSize = 42.0f * scale;
		const float step = 76.0f * scale;
		const float start = centerX - step * 2.0f;
		const std::array<ElementType, 5> elements{
			ElementType::Fire,
			ElementType::Wood,
			ElementType::Water,
			ElementType::Dark,
			ElementType::Light
		};
		for(std::size_t index = 0; index < elements.size(); ++index){
			Badge(controller, elements[index],
				start + step * static_cast<float>(index), y,
				badgeSize, 72, false);
		}
		for(int index : {0, 1, 3}){
			Create("AffinityArrow", "▶",
				start + step * (static_cast<float>(index) + 0.45f),
				y - 15.0f, 26.0f, 30.0f, 17.0f,
				{1.0f, 0.84f, 0.30f}, 0.95f, 75);
		}
		Create("AffinityDivider", "│", start + step * 2.48f,
			y - 23.0f, 18.0f, 46.0f, 27.0f,
			{0.48f, 0.58f, 0.74f}, 0.72f, 74);
	}

	static std::string DeckMeter(std::size_t count){
		std::string result;
		for(std::size_t index = 0; index < 8; ++index){
			result += index < count ? "■" : "□";
		}
		return result;
	}

	void BuildTitle(ElemenTacticsGameController& controller){
		const float center = controller.ViewWidth() * 0.5f;
		const float spacing = 132.0f;
		const std::array<ElementType, 5> elements{
			ElementType::Fire,
			ElementType::Water,
			ElementType::Wood,
			ElementType::Dark,
			ElementType::Light
		};
		for(std::size_t index = 0; index < elements.size(); ++index){
			Badge(controller, elements[index],
				center + (static_cast<float>(index) - 2.0f) * spacing,
				375.0f, 92.0f, 18, false);
		}
		Create("TitleAffinity", "火  ◀  水  ◀  木      闇  ▶  光",
			center - 310.0f, 420.0f, 620.0f, 38.0f, 20.0f,
			{0.82f, 0.90f, 1.0f}, 0.94f, 22);
	}

	void BuildModeSelect(ElemenTacticsGameController& controller){
		const float x = controller.ViewWidth() * 0.5f - 288.0f;
		Create("ModeCpu", "◇\nCPU", x, 198.0f, 68.0f, 76.0f, 22.0f,
			{0.34f, 0.80f, 1.0f}, 1.0f, 58);
		Create("ModeLocal", "● ●", x, 282.0f, 68.0f, 58.0f, 23.0f,
			{1.0f, 0.58f, 0.38f}, 1.0f, 58);
		Create("ModeRules", "?", x, 378.0f, 68.0f, 64.0f, 44.0f,
			{0.74f, 0.92f, 0.48f}, 1.0f, 58);
	}

	void BuildRules(ElemenTacticsGameController& controller){
		const float x = controller.ViewWidth() - 210.0f;
		const std::array<std::pair<ElementType, ElementType>, 3> pairs{{
			{ElementType::Fire, ElementType::Wood},
			{ElementType::Wood, ElementType::Water},
			{ElementType::Water, ElementType::Fire}
		}};
		for(std::size_t index = 0; index < pairs.size(); ++index){
			const float y = 158.0f + static_cast<float>(index) * 72.0f;
			Badge(controller, pairs[index].first, x, y, 52.0f, 10, false);
			Create("RulesArrow", "▶", x + 40.0f, y - 14.0f,
				26.0f, 28.0f, 17.0f, {1.0f, 0.84f, 0.30f}, 0.95f, 12);
			Badge(controller, pairs[index].second, x + 80.0f, y, 52.0f, 10, false);
		}
		Create("RulesActions", "移動  ➜\n戦闘  ⚔\n偵察  ◎\n再編  ↻",
			x - 25.0f, 376.0f, 170.0f, 172.0f, 21.0f,
			{0.72f, 0.88f, 1.0f}, 0.94f, 12, false);
	}

	void BuildDeckSetup(ElemenTacticsGameController& controller){
		const auto& decks = controller.m_flow.EditingDeck().Decks();
		const float columnWidth =
			std::min(300.0f, (controller.ViewWidth() - 160.0f) / 3.0f);
		const float gap =
			(controller.ViewWidth() - columnWidth * 3.0f) / 4.0f;

		for(std::size_t slot = 0; slot < decks.size(); ++slot){
			const float x = gap + (columnWidth + gap) * static_cast<float>(slot);
			const char* numeral = slot == 0 ? "Ⅰ" : (slot == 1 ? "Ⅱ" : "Ⅲ");
			Create("DeckPieceToken", numeral,
				x + columnWidth * 0.5f - 32.0f, 123.0f,
				64.0f, 50.0f, 32.0f,
				{0.34f + static_cast<float>(slot) * 0.16f,
				0.78f,
				1.0f - static_cast<float>(slot) * 0.16f},
				0.72f, 16);
			Create("DeckCountMeter", DeckMeter(decks[slot].size()),
				x + 18.0f, 160.0f, columnWidth - 36.0f, 23.0f,
				16.0f, {0.48f, 0.72f, 0.98f}, 0.82f, 19);

			for(std::size_t index = 0; index < decks[slot].size(); ++index){
				const ElementType element = decks[slot][index];
				const Color color = GetElementColor(controller, element);
				const float y = 180.0f + static_cast<float>(index) * 45.0f;
				const bool selected = controller.m_selectedDeckCard &&
					controller.m_selectedDeckCard->first == slot &&
					controller.m_selectedDeckCard->second == index;
				Create("DeckCardPlate", "▰",
					x + 10.0f, y - 3.0f, columnWidth - 20.0f, 43.0f, 40.0f,
					Dim(color, selected ? 0.95f : 0.52f),
					selected ? 0.52f : 0.22f,
					selected ? 29 : 21);
				Create("DeckCardIcon", controller.ElementSymbol(element),
					x + columnWidth - 53.0f, y + 2.0f,
					35.0f, 34.0f, 22.0f, color, 0.98f, 28);
				if(index == 0){
					Create("DeckFront", "▲ 先頭",
						x + columnWidth - 98.0f, y - 18.0f,
						82.0f, 20.0f, 13.0f,
						{1.0f, 0.82f, 0.30f}, 0.98f, 32);
				}
			}
		}

		AffinityStrip(controller, controller.ViewWidth() * 0.5f,
			controller.ViewHeight() - 205.0f, 0.85f);
	}

	void BuildPrivacy(ElemenTacticsGameController& controller){
		const float center = controller.ViewWidth() * 0.5f;
		Create("PrivacyCard", "▣", center - 100.0f, 300.0f,
			200.0f, 170.0f, 150.0f,
			{0.24f, 0.34f, 0.52f}, 0.86f, 12);
		Create("PrivacyCross", "×", center - 45.0f, 342.0f,
			90.0f, 90.0f, 58.0f,
			{1.0f, 0.58f, 0.32f}, 0.96f, 14);
	}

	void BuildIntroduction(ElemenTacticsGameController& controller){
		const float center = controller.ViewWidth() * 0.5f;
		Create("ActionPips", "●   ●", center - 110.0f, 372.0f,
			220.0f, 68.0f, 46.0f,
			{1.0f, 0.82f, 0.30f}, 0.96f, 20);
		Create("ActionFlow", "駒  ➜  マス     駒  ⚔  駒     駒  ◎  駒",
			center - 330.0f, 408.0f, 660.0f, 42.0f, 22.0f,
			{0.76f, 0.88f, 1.0f}, 0.96f, 21);
	}

	void BuildBattle(ElemenTacticsGameController& controller){
		if(!controller.m_flow.Match()) return;
		const GameState& state = *controller.m_flow.Match();
		const PlayerId viewer = controller.ActiveViewer();
		const PublicGameView view =
			ElemenTacticsRules::BuildPublicView(state, viewer);

		for(const BoardCell& cell : BoardCells){
			const ScreenPoint center = controller.m_boardLayout.CellCenter(cell.id);
			Create("BoardRing", "⬡",
				center.x - 43.0f, center.y - 45.0f,
				86.0f, 90.0f, 64.0f,
				cell.center ? Color{1.0f, 0.78f, 0.24f}
					: Color{0.28f, 0.48f, 0.76f},
				cell.center ? 0.94f : 0.44f, 18);

			const auto& occupant =
				view.occupancy[static_cast<std::size_t>(cell.id)];
			if(occupant){
				const bool playerOne = occupant->owner == PlayerId::One;
				Create("PieceToken", playerOne ? "●" : "◆",
					center.x - 31.0f, center.y - 32.0f,
					62.0f, 64.0f, 49.0f,
					playerOne ? Color{0.26f, 0.80f, 1.0f}
						: Color{1.0f, 0.40f, 0.30f},
					0.92f, 39);
			}
		}

		if(controller.m_interaction.SelectedPiece()){
			const PieceId selectedId = *controller.m_interaction.SelectedPiece();
			if(selectedId.slot < 3){
				const PieceState& selected =
					state.players[PlayerIndex(selectedId.owner)].pieces[selectedId.slot];
				if(selected.cell){
					const ScreenPoint center =
						controller.m_boardLayout.CellCenter(*selected.cell);
					Create("SelectedRing", "◎",
						center.x - 47.0f, center.y - 48.0f,
						94.0f, 96.0f, 76.0f,
						{1.0f, 0.86f, 0.28f}, 0.96f, 44);
				}
			}
		}

		const float pipX = controller.ViewWidth() * 0.5f + 265.0f;
		const char* pips = state.actionsRemaining >= 2
			? "● ●"
			: (state.actionsRemaining == 1 ? "● ○" : "○ ○");
		Create("RemainingActions", pips, pipX, 11.0f,
			100.0f, 42.0f, 24.0f,
			{1.0f, 0.82f, 0.30f}, 1.0f, 55);

		const ScreenRect left = controller.m_boardLayout.LeftHudBounds();
		float y = left.y + 86.0f;
		for(const PublicPieceView& piece : view.pieces[PlayerIndex(viewer)]){
			if(!piece.alive) continue;
			float x = left.x + 12.0f;
			for(ElementType element : piece.visibleDeck){
				Create("DeckMiniIcon", controller.ElementSymbol(element),
					x, y, 25.0f, 25.0f, 16.0f,
					GetElementColor(controller, element), 1.0f, 37);
				x += 24.0f;
			}
			y += 82.0f;
		}

		const ScreenRect footer = controller.m_boardLayout.FooterBounds();
		const bool scout =
			controller.m_interaction.Mode() == BattleInputMode::Scout;
		Create("BattleMode", scout ? "◎" : "⚔",
			footer.x + 210.0f, footer.y + 1.0f,
			58.0f, 42.0f, 28.0f,
			scout ? Color{0.48f, 0.92f, 0.58f}
				: Color{1.0f, 0.56f, 0.30f},
			1.0f, 48);
		AffinityStrip(controller, controller.ViewWidth() * 0.5f, 70.0f, 0.62f);
	}

	void BuildReorder(ElemenTacticsGameController& controller){
		if(controller.m_reorderOrder.empty()) return;
		const float cardWidth = 118.0f;
		const float totalWidth =
			cardWidth * static_cast<float>(controller.m_reorderOrder.size());
		const float startX = controller.ViewWidth() * 0.5f - totalWidth * 0.5f;
		for(std::size_t index = 0; index < controller.m_reorderOrder.size(); ++index){
			const ElementType element = controller.m_reorderOrder[index];
			const Color color = GetElementColor(controller, element);
			const bool selected = controller.m_selectedReorderCard &&
				*controller.m_selectedReorderCard == index;
			Create("ReorderPlate", "▰",
				startX + cardWidth * static_cast<float>(index), 250.0f,
				cardWidth - 8.0f, 140.0f, 105.0f,
				Dim(color, selected ? 0.85f : 0.48f),
				selected ? 0.60f : 0.28f,
				selected ? 34 : 29);
			if(index == 0){
				Create("ReorderFront", "▲ 新しい先頭",
					startX - 10.0f, 218.0f,
					cardWidth + 12.0f, 28.0f, 15.0f,
					{1.0f, 0.84f, 0.30f}, 1.0f, 38);
			}
		}
		Create("ReorderCycle", "↺   並べ替え   ↻",
			controller.ViewWidth() * 0.5f - 170.0f, 392.0f,
			340.0f, 42.0f, 25.0f,
			{0.72f, 0.88f, 1.0f}, 0.95f, 34);
	}

	void BuildResult(ElemenTacticsGameController& controller){
		if(!controller.m_flow.Match()) return;
		const GameResult& result = controller.m_flow.Match()->result;
		const bool playerOne = result.winner == PlayerId::One;
		Create("WinnerMedal", playerOne ? "●" : "◆",
			controller.ViewWidth() * 0.5f - 90.0f, 180.0f,
			180.0f, 180.0f, 138.0f,
			playerOne ? Color{0.28f, 0.80f, 1.0f}
				: Color{1.0f, 0.42f, 0.30f},
			0.42f, 20);
		Create("ResultCritical",
			result.reason == GameEndReason::KingLost ? "♛" : "†",
			controller.ViewWidth() * 0.5f - 55.0f, 225.0f,
			110.0f, 110.0f, 72.0f,
			{1.0f, 0.82f, 0.28f}, 0.95f, 52);
	}

	std::shared_ptr<Lifetime> m_lifetime;
	std::uint64_t m_signature = InvalidSignature;
	float m_viewWidth = 1280.0f;
	float m_viewHeight = 720.0f;
};

} // namespace ElemenTactics
