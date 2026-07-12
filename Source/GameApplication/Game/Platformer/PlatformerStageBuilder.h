#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/ColliderComponent.h"
#include "Engine/Scene/Component/CameraComponent.h"
#include "Engine/Scene/Component/materialComponent.h"
#include "Engine/Scene/Component/textureComponent.h"
#include "Engine/Scene/Component/entityNameComponent.h"
#include "Game/Platformer/PlatformerCameraController.h"
#include "Game/Platformer/PlatformerCameraZone.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerEnemy.h"
#include "Game/Platformer/PlatformerMovingPlatform.h"
#include "Game/Platformer/PlatformerSceneAccess.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>

class PlatformerStageBuilder : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerStageBuilder)
		REFLECT_FIELD(bool, buildOnStart, false)
		REFLECT_FIELD(int, stageRevision, 3)

public:
	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		(void)context;
		DECODE_FIELDS(node);
		stageRevision = (std::max)(3, stageRevision);
		return true;
	}

	void inspector(SceneContext* context) override {
		(void)context;
		ImGui::Text("Platformer Stage");
		ImGui::TextWrapped(
			"Revision 3 rebuilds the demo as a beginner-friendly 3D course: "
			"wide landings, readable XYZ turns, four checkpoints, four regular enemies, "
			"an orchard lift, ruins, and a dedicated boss arena.");
		ImGui::Text("Stage Revision: %d", stageRevision);
	}

	void OnStart() override {
		if(!buildOnStart) return;
		Rebuild(m_ref.GetScene(), GetCommandBuffer());
		buildOnStart = false;
	}

	static void Rebuild(SceneContext* context, EntityCommandBuffer* commands) {
		if(!context || !context->component || !commands) return;
		PurgeBakedStage(context, *commands);
		ConfigureCore(context);
		BuildCourse(*commands);
	}

private:
	struct MaterialStyle {
		float4 baseColor;
		float metallic;
		float roughness;
		float3 emissiveColor;
		float emissiveIntensity;
	};

	static constexpr const char* BlockPrefab =
		"Asset/Game/Platformer/Prefab/PlatformerBlock.prefab";
	static constexpr const char* CoinPrefab =
		"Asset/Game/Platformer/Prefab/PlatformerCoin.prefab";
	static constexpr const char* EnemyPrefab =
		"Asset/Game/Platformer/Prefab/PlatformerEnemy.prefab";
	static constexpr const char* CheckpointPrefab =
		"Asset/Game/Platformer/Prefab/PlatformerCheckpoint.prefab";
	static constexpr const char* MovingPlatformPrefab =
		"Asset/Game/Platformer/Prefab/PlatformerMovingPlatform.prefab";
	static constexpr const char* CameraZonePrefab =
		"Asset/Game/Platformer/Prefab/PlatformerCameraZone.prefab";
	static constexpr const char* BossPrefab =
		"Asset/Game/Platformer/Prefab/PlatformerBoss.prefab";
	static constexpr const char* TreePrefab =
		"Asset/Game/Platformer/Prefab/PlatformerTreeDecoration.prefab";
	static constexpr const char* BearDecorationPrefab =
		"Asset/Game/Platformer/Prefab/PlatformerBearDecoration.prefab";
	static constexpr const char* GlowOrbPrefab =
		"Asset/Game/Platformer/Prefab/PlatformerGlowOrb.prefab";

	#include "Game/Platformer/PlatformerStageBuilderCommon.inl"
	#include "Game/Platformer/PlatformerStageBuilderCourse.inl"
};
