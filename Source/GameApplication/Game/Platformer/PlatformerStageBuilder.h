#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/entityNameComponent.h"
#include "Engine/Scene/Component/materialComponent.h"
#include "Game/Platformer/PlatformerBoss.h"
#include "Game/Platformer/PlatformerCameraZone.h"
#include "Game/Platformer/PlatformerEnemy.h"
#include "Game/Platformer/PlatformerMovingPlatform.h"

#include <string>
#include <vector>

class PlatformerStageBuilder : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerStageBuilder)
		REFLECT_FIELD(bool, buildOnStart, true)
		REFLECT_FIELD(int, stageRevision, 1)

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
		ImGui::Text("Platformer Stage Builder");
		INSPECTOR_FIELDS();
		ImGui::Text("Queued: %s", built ? "true" : "false");
	}

	void OnStart() override {
		if(buildOnStart && !built) BuildStage();
	}

	void OnStop() override {
		built = false;
	}

private:
	static constexpr const char* BlockPrefab = "Asset/Game/Platformer/Prefab/PlatformerBlock.prefab";
	static constexpr const char* CoinPrefab = "Asset/Game/Platformer/Prefab/PlatformerCoin.prefab";
	static constexpr const char* EnemyPrefab = "Asset/Game/Platformer/Prefab/PlatformerEnemy.prefab";
	static constexpr const char* CheckpointPrefab = "Asset/Game/Platformer/Prefab/PlatformerCheckpoint.prefab";
	static constexpr const char* MovingPlatformPrefab = "Asset/Game/Platformer/Prefab/PlatformerMovingPlatform.prefab";
	static constexpr const char* CameraZonePrefab = "Asset/Game/Platformer/Prefab/PlatformerCameraZone.prefab";
	static constexpr const char* BossPrefab = "Asset/Game/Platformer/Prefab/PlatformerBoss.prefab";
	static constexpr const char* PlayerFeedbackPrefab = "Asset/Game/Platformer/Prefab/PlatformerPlayerFeedback.prefab";

	void BuildStage() {
		EntityCommandBuffer* commands = GetCommandBuffer();
		if(!commands) return;
		built = true;
		commands->InstantiatePrefab(PlayerFeedbackPrefab);

		const float4 courseColor(0.22f, 0.34f, 0.48f, 1.0f);
		const float4 tripleColor(0.24f, 0.44f, 0.72f, 1.0f);
		const float4 wallColor(0.28f, 0.24f, 0.58f, 1.0f);
		const float4 gimmickColor(0.16f, 0.52f, 0.44f, 1.0f);
		const float4 arenaColor(0.32f, 0.18f, 0.42f, 1.0f);

		// Section 1: broad introduction and readable low-risk steps.
		SpawnBlock(*commands, "IntroGround", Vector3(0.0f, -0.5f, 7.0f), Vector3(6.0f, 0.5f, 13.0f), courseColor);
		SpawnBlock(*commands, "IntroStepA", Vector3(0.0f, 0.15f, 18.0f), Vector3(3.2f, 0.65f, 2.0f), courseColor);
		SpawnBlock(*commands, "IntroStepB", Vector3(0.0f, 0.85f, 22.0f), Vector3(3.0f, 1.35f, 2.0f), courseColor);

		// Section 2: continuous lower recovery path plus elevated triple-jump rewards.
		SpawnBlock(*commands, "TripleLowerA", Vector3(0.0f, -0.5f, 29.0f), Vector3(5.0f, 0.5f, 7.0f), tripleColor);
		SpawnBlock(*commands, "TripleLowerB", Vector3(0.0f, -0.5f, 42.0f), Vector3(5.0f, 0.5f, 6.0f), tripleColor);
		SpawnBlock(*commands, "TripleLedgeA", Vector3(0.0f, 1.05f, 28.0f), Vector3(2.5f, 0.3f, 1.8f), tripleColor);
		SpawnBlock(*commands, "TripleLedgeB", Vector3(0.0f, 2.25f, 33.0f), Vector3(2.5f, 0.3f, 1.8f), tripleColor);
		SpawnBlock(*commands, "TripleLedgeC", Vector3(0.0f, 3.75f, 38.5f), Vector3(3.0f, 0.35f, 2.2f), tripleColor);
		SpawnCameraZone(*commands, Vector3(0.0f, 3.0f, 33.0f), Vector3(8.0f, 6.0f, 20.0f), 1, 0, true);

		// Section 3: safe wall contact, alternating shaft, then a short exposed exit.
		SpawnBlock(*commands, "WallEntry", Vector3(0.0f, -0.5f, 50.0f), Vector3(4.5f, 0.5f, 4.0f), wallColor);
		SpawnBlock(*commands, "WallLeft", Vector3(-3.0f, 4.2f, 59.0f), Vector3(0.45f, 4.7f, 8.0f), wallColor);
		SpawnBlock(*commands, "WallRight", Vector3(3.0f, 4.2f, 59.0f), Vector3(0.45f, 4.7f, 8.0f), wallColor);
		SpawnBlock(*commands, "WallSafeShelf", Vector3(1.7f, 2.0f, 53.5f), Vector3(1.3f, 0.3f, 1.8f), wallColor);
		SpawnBlock(*commands, "WallMidShelf", Vector3(-1.7f, 4.1f, 59.0f), Vector3(1.2f, 0.3f, 1.8f), wallColor);
		SpawnBlock(*commands, "WallExitShelf", Vector3(1.6f, 6.0f, 65.5f), Vector3(1.4f, 0.35f, 2.0f), wallColor);
		SpawnBlock(*commands, "WallRecovery", Vector3(0.0f, -0.5f, 62.0f), Vector3(3.6f, 0.5f, 9.0f), wallColor);
		SpawnBlock(*commands, "WallExitGround", Vector3(0.0f, -0.5f, 70.5f), Vector3(4.5f, 0.5f, 3.0f), wallColor);
		SpawnCameraZone(*commands, Vector3(0.0f, 4.0f, 59.0f), Vector3(8.0f, 8.0f, 21.0f), 2, 0, true);

		// Section 4: moving platform and first stomp enemy.
		SpawnBlock(*commands, "GimmickStart", Vector3(0.0f, -0.5f, 76.0f), Vector3(4.5f, 0.5f, 3.5f), gimmickColor);
		SpawnMovingPlatform(*commands, Vector3(0.0f, 0.25f, 82.5f), Vector3(2.2f, 0.35f, 2.2f), Vector3(0.0f, 3.0f, 0.0f), 3.8f, 0.0f);
		SpawnBlock(*commands, "GimmickLanding", Vector3(0.0f, -0.5f, 89.0f), Vector3(5.0f, 0.5f, 4.0f), gimmickColor);
		SpawnEnemy(*commands, Vector3(0.0f, 0.45f, 90.0f), Vector3(1.0f, 0.0f, 0.0f), 2.8f);

		// Section 5: boss checkpoint and open-entry charge arena.
		SpawnCheckpoint(*commands, Vector3(0.0f, 0.6f, 95.0f));
		SpawnBlock(*commands, "BossApproach", Vector3(0.0f, -0.5f, 96.0f), Vector3(5.5f, 0.5f, 4.0f), arenaColor);
		SpawnBlock(*commands, "BossArenaFloor", Vector3(0.0f, -0.5f, 109.0f), Vector3(20.0f, 0.5f, 26.0f), arenaColor);
		SpawnBlock(*commands, "BossWallLeft", Vector3(-10.0f, 2.5f, 109.0f), Vector3(1.0f, 6.0f, 26.0f), arenaColor);
		SpawnBlock(*commands, "BossWallRight", Vector3(10.0f, 2.5f, 109.0f), Vector3(1.0f, 6.0f, 26.0f), arenaColor);
		SpawnBlock(*commands, "BossWallFar", Vector3(0.0f, 2.5f, 122.0f), Vector3(21.0f, 6.0f, 1.0f), arenaColor);
		SpawnBlock(*commands, "BossJumpWallLeft", Vector3(-6.0f, 2.2f, 111.0f), Vector3(0.8f, 5.4f, 4.0f), wallColor);
		SpawnBlock(*commands, "BossJumpWallRight", Vector3(6.0f, 2.2f, 111.0f), Vector3(0.8f, 5.4f, 4.0f), wallColor);
		SpawnBoss(*commands, Vector3(0.0f, 1.0f, 113.0f));

		SpawnCoins(*commands);
	}

	void SpawnCoins(EntityCommandBuffer& commands) {
		const std::vector<Vector3> positions = {
			// Basic movement and normal jump guide.
			Vector3(0.0f, 1.1f, 2.0f), Vector3(0.0f, 1.1f, 5.0f), Vector3(0.0f, 1.1f, 8.0f),
			Vector3(0.0f, 1.4f, 12.0f), Vector3(0.0f, 2.0f, 17.5f), Vector3(0.0f, 2.8f, 21.5f),
			// Triple-jump arc and elevated reward line.
			Vector3(0.0f, 1.2f, 25.0f), Vector3(0.0f, 1.8f, 27.0f), Vector3(0.0f, 2.4f, 29.0f),
			Vector3(0.0f, 3.1f, 31.5f), Vector3(0.0f, 3.8f, 34.0f), Vector3(0.0f, 4.5f, 36.5f),
			Vector3(0.0f, 5.0f, 39.0f), Vector3(0.0f, 3.1f, 43.0f),
			// Wall-kick direction guide, alternating left/right.
			Vector3(1.9f, 1.6f, 52.0f), Vector3(-1.9f, 2.4f, 54.5f), Vector3(1.9f, 3.2f, 57.0f),
			Vector3(-1.9f, 4.0f, 59.5f), Vector3(1.9f, 4.8f, 62.0f), Vector3(-1.9f, 5.6f, 64.0f),
			Vector3(1.4f, 6.7f, 66.0f), Vector3(0.0f, 1.1f, 71.0f),
			// Moving platform and stomp landing guide.
			Vector3(0.0f, 1.4f, 78.0f), Vector3(0.0f, 2.2f, 81.0f), Vector3(0.0f, 3.2f, 83.5f),
			Vector3(0.0f, 2.0f, 86.5f), Vector3(0.0f, 1.5f, 89.5f),
			// Boss approach and optional arena side routes.
			Vector3(0.0f, 1.3f, 94.0f), Vector3(-5.2f, 2.2f, 106.0f), Vector3(5.2f, 2.2f, 116.0f)
		};

		for(size_t i = 0; i < positions.size(); ++i) {
			SpawnCoin(commands, positions[i], static_cast<int>(i));
		}
	}

	static void SpawnBlock(
		EntityCommandBuffer& commands,
		const std::string& name,
		const Vector3& position,
		const Vector3& scale,
		const float4& color
	) {
		commands.InstantiatePrefab(BlockPrefab, [name, position, scale, color](EntityRef root) {
			SetName(root, name);
			SetTransform(root, position, scale);
			SetMaterialColor(root, color);
		});
	}

	static void SpawnCoin(EntityCommandBuffer& commands, const Vector3& position, int index) {
		commands.InstantiatePrefab(CoinPrefab, [position, index](EntityRef root) {
			SetName(root, "Coin_" + std::to_string(index + 1));
			SetTransform(root, position, Vector3(0.34f, 0.52f, 0.12f));
		});
	}

	static void SpawnEnemy(
		EntityCommandBuffer& commands,
		const Vector3& position,
		const Vector3& patrolAxis,
		float patrolDistance
	) {
		commands.InstantiatePrefab(EnemyPrefab, [position, patrolAxis, patrolDistance](EntityRef root) {
			SetTransform(root, position, Vector3(0.9f, 0.9f, 0.9f));
			if(auto* enemy = ComponentRef<PlatformerEnemy>(root).TryGet()) {
				enemy->patrolAxis = patrolAxis;
				enemy->patrolDistance = patrolDistance;
			}
		});
	}

	static void SpawnCheckpoint(EntityCommandBuffer& commands, const Vector3& position) {
		commands.InstantiatePrefab(CheckpointPrefab, [position](EntityRef root) {
			SetTransform(root, position, Vector3(0.45f, 1.2f, 0.45f));
		});
	}

	static void SpawnMovingPlatform(
		EntityCommandBuffer& commands,
		const Vector3& position,
		const Vector3& scale,
		const Vector3& offset,
		float cycle,
		float phase
	) {
		commands.InstantiatePrefab(MovingPlatformPrefab, [position, scale, offset, cycle, phase](EntityRef root) {
			SetTransform(root, position, scale);
			if(auto* platform = ComponentRef<PlatformerMovingPlatform>(root).TryGet()) {
				platform->localOffset = offset;
				platform->cycleSeconds = cycle;
				platform->phaseOffset = phase;
			}
		});
	}

	static void SpawnCameraZone(
		EntityCommandBuffer& commands,
		const Vector3& position,
		const Vector3& scale,
		int profile,
		int exitProfile,
		bool restoreOnExit
	) {
		commands.InstantiatePrefab(CameraZonePrefab, [position, scale, profile, exitProfile, restoreOnExit](EntityRef root) {
			SetTransform(root, position, scale);
			if(auto* zone = ComponentRef<PlatformerCameraZone>(root).TryGet()) {
				zone->profile = profile;
				zone->exitProfile = exitProfile;
				zone->restoreOnExit = restoreOnExit;
			}
		});
	}

	static void SpawnBoss(EntityCommandBuffer& commands, const Vector3& position) {
		commands.InstantiatePrefab(BossPrefab, [position](EntityRef root) {
			SetTransform(root, position, Vector3(2.4f, 2.0f, 2.4f));
		});
	}

	static void SetTransform(const EntityRef& root, const Vector3& position, const Vector3& scale) {
		if(auto* transform = ComponentRef<TransformComponent>(root).TryGet()) {
			transform->position = position;
			transform->scale = scale;
		}
	}

	static void SetName(const EntityRef& root, const std::string& name) {
		if(auto* component = ComponentRef<NameComponent>(root).TryGet()) component->name = name;
	}

	static void SetMaterialColor(const EntityRef& root, const float4& color) {
		if(auto* material = ComponentRef<MaterialComponent>(root).TryGet()) material->Material.BaseColor = color;
	}

	bool built = false;
};
