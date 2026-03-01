#pragma once
#include "Component/entityNameComponent.h"
#include "Component/transformComponent.h"
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

#include "Script/SetScene.h"
#include "Script/ScoreManager.h"
#include "Script/ScoreSprite.h"
#include "Script/PlayerController.h"
#include "Script/GameTimeManager.h"
#include "Script/TimerSprite.h"
#include "Script/BallController.h"
#include "Script/EnemyController.h"
#include "Script/FadeInSprite.h"
#include "Script/FadeOutSprite.h"
#include "Script/FadeSetScene.h"
#include "Script/CameraController.h"
#include "Script/GN31.h"

enum ComponentStorageType {
	COMPONENT_SPARSE,
	COMPONENT_ARCHETYPE
};

// コンポーネントを登録（Archetype or Sparse を選択）
// ボトルネックが見つかったコンポーネントから ArchetypeStorage<T> に移行

#define COMPONENT_LIST(X) \
    X(NameComponent,COMPONENT_SPARSE)\
    X(TransformComponent,COMPONENT_SPARSE)\
    X(CustomScriptComponent,COMPONENT_SPARSE)\
    X(ColliderComponent,COMPONENT_SPARSE)\
    X(AudioComponent,COMPONENT_SPARSE)\
    X(RenderLayerComponent,COMPONENT_SPARSE)\
    X(OrderInLayerComponent,COMPONENT_SPARSE)\
    X(MaterialComponent,COMPONENT_SPARSE)\
    X(TextureComponent,COMPONENT_SPARSE)\
    X(BumpMapComponent,COMPONENT_SPARSE)\
    X(LightComponent,COMPONENT_SPARSE)\
    X(MeshRendererComponent,COMPONENT_SPARSE)\
    X(ModelRendererComponent,COMPONENT_SPARSE)\
    X(BillBoardRendererComponent,COMPONENT_SPARSE)\
    X(SpriteRendererComponent,COMPONENT_SPARSE)\
    X(TerrainComponent,COMPONENT_SPARSE)\
    X(WaveComponent,COMPONENT_SPARSE)\
    X(OutlineComponent,COMPONENT_SPARSE)\
    X(ParticleComponent,COMPONENT_SPARSE)\
    X(EffectComponent,COMPONENT_SPARSE)\
    X(CameraComponent,COMPONENT_SPARSE)\
    X(SetScene,COMPONENT_SPARSE)\
    X(ScoreManager,COMPONENT_SPARSE)\
    X(ScoreSprite,COMPONENT_SPARSE)\
    X(PlayerController,COMPONENT_SPARSE)\
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
    X(PrefabComponent,COMPONENT_SPARSE)
