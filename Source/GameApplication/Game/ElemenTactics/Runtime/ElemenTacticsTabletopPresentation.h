#pragma once

#include "ElemenTacticsGameController.h"

#include "Component/LightComponent.h"
#include "Component/RenderLayerComponent.h"
#include "Component/entityNameComponent.h"
#include "Component/materialComponent.h"
#include "Component/modelRendererComponent.h"
#include "Component/transformComponent.h"
#include "Registry/componentRegistry.h"

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

// World-space presentation for the game. Rules, keyboard navigation and text
// remain owned by the existing runtime components; this class only mirrors the
// current state as a dark physical tabletop made from ordinary 3D entities.
class ElemenTacticsTabletopPresentation final : public CustomScriptComponent {
public:
	ElemenTacticsTabletopPresentation(){
		scriptName = "ElemenTacticsTabletopPresentation";
		SetExecutionOrder(SystemTaskDomain::Frame, SystemPhase::Default, 115);
	}

	void OnStart() override{
		m_staticLifetime = std::make_shared<Lifetime>();
		m_dynamicLifetime = std::make_shared<Lifetime>();
		m_signature = InvalidSignature;
		BuildRoom();
	}

	void OnUpdate(float) override{
		ElemenTacticsGameController* controller = ResolveController();
		if(!controller || !controller->m_started) return;

		const std::uint64_t signature = CalculateSignature(*controller);
		if(signature == m_signature) return;
		m_signature = signature;
		RebuildDynamic(*controller);
	}

	void OnStop() override{
		ClearLifetime(m_dynamicLifetime);
		ClearLifetime(m_staticLifetime);
		m_signature = InvalidSignature;
	}

private:
	static constexpr std::uint64_t InvalidSignature =
		(std::numeric_limits<std::uint64_t>::max)();
	static constexpr const char* CubeModelPath = "Asset/Model\\cube.obj";

	struct Lifetime {
		bool alive = true;
		std::vector<EntityRef> entities;
	};

	struct Color {
		float r = 1.0f;
		float g = 1.0f;
		float b = 1.0f;
		float a = 1.0f;
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
			Hash(value, static_cast<std::uint64_t>(state.currentPlayer));
			Hash(value, static_cast<std::uint64_t>(state.actionsRemaining));
			Hash(value, state.result.finished ? 1ULL : 0ULL);
		}

		const FlowScreen screen = controller.m_flow.Screen();
		if(screen == FlowScreen::DeckSetupPlayerOne ||
			screen == FlowScreen::DeckSetupPlayerTwo){
			for(const auto& deck : controller.m_flow.EditingDeck().Decks()){
				Hash(value, deck.size());
				for(ElementType element : deck){
					Hash(value, static_cast<std::uint64_t>(element));
				}
			}
		}
		for(ElementType element : controller.m_reorderOrder){
			Hash(value, 700ULL + static_cast<std::uint64_t>(element));
		}
		return value == 0 ? 1 : value;
	}

	void ClearLifetime(std::shared_ptr<Lifetime>& lifetime){
		if(!lifetime) return;
		lifetime->alive = false;
		for(const EntityRef& entity : lifetime->entities){
			if(entity.IsValid()) QueueDestroyEntity(entity.GetEntityID());
		}
		lifetime->entities.clear();
	}

	void RebuildDynamic(ElemenTacticsGameController& controller){
		ClearLifetime(m_dynamicLifetime);
		m_dynamicLifetime = std::make_shared<Lifetime>();

		if(controller.m_localTurnHandoff &&
			controller.m_flow.Screen() == FlowScreen::BattleBoard){
			BuildPrivacyCover();
			return;
		}

		switch(controller.m_flow.Screen()){
		case FlowScreen::Title: BuildTitle(); break;
		case FlowScreen::ModeSelect: BuildModeCards(); break;
		case FlowScreen::Rules: BuildRuleBook(); break;
		case FlowScreen::DeckSetupPlayerOne:
		case FlowScreen::DeckSetupPlayerTwo: BuildDeckSetup(controller); break;
		case FlowScreen::LocalPrivacyHandoff: BuildPrivacyCover(); break;
		case FlowScreen::MatchIntroduction: BuildVersusLayout(controller); break;
		case FlowScreen::BattleBoard: BuildBattle(controller); break;
		case FlowScreen::CenterReorder: BuildReorder(controller); break;
		case FlowScreen::Result: BuildResult(controller); break;
		default: BuildTitle(); break;
		}
	}

	static Color ElementColor(ElementType element) noexcept{
		switch(element){
		case ElementType::Fire: return {0.58f, 0.10f, 0.045f, 1.0f};
		case ElementType::Water: return {0.10f, 0.20f, 0.34f, 1.0f};
		case ElementType::Wood: return {0.18f, 0.29f, 0.11f, 1.0f};
		case ElementType::Dark: return {0.035f, 0.025f, 0.035f, 1.0f};
		case ElementType::Light: return {0.66f, 0.48f, 0.17f, 1.0f};
		default: return {0.28f, 0.20f, 0.12f, 1.0f};
		}
	}

	static Vector3 CellWorld(const BoardCell& cell) noexcept{
		const float x = (static_cast<float>(cell.q) +
			static_cast<float>(cell.r) * 0.5f) * 1.43f;
		const float z = static_cast<float>(cell.r) * 1.24f;
		return Vector3(x, 0.0f, z);
	}

	void CreateCube(
		const std::shared_ptr<Lifetime>& lifetime,
		std::string name,
		Vector3 position,
		Vector3 scale,
		Color color,
		float roughness = 0.82f,
		float metallic = 0.0f,
		Vector3 rotationEuler = Vector3(0.0f, 0.0f, 0.0f),
		Color emissive = Color{0.0f, 0.0f, 0.0f, 1.0f},
		float emissiveIntensity = 0.0f
	){
		if(!lifetime || !lifetime->alive) return;
		const CommandEntity entity = QueueCreateEntity();
		QueueAddComponent<NameComponent>(entity);
		QueueAddComponent<TransformComponent>(entity);
		QueueAddComponent<MaterialComponent>(entity);
		QueueAddComponent<ModelRendererComponent>(entity);
		QueueAddComponent<RenderLayerComponent>(entity);

		const std::weak_ptr<Lifetime> weak = lifetime;
		QueueEntitySetup(entity,
			[weak, name = std::move(name), position, scale, color, roughness,
			 metallic, rotationEuler, emissive, emissiveIntensity]
			(Entity created, SceneContext& context){
				auto locked = weak.lock();
				if(!locked || !locked->alive || !context.component) return;

				if(NameComponent* component =
					context.component->GetComponent<NameComponent>(created)){
					component->name = name;
				}
				if(TransformComponent* transform =
					context.component->GetComponent<TransformComponent>(created)){
					transform->position = position;
					transform->scale = scale;
					transform->SetRotationEuler(rotationEuler);
				}
				if(MaterialComponent* material =
					context.component->GetComponent<MaterialComponent>(created)){
					material->ShaderID = 1;
					material->Material.BaseColor =
						float4(color.r, color.g, color.b, color.a);
					material->Material.Metallic = metallic;
					material->Material.Roughness = roughness;
					material->Material.AO = 1.0f;
					material->Material.EmissiveColor =
						float3(emissive.r, emissive.g, emissive.b);
					material->Material.EmissiveIntensity = emissiveIntensity;
					material->Material.MaterialFlags = 0;
				}
				if(ModelRendererComponent* model =
					context.component->GetComponent<ModelRendererComponent>(created)){
					model->modelFilePath = CubeModelPath;
					model->isBlender = false;
					model->CreateModel(&context);
				}
				if(RenderLayerComponent* layer =
					context.component->GetComponent<RenderLayerComponent>(created)){
					layer->layer = RenderLayer::Opaque3D;
				}
				locked->entities.emplace_back(created, &context);
			});
	}

	void CreatePointLight(
		std::string name,
		Vector3 position,
		Color diffuse,
		float range,
		float ambientStrength
	){
		if(!m_staticLifetime || !m_staticLifetime->alive) return;
		const CommandEntity entity = QueueCreateEntity();
		QueueAddComponent<NameComponent>(entity);
		QueueAddComponent<TransformComponent>(entity);
		QueueAddComponent<LightComponent>(entity);
		const std::weak_ptr<Lifetime> weak = m_staticLifetime;
		QueueEntitySetup(entity,
			[weak, name = std::move(name), position, diffuse, range, ambientStrength]
			(Entity created, SceneContext& context){
				auto locked = weak.lock();
				if(!locked || !locked->alive || !context.component) return;
				if(NameComponent* component =
					context.component->GetComponent<NameComponent>(created)){
					component->name = name;
				}
				if(TransformComponent* transform =
					context.component->GetComponent<TransformComponent>(created)){
					transform->position = position;
				}
				if(LightComponent* light =
					context.component->GetComponent<LightComponent>(created)){
					light->light.Enable = 1;
					light->light.LightType = LIGHT_TYPE_POINT;
					light->light.CastShadow = 0;
					light->light.Position = float4(position.x, position.y, position.z, 1.0f);
					light->light.Diffuse =
						float4(diffuse.r, diffuse.g, diffuse.b, 1.0f);
					light->light.Ambient = float4(
						ambientStrength, ambientStrength * 0.72f,
						ambientStrength * 0.45f, 1.0f);
					light->light.Param = float4(range, 0.0f, 0.0f, 0.0f);
					light->dirty = true;
				}
				locked->entities.emplace_back(created, &context);
			});
	}

	void BuildRoom(){
		const Color wood{0.105f, 0.052f, 0.026f, 1.0f};
		const Color edge{0.035f, 0.018f, 0.012f, 1.0f};
		const Color inset{0.16f, 0.085f, 0.045f, 1.0f};
		CreateCube(m_staticLifetime, "TableSlab",
			Vector3(0.0f, -0.72f, 0.0f), Vector3(14.0f, 0.82f, 9.0f),
			wood, 0.92f);
		CreateCube(m_staticLifetime, "TableInset",
			Vector3(0.0f, -0.27f, 0.0f), Vector3(12.6f, 0.10f, 7.6f),
			inset, 0.98f);
		CreateCube(m_staticLifetime, "RailNear",
			Vector3(0.0f, -0.02f, -4.05f), Vector3(14.4f, 0.52f, 0.42f), edge, 0.82f);
		CreateCube(m_staticLifetime, "RailFar",
			Vector3(0.0f, -0.02f, 4.05f), Vector3(14.4f, 0.52f, 0.42f), edge, 0.82f);
		CreateCube(m_staticLifetime, "RailLeft",
			Vector3(-6.95f, -0.02f, 0.0f), Vector3(0.42f, 0.52f, 8.5f), edge, 0.82f);
		CreateCube(m_staticLifetime, "RailRight",
			Vector3(6.95f, -0.02f, 0.0f), Vector3(0.42f, 0.52f, 8.5f), edge, 0.82f);

		for(float side : {-1.0f, 1.0f}){
			const float x = side * 5.8f;
			CreateCube(m_staticLifetime, "CandleWax",
				Vector3(x, 0.30f, 2.8f), Vector3(0.32f, 1.12f, 0.32f),
				{0.58f, 0.47f, 0.30f, 1.0f}, 0.88f);
			CreateCube(m_staticLifetime, "CandleFlame",
				Vector3(x, 1.02f, 2.8f), Vector3(0.13f, 0.30f, 0.13f),
				{0.95f, 0.48f, 0.08f, 1.0f}, 0.35f, 0.0f,
				Vector3(0.0f, 0.0f, side * 0.08f),
				{1.0f, 0.32f, 0.035f, 1.0f}, 4.0f);
		}
		CreatePointLight("CandleLightLeft", Vector3(-5.8f, 2.0f, 2.8f),
			{1.0f, 0.32f, 0.08f, 1.0f}, 9.0f, 0.025f);
		CreatePointLight("CandleLightRight", Vector3(5.8f, 2.0f, 2.8f),
			{1.0f, 0.32f, 0.08f, 1.0f}, 9.0f, 0.025f);
		CreatePointLight("WarmFill", Vector3(0.0f, 5.5f, -1.0f),
			{0.36f, 0.20f, 0.10f, 1.0f}, 18.0f, 0.018f);
	}

	void CreateCard(
		std::string name,
		Vector3 position,
		ElementType element,
		bool selected,
		float yaw = 0.0f,
		float scale = 1.0f
	){
		const float lift = selected ? 0.22f : 0.0f;
		const Color parchment = selected
			? Color{0.62f, 0.45f, 0.22f, 1.0f}
			: Color{0.30f, 0.20f, 0.115f, 1.0f};
		CreateCube(m_dynamicLifetime, name + "Base",
			Vector3(position.x, position.y + lift, position.z),
			Vector3(0.92f * scale, 0.11f, 1.30f * scale),
			parchment, 0.94f, 0.0f, Vector3(0.0f, yaw, 0.0f),
			selected ? Color{0.72f, 0.33f, 0.07f, 1.0f} : Color{},
			selected ? 1.8f : 0.0f);
		const Color elementColor = ElementColor(element);
		CreateCube(m_dynamicLifetime, name + "Seal",
			Vector3(position.x, position.y + 0.10f + lift, position.z - 0.18f),
			Vector3(0.48f * scale, 0.055f, 0.48f * scale),
			elementColor, 0.72f, 0.0f, Vector3(0.0f, yaw, 0.0f),
			elementColor, selected ? 1.1f : 0.18f);
	}

	void BuildTitle(){
		const std::array<ElementType, 5> elements{
			ElementType::Fire, ElementType::Water, ElementType::Wood,
			ElementType::Dark, ElementType::Light};
		for(std::size_t index = 0; index < elements.size(); ++index){
			const float offset = static_cast<float>(index) - 2.0f;
			CreateCard("TitleCard" + std::to_string(index),
				Vector3(offset * 1.15f, 0.08f + std::abs(offset) * 0.025f, 0.15f),
				elements[index], index == 4, -offset * 0.10f, 1.12f);
		}
		CreateCube(m_dynamicLifetime, "TitlePlaque",
			Vector3(0.0f, 0.05f, -2.2f), Vector3(5.2f, 0.16f, 0.78f),
			{0.055f, 0.028f, 0.018f, 1.0f}, 0.78f, 0.05f);
	}

	void BuildModeCards(){
		for(int index = 0; index < 3; ++index){
			const float x = (static_cast<float>(index) - 1.0f) * 3.0f;
			CreateCube(m_dynamicLifetime, "ModeCard" + std::to_string(index),
				Vector3(x, 0.10f, 0.0f), Vector3(2.25f, 0.16f, 3.15f),
				index == 2 ? Color{0.22f, 0.16f, 0.085f, 1.0f}
					: Color{0.30f, 0.20f, 0.11f, 1.0f},
				0.94f, 0.0f, Vector3(0.0f, (static_cast<float>(index) - 1.0f) * -0.045f, 0.0f));
			CreateCube(m_dynamicLifetime, "ModeSeal" + std::to_string(index),
				Vector3(x, 0.22f, -0.20f), Vector3(0.72f, 0.06f, 0.72f),
				index == 0 ? Color{0.34f, 0.16f, 0.07f, 1.0f}
					: (index == 1 ? Color{0.40f, 0.08f, 0.045f, 1.0f}
						: Color{0.42f, 0.31f, 0.11f, 1.0f}),
				0.72f, 0.0f, Vector3(0.0f, 0.0f, 0.0f));
		}
	}

	void BuildRuleBook(){
		CreateCube(m_dynamicLifetime, "RuleBookLeft",
			Vector3(-1.65f, 0.10f, 0.0f), Vector3(3.15f, 0.15f, 4.0f),
			{0.34f, 0.24f, 0.14f, 1.0f}, 0.98f,
			0.0f, Vector3(0.0f, 0.045f, -0.02f));
		CreateCube(m_dynamicLifetime, "RuleBookRight",
			Vector3(1.65f, 0.10f, 0.0f), Vector3(3.15f, 0.15f, 4.0f),
			{0.34f, 0.24f, 0.14f, 1.0f}, 0.98f,
			0.0f, Vector3(0.0f, -0.045f, 0.02f));
		CreateCube(m_dynamicLifetime, "RuleBookSpine",
			Vector3(0.0f, 0.20f, 0.0f), Vector3(0.18f, 0.22f, 4.05f),
			{0.075f, 0.035f, 0.02f, 1.0f}, 0.72f);
	}

	void BuildDeckSetup(ElemenTacticsGameController& controller){
		const auto& decks = controller.m_flow.EditingDeck().Decks();
		for(std::size_t slot = 0; slot < decks.size(); ++slot){
			const float trayX = (static_cast<float>(slot) - 1.0f) * 4.0f;
			CreateCube(m_dynamicLifetime, "DeckTray" + std::to_string(slot),
				Vector3(trayX, 0.01f, 0.05f), Vector3(3.25f, 0.13f, 5.45f),
				{0.085f, 0.042f, 0.025f, 1.0f}, 0.90f);
			CreateCube(m_dynamicLifetime, "DeckTrayInset" + std::to_string(slot),
				Vector3(trayX, 0.09f, 0.05f), Vector3(2.85f, 0.035f, 5.05f),
				{0.18f, 0.105f, 0.055f, 1.0f}, 0.98f);

			for(std::size_t index = 0; index < decks[slot].size(); ++index){
				const float localX = (index % 2 == 0 ? -0.72f : 0.72f);
				const float localZ = -1.75f + static_cast<float>(index / 2) * 1.16f;
				const bool selected = controller.m_selectedDeckCard &&
					controller.m_selectedDeckCard->first == slot &&
					controller.m_selectedDeckCard->second == index;
				CreateCard("DeckCard" + std::to_string(slot) + "_" + std::to_string(index),
					Vector3(trayX + localX, 0.22f, localZ), decks[slot][index],
					selected, selected ? -0.04f : 0.0f, 0.78f);
			}
		}
	}

	void BuildPrivacyCover(){
		CreateCube(m_dynamicLifetime, "PrivacyCover",
			Vector3(0.0f, 0.18f, 0.0f), Vector3(7.5f, 0.22f, 5.1f),
			{0.028f, 0.014f, 0.012f, 1.0f}, 0.86f);
		CreateCube(m_dynamicLifetime, "PrivacySeal",
			Vector3(0.0f, 0.34f, 0.0f), Vector3(1.15f, 0.08f, 1.15f),
			{0.42f, 0.055f, 0.025f, 1.0f}, 0.58f, 0.0f,
			Vector3(0.0f, 0.0f, 0.0f), {0.40f, 0.035f, 0.015f, 1.0f}, 0.8f);
	}

	void BuildVersusLayout(ElemenTacticsGameController& controller){
		(void)controller;
		for(int side = -1; side <= 1; side += 2){
			for(int index = 0; index < 3; ++index){
				CreateCube(m_dynamicLifetime,
					"VersusPiece" + std::to_string(side) + "_" + std::to_string(index),
					Vector3(static_cast<float>(side) * 3.2f,
						0.35f, (static_cast<float>(index) - 1.0f) * 1.35f),
					Vector3(0.82f, 0.72f, 0.82f),
					side < 0 ? Color{0.46f, 0.37f, 0.24f, 1.0f}
						: Color{0.37f, 0.075f, 0.045f, 1.0f},
					0.66f, 0.05f);
			}
		}
		CreateCube(m_dynamicLifetime, "VersusDivider",
			Vector3(0.0f, 0.14f, 0.0f), Vector3(0.18f, 0.20f, 5.2f),
			{0.64f, 0.43f, 0.14f, 1.0f}, 0.56f,
			0.1f, Vector3(0.0f, 0.0f, 0.0f),
			{0.55f, 0.22f, 0.035f, 1.0f}, 0.8f);
	}

	void BuildBattle(ElemenTacticsGameController& controller){
		if(!controller.m_flow.Match()) return;
		const GameState& state = *controller.m_flow.Match();
		const PlayerId viewer = controller.ActiveViewer();
		const PublicGameView view = ElemenTacticsRules::BuildPublicView(state, viewer);

		for(const BoardCell& cell : BoardCells){
			const Vector3 world = CellWorld(cell);
			const bool center = cell.center;
			CreateCube(m_dynamicLifetime, "BoardTile" + std::to_string(cell.id),
				Vector3(world.x, 0.08f, world.z), Vector3(1.18f, 0.17f, 1.00f),
				center ? Color{0.40f, 0.25f, 0.075f, 1.0f}
					: Color{0.105f, 0.067f, 0.038f, 1.0f},
				center ? 0.66f : 0.94f,
				center ? 0.12f : 0.0f,
				Vector3(0.0f, 0.0f, 0.0f),
				center ? Color{0.42f, 0.19f, 0.025f, 1.0f} : Color{},
				center ? 0.8f : 0.0f);

			const auto& occupant = view.occupancy[static_cast<std::size_t>(cell.id)];
			if(!occupant) continue;
			const PublicPieceView& piece =
				view.pieces[PlayerIndex(occupant->owner)][occupant->slot];
			const bool selected = controller.m_interaction.SelectedPiece() &&
				*controller.m_interaction.SelectedPiece() == *occupant;
			const bool playerOne = occupant->owner == PlayerId::One;
			const float lift = selected ? 0.22f : 0.0f;
			if(selected){
				CreateCube(m_dynamicLifetime, "SelectedPlinth",
					Vector3(world.x, 0.28f, world.z), Vector3(0.88f, 0.10f, 0.78f),
					{0.58f, 0.36f, 0.08f, 1.0f}, 0.55f, 0.06f,
					Vector3(0.0f, 0.0f, 0.0f), {0.45f, 0.14f, 0.02f, 1.0f}, 1.2f);
			}
			CreateCube(m_dynamicLifetime, "Piece" + std::to_string(cell.id),
				Vector3(world.x, 0.52f + lift, world.z), Vector3(0.64f, 0.72f, 0.64f),
				playerOne ? Color{0.48f, 0.39f, 0.26f, 1.0f}
					: Color{0.42f, 0.065f, 0.038f, 1.0f},
				0.60f, 0.08f);
			CreateCube(m_dynamicLifetime, "PieceCrest" + std::to_string(cell.id),
				Vector3(world.x, 0.93f + lift, world.z), Vector3(0.32f, 0.12f, 0.32f),
				playerOne ? Color{0.74f, 0.58f, 0.31f, 1.0f}
					: Color{0.64f, 0.17f, 0.07f, 1.0f},
				0.45f, 0.16f);

			const ElementType marker = piece.visibleDeck.empty()
				? ElementType::Dark
				: piece.visibleDeck.front();
			CreateCube(m_dynamicLifetime, "PieceFrontCard" + std::to_string(cell.id),
				Vector3(world.x, 1.10f + lift, world.z - 0.05f),
				Vector3(0.30f, 0.07f, 0.42f),
				piece.visibleDeck.empty()
					? Color{0.025f, 0.018f, 0.020f, 1.0f}
					: ElementColor(marker),
				0.70f, 0.0f);
		}

		for(int index = 0; index < 2; ++index){
			const bool active = index < state.actionsRemaining;
			CreateCube(m_dynamicLifetime, "ActionToken" + std::to_string(index),
				Vector3(-0.55f + static_cast<float>(index) * 1.1f, 0.18f, -3.25f),
				Vector3(0.50f, 0.20f, 0.50f),
				active ? Color{0.58f, 0.37f, 0.09f, 1.0f}
					: Color{0.055f, 0.035f, 0.025f, 1.0f},
				active ? 0.48f : 0.95f, active ? 0.12f : 0.0f,
				Vector3(0.0f, 0.0f, 0.0f),
				active ? Color{0.48f, 0.16f, 0.02f, 1.0f} : Color{},
				active ? 0.7f : 0.0f);
		}
	}

	void BuildReorder(ElemenTacticsGameController& controller){
		const float spacing = 1.35f;
		const float start = -spacing * static_cast<float>(
			controller.m_reorderOrder.size() > 0 ? controller.m_reorderOrder.size() - 1 : 0) * 0.5f;
		for(std::size_t index = 0; index < controller.m_reorderOrder.size(); ++index){
			const bool selected = controller.m_selectedReorderCard &&
				*controller.m_selectedReorderCard == index;
			CreateCard("ReorderCard" + std::to_string(index),
				Vector3(start + spacing * static_cast<float>(index), 0.16f, 0.0f),
				controller.m_reorderOrder[index], selected, 0.0f, 0.98f);
		}
	}

	void BuildResult(ElemenTacticsGameController& controller){
		if(!controller.m_flow.Match()) return;
		const GameResult& result = controller.m_flow.Match()->result;
		const bool playerOne = result.winner == PlayerId::One;
		CreateCube(m_dynamicLifetime, "ResultAltar",
			Vector3(0.0f, 0.12f, 0.0f), Vector3(4.2f, 0.26f, 3.0f),
			{0.055f, 0.025f, 0.018f, 1.0f}, 0.72f, 0.08f);
		CreateCube(m_dynamicLifetime, "WinnerIdol",
			Vector3(0.0f, 1.0f, 0.0f), Vector3(1.15f, 1.75f, 1.15f),
			playerOne ? Color{0.52f, 0.42f, 0.26f, 1.0f}
				: Color{0.45f, 0.055f, 0.03f, 1.0f},
			0.48f, 0.18f,
			Vector3(0.0f, 0.0f, 0.0f),
			playerOne ? Color{0.46f, 0.22f, 0.04f, 1.0f}
				: Color{0.40f, 0.025f, 0.012f, 1.0f},
			1.1f);
	}

	std::shared_ptr<Lifetime> m_staticLifetime;
	std::shared_ptr<Lifetime> m_dynamicLifetime;
	std::uint64_t m_signature = InvalidSignature;
};

} // namespace ElemenTactics
