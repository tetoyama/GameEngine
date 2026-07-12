#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/ColliderComponent.h"
#include "Engine/Scene/Component/entityNameComponent.h"
#include "Game/Platformer/PlatformerCameraController.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerSceneAccess.h"
#include "Game/Platformer/PlatformerSoundLibrary.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

class PlatformerGameManager : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerGameManager)
		REFLECT_FIELD(int, authoredCoinTotal, 0)
		REFLECT_FIELD(int, initialBossHealth, 3)
		REFLECT_FIELD(float, clearInputLockSeconds, 1.2f)
		REFLECT_FIELD(float, bossActivationZ, 139.0f)
		REFLECT_FIELD(float, bossActivationDistance, 13.25f)
		REFLECT_FIELD(float, arenaLockZ, 136.75f)
		REFLECT_FIELD(Vector3, arenaRespawnPosition, Vector3(0.0f, 0.08f, 139.5f))
		REFLECT_FIELD(float, arenaMinX, -11.25f)
		REFLECT_FIELD(float, arenaMaxX, 11.25f)
		REFLECT_FIELD(float, arenaMaxZ, 161.25f)
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
		ValidateArenaSettings();
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
		ImGui::Text("Arena Locked: %s", arenaLocked ? "true" : "false");
		ImGui::Text("Hit Stop: %.3f", hitStopTimer);
	}

	void OnStart() override {
		ValidateArenaSettings();
		PlatformerSoundLibrary::EnsureGenerated();
		player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		ConfigureBossForNormalJumpAndArenaEntry();
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
		arenaLocked = false;
		gateImpactCooldown = 0.0f;
		hitStopTimer = 0.0f;
		requestedHitStop = 0.0f;
		hitStopActive = false;
		frozenActors.clear();
		if(auto* controller = player.TryGet()) {
			observedStompRevision = controller->GetStompEventRevision();
			observedDamageRevision = controller->GetDamageEventRevision();
			observedRespawnRevision = controller->GetRespawnEventRevision();
		}
	}

	void OnUpdate(float dt) override {
		const float safeDt = (std::max)(0.0f, dt);
		TickHitStop(safeDt);
		ObservePlayerFeedbackEvents();

		if(state != RunState::Cleared && !hitStopActive) runTime += safeDt;
		if(clearTimer > 0.0f) clearTimer = (std::max)(0.0f, clearTimer - safeDt);
		gateImpactCooldown = (std::max)(0.0f, gateImpactCooldown - safeDt);

		const bool restartPressed = GetKeyDown('R') || GetKeyDown(VK_RETURN);
		if(state == RunState::Cleared && clearTimer <= 0.0f && !reloadRequested && restartPressed) {
			reloadRequested = true;
			if(!restartScenePath.empty()) LoadScene(restartScenePath);
		}
	}

	void OnFixedUpdate(float dt) override {
		if(dt <= 0.0f || !arenaLocked) return;
		EnforceArenaLock(false);
	}

	void OnStop() override {
		EndHitStop();
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
		arenaLocked = true;
		if(auto* controller = player.TryGet()) {
			controller->SetCheckpoint(arenaRespawnPosition);
		}
		EnforceArenaLock(false);
		RequestHitStop(0.055f);
		ImpactCamera(0.62f, 0.30f, -0.014f, Vector3(0.0f, 0.0f, 1.0f));
		++stateRevision;
	}

	void BeginBossBattle(int health) {
		bossMaxHealth = (std::max)(1, health);
		bossHealth = bossMaxHealth;
		state = RunState::BossBattle;
		arenaLocked = true;
		if(auto* controller = player.TryGet()) {
			controller->SetCheckpoint(arenaRespawnPosition);
		}
		++bossRevision;
		++stateRevision;
	}

	void SetBossHealth(int health) {
		const int clamped = std::clamp(health, 0, bossMaxHealth);
		if(clamped == bossHealth) return;
		bossHealth = clamped;
		RequestHitStop(clamped <= 0 ? 0.155f : 0.105f);
		ImpactCamera(
			clamped <= 0 ? 1.45f : 1.05f,
			clamped <= 0 ? 0.48f : 0.34f,
			clamped <= 0 ? 0.040f : 0.028f,
			Vector3(0.0f, -1.0f, 0.0f));
		++bossRevision;
	}

	void NotifyBossDefeated() {
		bossHealth = 0;
		state = RunState::BossDefeated;
		arenaLocked = true;
		RequestHitStop(0.18f);
		ImpactCamera(1.50f, 0.50f, 0.040f, Vector3(0.0f, 1.0f, 0.0f));
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

	void RequestHitStop(float seconds) {
		seconds = std::clamp(seconds, 0.0f, 0.22f);
		if(seconds <= 0.0f) return;
		if(hitStopActive) {
			hitStopTimer = (std::max)(hitStopTimer, seconds);
		} else {
			requestedHitStop = (std::max)(requestedHitStop, seconds);
		}
	}

	RunState GetState() const { return state; }
	bool IsCleared() const { return state == RunState::Cleared; }
	bool IsBossBattle() const { return state == RunState::BossBattle; }
	bool IsArenaLocked() const { return arenaLocked; }
	bool IsHitStopActive() const { return hitStopActive; }
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
	struct FrozenActorState {
		physx::PxRigidDynamic* actor = nullptr;
		physx::PxVec3 linearVelocity = physx::PxVec3(0.0f);
		physx::PxVec3 angularVelocity = physx::PxVec3(0.0f);
	};

	void ValidateArenaSettings() {
		bossActivationDistance = (std::max)(10.0f, bossActivationDistance);
		arenaLockZ = (std::min)(arenaLockZ, bossActivationZ - 0.5f);
		arenaMaxZ = (std::max)(arenaMaxZ, bossActivationZ + 8.0f);
		if(arenaMinX > arenaMaxX) std::swap(arenaMinX, arenaMaxX);
		arenaRespawnPosition.x = std::clamp(arenaRespawnPosition.x, arenaMinX + 0.5f, arenaMaxX - 0.5f);
		arenaRespawnPosition.z = std::clamp(arenaRespawnPosition.z, arenaLockZ + 0.5f, arenaMaxZ - 1.0f);
	}

	void ResolveRuntimeReferences() {
		if(!player.IsValid()) {
			player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		}
		if(!camera.IsValid()) {
			camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		}
	}

	void ObservePlayerFeedbackEvents() {
		ResolveRuntimeReferences();
		auto* controller = player.TryGet();
		if(!controller) return;

		if(controller->GetStompEventRevision() != observedStompRevision) {
			observedStompRevision = controller->GetStompEventRevision();
			RequestHitStop(IsBossBattle() ? 0.105f : 0.060f);
			ImpactCamera(
				IsBossBattle() ? 0.88f : 0.48f,
				IsBossBattle() ? 0.30f : 0.18f,
				IsBossBattle() ? 0.024f : 0.010f,
				Vector3(0.0f, -1.0f, 0.0f));
		}

		if(controller->GetDamageEventRevision() != observedDamageRevision) {
			observedDamageRevision = controller->GetDamageEventRevision();
			RequestHitStop(IsBossBattle() ? 0.075f : 0.050f);
			ImpactCamera(
				IsBossBattle() ? 0.82f : 0.54f,
				0.24f,
				-0.015f,
				Vector3(0.0f, 0.25f, -1.0f));
		}

		if(controller->GetRespawnEventRevision() != observedRespawnRevision) {
			observedRespawnRevision = controller->GetRespawnEventRevision();
			if(arenaLocked) {
				controller->SetCheckpoint(arenaRespawnPosition);
				EnforceArenaLock(true);
				ImpactCamera(0.32f, 0.22f, 0.008f, Vector3(0.0f, 1.0f, 0.0f));
			}
		}
	}

	void TickHitStop(float dt) {
		if(!hitStopActive && requestedHitStop > 0.0f) {
			BeginHitStop(requestedHitStop);
			requestedHitStop = 0.0f;
			return;
		}
		if(!hitStopActive) return;

		hitStopTimer = (std::max)(0.0f, hitStopTimer - dt);
		if(requestedHitStop > 0.0f) {
			hitStopTimer = (std::max)(hitStopTimer, requestedHitStop);
			requestedHitStop = 0.0f;
		}
		if(hitStopTimer <= 0.0f) EndHitStop();
	}

	void BeginHitStop(float duration) {
		if(hitStopActive) {
			hitStopTimer = (std::max)(hitStopTimer, duration);
			return;
		}
		SceneContext* context = m_ref.GetScene();
		if(!context || !context->component) return;

		frozenActors.clear();
		const auto colliderEntities = context->component->FindEntitiesWithComponent<ColliderComponent>();
		for(Entity entity : colliderEntities) {
			auto* component = context->component->GetComponent<ColliderComponent>(entity);
			auto* rigid = component ? component->pRigidbodyDynamic : nullptr;
			if(!rigid || rigid->getActorFlags().isSet(physx::PxActorFlag::eDISABLE_SIMULATION)) continue;

			FrozenActorState frozen;
			frozen.actor = rigid;
			frozen.linearVelocity = rigid->getLinearVelocity();
			frozen.angularVelocity = rigid->getAngularVelocity();
			frozenActors.push_back(frozen);
			rigid->setLinearVelocity(physx::PxVec3(0.0f));
			rigid->setAngularVelocity(physx::PxVec3(0.0f));
			rigid->setActorFlag(physx::PxActorFlag::eDISABLE_SIMULATION, true);
		}
		hitStopTimer = duration;
		hitStopActive = true;
	}

	void EndHitStop() {
		if(!hitStopActive && frozenActors.empty()) return;
		for(const FrozenActorState& frozen : frozenActors) {
			if(!frozen.actor) continue;
			frozen.actor->setActorFlag(physx::PxActorFlag::eDISABLE_SIMULATION, false);
			frozen.actor->setLinearVelocity(frozen.linearVelocity);
			frozen.actor->setAngularVelocity(frozen.angularVelocity);
			frozen.actor->wakeUp();
		}
		frozenActors.clear();
		hitStopTimer = 0.0f;
		hitStopActive = false;
	}

	void ImpactCamera(float strength, float duration, float fovKick, const Vector3& direction) {
		ResolveRuntimeReferences();
		if(auto* controller = camera.TryGet()) {
			controller->AddImpulse(strength, duration, fovKick, direction);
		}
	}

	void EnforceArenaLock(bool forceRespawn) {
		ResolveRuntimeReferences();
		auto* controller = player.TryGet();
		ComponentRef<TransformComponent> playerTransform(player.GetEntityRef());
		ComponentRef<ColliderComponent> playerCollider(player.GetEntityRef());
		auto* pose = playerTransform.TryGet();
		if(!controller || !pose) return;

		Vector3 target = pose->position;
		bool corrected = false;
		if(forceRespawn) {
			target = arenaRespawnPosition;
			corrected = true;
		} else {
			const float clampedX = std::clamp(target.x, arenaMinX, arenaMaxX);
			const float clampedZ = std::clamp(target.z, arenaLockZ, arenaMaxZ);
			corrected = std::abs(clampedX - target.x) > 0.0001f ||
				std::abs(clampedZ - target.z) > 0.0001f;
			target.x = clampedX;
			target.z = clampedZ;
		}
		if(!corrected) return;

		pose->position = target;
		if(auto* component = playerCollider.TryGet()) {
			if(auto* rigid = component->pRigidbodyDynamic) {
				physx::PxTransform actorPose = rigid->getGlobalPose();
				actorPose.p = physx::PxVec3(target.x, target.y, target.z);
				rigid->setGlobalPose(actorPose, true);
				const physx::PxVec3 velocity = rigid->getLinearVelocity();
				const float safeZ = target.z <= arenaLockZ + 0.02f
					? (std::max)(0.0f, velocity.z)
					: velocity.z;
				rigid->setLinearVelocity(physx::PxVec3(velocity.x, forceRespawn ? 0.0f : velocity.y, safeZ));
				rigid->wakeUp();
			}
		}

		if(!forceRespawn && gateImpactCooldown <= 0.0f) {
			gateImpactCooldown = 0.18f;
			ImpactCamera(0.22f, 0.12f, -0.004f, Vector3(0.0f, 0.0f, 1.0f));
		}
	}

	void ConfigureBossForNormalJumpAndArenaEntry() {
		SceneContext* context = m_ref.GetScene();
		if(!context || !context->component) return;

		const auto namedEntities = context->component->FindEntitiesWithComponent<NameComponent>();
		for(Entity entity : namedEntities) {
			auto* name = context->component->GetComponent<NameComponent>(entity);
			if(!name || name->name != "PlatformerBoss") continue;

			auto* pose = context->component->GetComponent<TransformComponent>(entity);
			auto* col = context->component->GetComponent<ColliderComponent>(entity);
			if(!pose || !col || col->colliders.empty()) return;

			ColliderShape& shape = col->colliders.front();
			if(shape.type != ColliderType::Box) return;

			const float shapeHeight = (std::max)(0.001f, shape.size.y);
			const float oldScaleY = (std::max)(0.001f, std::abs(pose->scale.y));
			const float oldWorldHeight = shapeHeight * oldScaleY;
			const float targetWorldHeight = (std::min)(oldWorldHeight, 1.15f);
			const float oldCenterY = pose->position.y + shape.offset.y * oldScaleY;
			const float bottomY = oldCenterY - oldWorldHeight * 0.5f;

			if(targetWorldHeight < oldWorldHeight - 0.01f) {
				pose->scale.y = targetWorldHeight / shapeHeight;
				const float newCenterY = bottomY + targetWorldHeight * 0.5f;
				pose->position.y = newCenterY - shape.offset.y * pose->scale.y;
				col->needsUpdate = true;
			}

			// The old distance-only trigger reached the player while they were still on
			// the last descending step. Put the dormant boss deep enough into the arena
			// that its existing activation radius begins only after the floor is reached.
			pose->position.z = (std::max)(pose->position.z, bossActivationZ + bossActivationDistance);
			if(auto* rigid = col->pRigidbodyDynamic) {
				physx::PxTransform actorPose = rigid->getGlobalPose();
				actorPose.p = physx::PxVec3(pose->position.x, pose->position.y, pose->position.z);
				rigid->setGlobalPose(actorPose, true);
				rigid->setLinearVelocity(physx::PxVec3(0.0f));
				rigid->setAngularVelocity(physx::PxVec3(0.0f));
				rigid->wakeUp();
			}
			return;
		}
	}

	ComponentRef<PlatformerCharacterController> player;
	ComponentRef<PlatformerCameraController> camera;
	RunState state = RunState::Playing;
	std::vector<FrozenActorState> frozenActors;
	int registeredCoins = 0;
	int collectedCoins = 0;
	int bossHealth = 3;
	int bossMaxHealth = 3;
	float runTime = 0.0f;
	float clearTimer = 0.0f;
	float gateImpactCooldown = 0.0f;
	float hitStopTimer = 0.0f;
	float requestedHitStop = 0.0f;
	bool reloadRequested = false;
	bool arenaLocked = false;
	bool hitStopActive = false;
	uint32_t observedStompRevision = 0;
	uint32_t observedDamageRevision = 0;
	uint32_t observedRespawnRevision = 0;
	uint32_t coinRevision = 0;
	uint32_t bossRevision = 0;
	uint32_t stateRevision = 0;
};
