#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/2DspriteRendererComponent.h"
#include "Engine/Scene/Component/textureComponent.h"
#include "Engine/Scene/Component/materialComponent.h"
#include "Engine/Scene/Component/entityNameComponent.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerGameManager.h"
#include "Game/Platformer/PlatformerSceneAccess.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

class PlatformerHud : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerHud)
		REFLECT_FIELD(float, instructionDuration, 8.0f)
		REFLECT_FIELD(bool, showControls, true)

public:
	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		DECODE_FIELDS(node);
		return true;
	}

	void inspector(SceneContext* context) override {
		ImGui::Text("Platformer HUD (Sprite UI)");
		INSPECTOR_FIELDS();
		ImGui::Text("Runtime elements: %s", uiReady ? "ready" : "queued");
	}

	void OnStart() override {
		manager = PlatformerSceneAccess::FindFirst<PlatformerGameManager>(m_ref.GetScene());
		player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		elapsed = 0.0f;
		QueueUi();
	}

	void OnUpdate(float dt) override {
		elapsed += (std::max)(0.0f, dt);
		if(!manager.IsValid()) manager = PlatformerSceneAccess::FindFirst<PlatformerGameManager>(m_ref.GetScene());
		if(!player.IsValid()) player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		UpdateUi();
	}

	void OnDraw() override {
		// Game HUD is rendered entirely by SpriteRendererComponent.
		// ImGui is intentionally reserved for the editor inspector only.
	}

	void OnStop() override {
		for(Element& element : elements) {
			if(element.entity.IsValid()) QueueDestroyEntity(element.entity.GetEntityID());
			element = {};
		}
		uiReady = false;
		uiQueued = false;
	}

private:
	enum Slot : size_t {
		StatusPanel = 0,
		CoinIcon,
		CoinTens,
		CoinOnes,
		CoinDivider,
		TotalTens,
		TotalOnes,
		HealthOne,
		HealthTwo,
		HealthThree,
		BossBack,
		BossFill,
		ClearBanner,
		RestartPrompt,
		SlotCount
	};

	struct Element {
		EntityRef entity;
		ComponentRef<TransformComponent> transform;
		ComponentRef<TextureComponent> texture;
		ComponentRef<MaterialComponent> material;
		Vector3 baseScale = Vector3(0.0f, 0.0f, 1.0f);
		float4 baseColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
	};

	static constexpr const char* PanelPrefab =
		"Asset/Game/Platformer/Prefab/PlatformerHudPanel.prefab";
	static constexpr const char* DigitPrefab =
		"Asset/Game/Platformer/Prefab/PlatformerHudDigit.prefab";
	static constexpr const char* ClearPrefab =
		"Asset/Game/Platformer/Prefab/PlatformerHudClear.prefab";
	static constexpr const char* PromptPrefab =
		"Asset/Game/Platformer/Prefab/PlatformerHudPrompt.prefab";

	void QueueUi() {
		if(uiQueued) return;
		EntityCommandBuffer* commands = GetCommandBuffer();
		if(!commands) return;
		uiQueued = true;

		QueueElement(*commands, StatusPanel, PanelPrefab, "HudStatusPanel",
			Vector2(0.0f, 0.0f), Vector2(0.0f, 0.0f),
			Vector3(0.018f, 0.020f, 0.0f), Vector3(0.270f, 0.090f, 1.0f),
			float4(0.015f, 0.025f, 0.055f, 0.78f), false);
		QueueElement(*commands, CoinIcon, PanelPrefab, "HudCoinIcon",
			Vector2(0.0f, 0.0f), Vector2(0.5f, 0.5f),
			Vector3(0.052f, 0.064f, 0.0f), Vector3(0.032f, 0.050f, 1.0f),
			float4(1.0f, 0.66f, 0.08f, 1.0f), false);

		QueueDigit(*commands, CoinTens, "HudCoinTens", 0.087f);
		QueueDigit(*commands, CoinOnes, "HudCoinOnes", 0.121f);
		QueueElement(*commands, CoinDivider, PanelPrefab, "HudCoinDivider",
			Vector2(0.0f, 0.0f), Vector2(0.5f, 0.5f),
			Vector3(0.157f, 0.064f, 0.0f), Vector3(0.007f, 0.058f, 1.0f),
			float4(0.78f, 0.84f, 0.96f, 0.92f), false, 0.32f);
		QueueDigit(*commands, TotalTens, "HudTotalTens", 0.191f);
		QueueDigit(*commands, TotalOnes, "HudTotalOnes", 0.225f);

		QueueHealth(*commands, HealthOne, "HudHealthOne", -0.118f);
		QueueHealth(*commands, HealthTwo, "HudHealthTwo", -0.078f);
		QueueHealth(*commands, HealthThree, "HudHealthThree", -0.038f);

		QueueElement(*commands, BossBack, PanelPrefab, "HudBossBack",
			Vector2(0.5f, 0.0f), Vector2(0.5f, 0.5f),
			Vector3(0.0f, 0.052f, 0.0f), Vector3(0.360f, 0.030f, 1.0f),
			float4(0.02f, 0.01f, 0.04f, 0.86f), true);
		QueueElement(*commands, BossFill, PanelPrefab, "HudBossFill",
			Vector2(0.5f, 0.0f), Vector2(0.0f, 0.5f),
			Vector3(-0.174f, 0.052f, 0.0f), Vector3(0.348f, 0.020f, 1.0f),
			float4(0.78f, 0.12f, 0.34f, 1.0f), true);

		QueueElement(*commands, ClearBanner, ClearPrefab, "HudClearBanner",
			Vector2(0.5f, 0.5f), Vector2(0.5f, 0.5f),
			Vector3(0.0f, -0.075f, 0.0f), Vector3(0.46f, 0.18f, 1.0f),
			float4(1.0f, 1.0f, 1.0f, 1.0f), true);
		QueueElement(*commands, RestartPrompt, PromptPrefab, "HudRestartPrompt",
			Vector2(0.5f, 0.5f), Vector2(0.5f, 0.5f),
			Vector3(0.0f, 0.105f, 0.0f), Vector3(0.28f, 0.050f, 1.0f),
			float4(1.0f, 1.0f, 1.0f, 1.0f), true);
	}

	void QueueDigit(EntityCommandBuffer& commands, Slot slot, const char* name, float x) {
		QueueElement(commands, slot, DigitPrefab, name,
			Vector2(0.0f, 0.0f), Vector2(0.5f, 0.5f),
			Vector3(x, 0.064f, 0.0f), Vector3(0.040f, 0.064f, 1.0f),
			float4(0.94f, 0.97f, 1.0f, 1.0f), false);
	}

	void QueueHealth(EntityCommandBuffer& commands, Slot slot, const char* name, float x) {
		QueueElement(commands, slot, PanelPrefab, name,
			Vector2(1.0f, 0.0f), Vector2(0.5f, 0.5f),
			Vector3(x, 0.050f, 0.0f), Vector3(0.030f, 0.046f, 1.0f),
			float4(1.0f, 0.16f, 0.20f, 1.0f), false, 0.785398163f);
	}

	void QueueElement(
		EntityCommandBuffer& commands,
		Slot slot,
		const char* prefab,
		std::string name,
		Vector2 anchor,
		Vector2 pivot,
		Vector3 position,
		Vector3 scale,
		float4 color,
		bool initiallyHidden,
		float rotationZ = 0.0f
	) {
		commands.InstantiatePrefab(prefab,
			[this, slot, name = std::move(name), anchor, pivot, position, scale, color, initiallyHidden, rotationZ](EntityRef root) {
				Element& element = elements[slot];
				element.entity = root;
				element.transform = ComponentRef<TransformComponent>(root);
				element.texture = ComponentRef<TextureComponent>(root);
				element.material = ComponentRef<MaterialComponent>(root);
				element.baseScale = scale;
				element.baseColor = color;

				if(auto* n = ComponentRef<NameComponent>(root).TryGet()) n->name = name;
				if(auto* t = element.transform.TryGet()) {
					t->position = position;
					t->scale = initiallyHidden ? Vector3(0.0f, 0.0f, scale.z) : scale;
					if(std::abs(rotationZ) > 0.0001f) t->SetRotationZ(rotationZ);
				}
				if(auto* sprite = ComponentRef<SpriteRendererComponent>(root).TryGet()) {
					sprite->anchor = anchor;
					sprite->pivot = pivot;
				}
				if(auto* material = element.material.TryGet()) {
					material->Material.BaseColor = color;
					if(initiallyHidden) material->Material.BaseColor.w = 0.0f;
				}
				uiReady = true;
			});
	}

	void UpdateUi() {
		auto* game = manager.TryGet();
		auto* controller = player.TryGet();
		if(!uiReady || !game || !controller) return;

		const int coins = std::clamp(game->GetCollectedCoins(), 0, 99);
		const int total = std::clamp(game->GetCoinTotal(), 0, 99);
		SetDigit(CoinTens, coins / 10);
		SetDigit(CoinOnes, coins % 10);
		SetDigit(TotalTens, total / 10);
		SetDigit(TotalOnes, total % 10);

		const int health = controller->GetHealth();
		SetHealth(HealthOne, health >= 1);
		SetHealth(HealthTwo, health >= 2);
		SetHealth(HealthThree, health >= 3);

		const bool showBoss = game->GetState() == PlatformerGameManager::RunState::BossIntro ||
			game->GetState() == PlatformerGameManager::RunState::BossBattle ||
			game->GetState() == PlatformerGameManager::RunState::BossDefeated;
		SetVisible(BossBack, showBoss);
		SetBar(BossFill, showBoss,
			game->GetBossMaxHealth() > 0
				? static_cast<float>(game->GetBossHealth()) / static_cast<float>(game->GetBossMaxHealth())
				: 0.0f);

		const bool cleared = game->IsCleared();
		SetVisible(ClearBanner, cleared);
		SetVisible(RestartPrompt, cleared && game->CanRestart());
	}

	void SetDigit(Slot slot, int value) {
		Element& element = elements[slot];
		if(auto* texture = element.texture.TryGet()) texture->AnimationNum = std::clamp(value, 0, 9);
		SetVisible(slot, true);
	}

	void SetHealth(Slot slot, bool active) {
		Element& element = elements[slot];
		if(auto* material = element.material.TryGet()) {
			material->Material.BaseColor = active
				? float4(1.0f, 0.14f, 0.18f, 1.0f)
				: float4(0.18f, 0.20f, 0.27f, 0.72f);
		}
		SetVisible(slot, true);
	}

	void SetVisible(Slot slot, bool visible) {
		Element& element = elements[slot];
		if(auto* transform = element.transform.TryGet()) {
			transform->scale = visible ? element.baseScale : Vector3(0.0f, 0.0f, element.baseScale.z);
		}
		if(auto* material = element.material.TryGet()) {
			material->Material.BaseColor.w = visible ? element.baseColor.w : 0.0f;
		}
	}

	void SetBar(Slot slot, bool visible, float fraction) {
		Element& element = elements[slot];
		const float clamped = std::clamp(fraction, 0.0f, 1.0f);
		if(auto* transform = element.transform.TryGet()) {
			transform->scale = visible
				? Vector3(element.baseScale.x * clamped, element.baseScale.y, element.baseScale.z)
				: Vector3(0.0f, 0.0f, element.baseScale.z);
		}
		if(auto* material = element.material.TryGet()) {
			material->Material.BaseColor.w = visible ? element.baseColor.w : 0.0f;
		}
	}

	ComponentRef<PlatformerGameManager> manager;
	ComponentRef<PlatformerCharacterController> player;
	std::array<Element, SlotCount> elements{};
	float elapsed = 0.0f;
	bool uiQueued = false;
	bool uiReady = false;
};
