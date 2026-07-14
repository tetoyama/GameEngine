// =======================================================================
// 
// componentList.h
// 
// =======================================================================
#pragma once
#include "Component/entityNameComponent.h"
#include "Component/transformComponent.h"
#include "Component/EntityStateComponents.h"
#include "Component/CullingComponent.h"
#include "Component/CameraComponent.h"
#include "Component/modelRendererComponent.h"
#include "Component/meshRendererComponent.h"
#include "Component/BillBoardRendererComponent.h"
#include "Component/terrainComponent.h"
#include "Component/textureComponent.h"
#include "Component/CustomScriptComponent.h"
#include "Component/bumpMapComponent.h"
#include "Component/2DspriteRendererComponent.h"
#include "Component/RenderLayerComponent.h"
#include "Component/LightComponent.h"
#include "Component/particleComponent.h"
#include "Component/audioComponent.h"
#include "Component/outlineComponent.h"
#include "Component/waveComponent.h"
#include "Component/EffectComponent.h"
#include "Component/ColliderComponent.h"
#include "Component/materialComponent.h"
#include "Component/scriptComponent.h"
#include "Component/PrefabComponent.h"
#include "Component/followComponent.h"
#include "Component/environmentMapComponent.h"

#include "Config/SceneStorageConfig.h"
#include "Storage/ComponentStorageStrategy.h"

#include "Script/SetScene.h"
#include "Script/ScoreManager.h"
#include "Script/ScoreSprite.h"
#include "Script/PlayerController.h"
#include "Script/CharacterController.h"
#include "Script/GameTimeManager.h"
#include "Script/TimerSprite.h"
#include "Script/BallController.h"
#include "Script/EnemyController.h"
#include "Script/FadeInSprite.h"
#include "Script/FadeOutSprite.h"
#include "Script/FadeSetScene.h"
#include "Script/CameraController.h"
#include "Script/GN31.h"

// Windows SDKのmin/maxマクロはMiniGameCollection内部のstd::min/std::maxを
// 破壊する。Engine全体でNOMINMAXを強制せず、このinclude境界だけ一時退避する。
#pragma push_macro("min")
#pragma push_macro("max")
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include "Game/MiniGameCollection/Runtime/MiniGameCollectionEntryRuntime.h"
#include "Game/MiniGameCollection/Runtime/MiniGamePersistentRuntime.h"
#include "Game/MiniGameCollection/Runtime/PresentationSpikeRuntime.h"
#include "Game/MiniGameCollection/Runtime/ColorTerritoryRuntime.h"
#include "Game/MiniGameCollection/Runtime/SheepRoundupRuntime.h"
#include "Game/MiniGameCollection/Runtime/BackshotRuntime.h"
#include "Game/MiniGameCollection/Runtime/BackshotSlideRuntime.h"
#include "Game/MiniGameCollection/Runtime/BackshotRouteRuntime.h"
#include "Game/MiniGameCollection/Runtime/MiniGameBriefingOverlayRuntime.h"
#include "Game/MiniGameCollection/Runtime/MiniGameEventTelegraphRuntime.h"
#pragma pop_macro("max")
#pragma pop_macro("min")

#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerAnimationController.h"
#include "Game/Platformer/PlatformerGameManager.h"
#include "Game/Platformer/PlatformerCoin.h"
#include "Game/Platformer/PlatformerCheckpoint.h"
#include "Game/Platformer/PlatformerEnemy.h"
#include "Game/Platformer/PlatformerMovingPlatform.h"
#include "Game/Platformer/PlatformerCameraController.h"
#include "Game/Platformer/PlatformerCameraZone.h"
#include "Game/Platformer/PlatformerBoss.h"
#include "Game/Platformer/PlatformerHud.h"
#include "Game/Platformer/PlatformerStageBuilder.h"
#include "Game/Platformer/PlatformerPlayerFeedback.h"
#include "Game/Platformer/PlatformerClearFeedback.h"

inline constexpr auto COMPONENT_SPARSE = ECSStorage::ComponentStorageStrategy::SparseStable;
inline constexpr auto COMPONENT_DENSE = ECSStorage::ComponentStorageStrategy::Dense;
inline constexpr auto COMPONENT_DIRECT_PAGED = ECSStorage::ComponentStorageStrategy::DirectPaged;
inline constexpr auto COMPONENT_ARCHETYPE = ECSStorage::ComponentStorageStrategy::Archetype;

namespace ECSStorage {
template<>
struct ComponentStoragePreference<TransformComponent> {
	static constexpr bool HasExplicitStrategy = true;
	static constexpr ComponentStorageStrategy Strategy = ComponentStorageStrategy::DirectPaged;
	static constexpr size_t ExpectedCount = SceneStorageConfig::DefaultExpectedTransformCount;
	static constexpr size_t PreallocatedPages = SceneStorageConfig::RequiredPageCount(ExpectedCount);
};
} // namespace ECSStorage

#define COMPONENT_LIST(X) \
    X(NameComponent,COMPONENT_ARCHETYPE)\
    X(TransformComponent,COMPONENT_DIRECT_PAGED)\
    X(DisabledComponent,COMPONENT_DIRECT_PAGED)\
    X(StaticEntityComponent,COMPONENT_DIRECT_PAGED)\
    X(HiddenComponent,COMPONENT_DIRECT_PAGED)\
    X(CullingComponent,COMPONENT_DENSE)\
    X(CustomScriptComponent,COMPONENT_SPARSE)\
    X(ColliderComponent,COMPONENT_SPARSE)\
    X(AudioComponent,COMPONENT_SPARSE)\
    X(RenderLayerComponent,COMPONENT_ARCHETYPE)\
    X(OrderInLayerComponent,COMPONENT_ARCHETYPE)\
    X(MaterialComponent,COMPONENT_ARCHETYPE)\
    X(TextureComponent,COMPONENT_SPARSE)\
    X(BumpMapComponent,COMPONENT_SPARSE)\
    X(LightComponent,COMPONENT_ARCHETYPE)\
    X(MeshRendererComponent,COMPONENT_SPARSE)\
    X(ModelRendererComponent,COMPONENT_SPARSE)\
    X(BillBoardRendererComponent,COMPONENT_SPARSE)\
    X(SpriteRendererComponent,COMPONENT_SPARSE)\
    X(TerrainComponent,COMPONENT_SPARSE)\
    X(WaveComponent,COMPONENT_SPARSE)\
    X(OutlineComponent,COMPONENT_SPARSE)\
    X(ParticleComponent,COMPONENT_ARCHETYPE)\
    X(EffectComponent,COMPONENT_SPARSE)\
    X(CameraComponent,COMPONENT_ARCHETYPE)\
    X(SetScene,COMPONENT_SPARSE)\
    X(ScoreManager,COMPONENT_SPARSE)\
    X(ScoreSprite,COMPONENT_SPARSE)\
    X(PlayerController,COMPONENT_SPARSE)\
    X(CharacterController,COMPONENT_SPARSE)\
    X(GameTimeManager,COMPONENT_SPARSE)\
    X(TimerSprite,COMPONENT_SPARSE)\
    X(CameraController,COMPONENT_SPARSE)\
    X(BallController,COMPONENT_SPARSE)\
    X(EnemyController,COMPONENT_SPARSE)\
    X(FadeInSprite,COMPONENT_SPARSE)\
    X(FadeOutSprite,COMPONENT_SPARSE)\
    X(FadeSetScene,COMPONENT_SPARSE)\
    X(ScriptComponent,COMPONENT_SPARSE)\
    X(GN31,COMPONENT_SPARSE)\
    X(PlatformerCharacterController,COMPONENT_SPARSE)\
    X(PlatformerAnimationController,COMPONENT_SPARSE)\
    X(PlatformerGameManager,COMPONENT_SPARSE)\
    X(PlatformerCoin,COMPONENT_SPARSE)\
    X(PlatformerCheckpoint,COMPONENT_SPARSE)\
    X(PlatformerEnemy,COMPONENT_SPARSE)\
    X(PlatformerMovingPlatform,COMPONENT_SPARSE)\
    X(PlatformerCameraController,COMPONENT_SPARSE)\
    X(PlatformerCameraZone,COMPONENT_SPARSE)\
    X(PlatformerBoss,COMPONENT_SPARSE)\
    X(PlatformerHud,COMPONENT_SPARSE)\
    X(PlatformerStageBuilder,COMPONENT_SPARSE)\
    X(PlatformerPlayerFeedback,COMPONENT_SPARSE)\
    X(PlatformerClearFeedback,COMPONENT_SPARSE)\
    X(MiniGameCollection::Runtime::MiniGameCollectionEntryRuntime,COMPONENT_SPARSE)\
    X(MiniGameCollection::Runtime::MiniGamePersistentRuntime,COMPONENT_SPARSE)\
    X(MiniGameCollection::Runtime::PresentationSpikeRuntime,COMPONENT_SPARSE)\
    X(MiniGameCollection::Runtime::ColorTerritoryRuntime,COMPONENT_SPARSE)\
    X(MiniGameCollection::Runtime::SheepRoundupRuntime,COMPONENT_SPARSE)\
    X(MiniGameCollection::Runtime::BackshotRuntime,COMPONENT_SPARSE)\
    X(MiniGameCollection::Runtime::BackshotSlideRuntime,COMPONENT_SPARSE)\
    X(MiniGameCollection::Runtime::BackshotRouteRuntime,COMPONENT_SPARSE)\
    X(MiniGameCollection::Runtime::ColorTerritoryBriefingRuntime,COMPONENT_SPARSE)\
    X(MiniGameCollection::Runtime::SheepRoundupBriefingRuntime,COMPONENT_SPARSE)\
    X(MiniGameCollection::Runtime::BackshotBriefingRuntime,COMPONENT_SPARSE)\
    X(MiniGameCollection::Runtime::ColorTerritoryEventTelegraphRuntime,COMPONENT_SPARSE)\
    X(MiniGameCollection::Runtime::SheepRoundupEventTelegraphRuntime,COMPONENT_SPARSE)\
    X(PrefabComponent,COMPONENT_SPARSE)\
    X(FollowComponent,COMPONENT_ARCHETYPE)\
    X(EnvironmentMapComponent,COMPONENT_ARCHETYPE)
