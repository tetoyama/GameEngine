#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerSceneAccess.h"

#include <algorithm>
#include <cstdint>
#include <string>

class PlatformerGameManager : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerGameManager)
		REFLECT_FIELD(int, authoredCoinTotal, 0)
		REFLECT_FIELD(int, initialBossHealth, 3)
		REFLECT_FIELD(float, clearInputLockSeconds, 1.2f)
		REFLECT_FIELD(std::string, restartScenePath, std::string("Asset/Game/Platformer/Scene/PlatformerTechDemo.scene"))

public:
	enum class RunState : int {
		Playing = 0,
		BossIntro,
		BossBattle,
		BossDefeated,
		Cleared
	};

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
		ImGui::Text("Platformer Game Manager");
		INSPECTOR_FIELDS();
		ImGui::Separator();
		ImGui::Text("State: %d", static_cast<int>(state));
		ImGui::Text("Coins: %d / %d", collectedCoins, GetCoinTotal());
		ImGui::Text("Boss: %d / %d", bossHealth, bossMaxHealth);
		ImGui::Text("Run Time: %.2f", runTime);
	}

	void OnStart() override {
		player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		state = RunState::Playing;
		collectedCoins = 0;
		registeredCoins = 0;
		bossMaxHealth = (std::max)(1, initialBossHealth);
		bossHealth = bossMaxHealth;
		runTime = 0.0f;
		clearTimer = 0.0f;
		coinRevision = 0;
		bossRevision = 0;
		stateRevision = 0;
		reloadRequested = false;
	}

	void OnUpdate(float dt) override {
		if(state != RunState::Cleared) runTime += (std::max)(0.0f, dt);
		if(clearTimer > 0.0f) clearTimer = (std::max)(0.0f, clearTimer - dt);

		if(state == RunState::Cleared && clearTimer <= 0.0f && !reloadRequested && GetKeyDown('R')) {
			reloadRequested = true;
			if(!restartScenePath.empty()) LoadScene(restartScenePath);
		}
	}

	void RegisterCoin() {
		++registeredCoins;
	}

	bool CollectCoin() {
		if(state == RunState::Cleared) return false;
		++collectedCoins;
		++coinRevision;
		return true;
	}

	void BeginBossIntro() {
		if(state != RunState::Playing) return;
		state = RunState::BossIntro;
		++stateRevision;
	}

	void BeginBossBattle(int health) {
		bossMaxHealth = (std::max)(1, health);
		bossHealth = bossMaxHealth;
		state = RunState::BossBattle;
		++bossRevision;
		++stateRevision;
	}

	void SetBossHealth(int health) {
		const int clamped = std::clamp(health, 0, bossMaxHealth);
		if(clamped == bossHealth) return;
		bossHealth = clamped;
		++bossRevision;
	}

	void NotifyBossDefeated() {
		bossHealth = 0;
		state = RunState::BossDefeated;
		++bossRevision;
		++stateRevision;
	}

	void RequestClear() {
		if(state == RunState::Cleared) return;
		state = RunState::Cleared;
		clearTimer = (std::max)(0.0f, clearInputLockSeconds);
		if(!player.IsValid()) player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		if(auto* controller = player.TryGet()) controller->BeginClear();
		++stateRevision;
	}

	RunState GetState() const { return state; }
	bool IsCleared() const { return state == RunState::Cleared; }
	bool IsBossBattle() const { return state == RunState::BossBattle; }
	bool CanRestart() const { return state == RunState::Cleared && clearTimer <= 0.0f; }
	int GetCollectedCoins() const { return collectedCoins; }
	int GetCoinTotal() const { return authoredCoinTotal > 0 ? authoredCoinTotal : registeredCoins; }
	int GetBossHealth() const { return bossHealth; }
	int GetBossMaxHealth() const { return bossMaxHealth; }
	float GetRunTime() const { return runTime; }
	float GetClearTimer() const { return clearTimer; }
	uint32_t GetCoinRevision() const { return coinRevision; }
	uint32_t GetBossRevision() const { return bossRevision; }
	uint32_t GetStateRevision() const { return stateRevision; }

private:
	ComponentRef<PlatformerCharacterController> player;
	RunState state = RunState::Playing;
	int registeredCoins = 0;
	int collectedCoins = 0;
	int bossHealth = 3;
	int bossMaxHealth = 3;
	float runTime = 0.0f;
	float clearTimer = 0.0f;
	bool reloadRequested = false;
	uint32_t coinRevision = 0;
	uint32_t bossRevision = 0;
	uint32_t stateRevision = 0;
};
