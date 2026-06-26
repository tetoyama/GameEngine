# Culling View Integration Completion

## 目的

`CullingComponent`を明示的にAttachしたEntityだけを対象に、Editor / Player ViewごとのFrustum Cullingを実描画経路へ接続する。

単一の可視FlagをComponentへ保存せず、同じCamera Entityを異なるView用途・解像度で利用しても結果を混同しない。

## 確定した実行順

```text
Transform / Follow / Script / Physics Update
    ↓
CullingSystem.Bounds.Update
    - Static Model Local Bounds供給
    - World Matrix Revision検証
    - World Bounds更新
    ↓
RenderSystem.Packet.Build
    - 全View共通の描画候補Packetを構築
    - Hidden / Disabled / stale Entityを除外
    ↓
PlayerPass / EditorPass
    - ViewProjectionからFrustumを生成
    - View単位Visibility Setを構築
    ↓
GBufferPass / ForwardPass
    - View VisibilityでPacketを絞り込み
    ↓
RenderSystem.Command.Submit
```

## 共通Packetを維持する理由

Editor CameraとPlayer Cameraでは可視結果が異なる。

共通Packet Build前に一方のCamera結果だけでEntityを除外すると、別Viewでは可視なEntityまで失われる。そのため全View共通Packetを一度構築し、GBuffer / Forward提出前にView単位で絞り込む。

現在削減できる対象:

- GBuffer / ForwardのSort対象数
- Renderable Execute呼び出し
- Draw Call
- Vertex / Pixel処理

将来RenderWorldへ移行した後は、View非依存Broad PhaseとView別Packet Listを分離し、Packet Build負荷も削減する。

## View Key

```cpp
enum class CullingViewKind : uint8_t {
    Custom,
    Editor,
    Player,
    Shadow
};

struct CullingViewKey {
    uint32_t sceneContextID;
    Entity camera;
    CullingViewKind kind;
    uint32_t instanceID;
};
```

KeyへScene、Camera Entity、View種別、Instance IDを含める。Instance IDはShadow Cascadeなど、同一Camera内の複数Viewへ使用する。

## Bounds供給

### Static Model

`ModelCullingBoundsProvider`がAssimp頂点列からLocal AABBを生成する。

- 通常Model: `(x, y, z)`
- Blender軸変換Model: `(x, -z, y)`
- Model Resource / aiScene / Path / Blender FlagからSource Revisionを生成
- Source Revisionが変化した場合だけLocal Boundsを再生成

### Skinned Model

Rest Pose Boundsによる誤Cullingを避けるため、現段階ではLocal Boundsを供給しない。`boundsValid=false`を維持し、安全側で常時描画候補へ残す。

### 未接続Resource

Mesh、Terrain、Wave、Particle、Effect、Sprite、Billboardは安全側で未確定のまま残す。

## World Bounds更新

`CullingBoundsRuntime`はWorld Matrixの16要素からRevision Hashを生成する。

Source RevisionまたはTransform Revisionが変化した時だけ、Local AABBの8頂点をWorld変換してWorld AABBを再計算する。

Local Bounds未供給、Transformなし、無効Boundsの場合は`boundsValid=false`を維持する。

## Scheduler契約

Task名:

```text
CullingSystem.Bounds.Update
```

設定:

- Domain: Render
- Phase: Early
- Priority: -100
- Thread Affinity: AnyWorker

Access:

- Read `TransformComponent`
- Read `ModelRendererComponent`
- Write `CullingComponent`
- Read `SceneManager`

## Scene Storage接続

`SceneStorageConfig::visibleEntityReserve`をView単位の可視Entity集合へ適用する。これはHard Limitではなく、View構築時の再確保を抑えるHintとして扱う。

`renderPacketReserve`は共通`RenderPacketFrameBuffer`へScene単位で一度だけ適用済み。

## 描画Pass接続

接続済み:

- Player View
- Editor View
- GBuffer Pass
- Forward Pass

意図的に未接続:

- Shadow Map Pass

ShadowはCamera FrustumではなくLight View / Cascade Frustumで判定する必要がある。Camera結果を流用すると画面外のShadow Casterを誤って除外するため、専用Light View Culling実装までは従来経路を維持する。

## 安全側Fallback

次の場合は描画候補へ残す。

- `CullingComponent`なし
- Bounds未生成
- View Visibility未構築
- Camera Entityが無効
- Resource Local Bounds未供給
- Skinned Model

Hidden / Disabled / stale Entityは既存Render Packet契約により除外する。

## 検証

専用Smoke Test:

- Culling AABB / Frustum
- View単位Visibility Set
- Editor / Player / Shadow Key分離
- ViewProjection Frustum抽出
- World Bounds Revision
- Scene Bounds Updater
- Static Model Local Bounds
- Culling System Task Access
- Render Packet View Culling
- Render Packet Visibility / Reserve

実機確認待ち:

- Editor / Player Viewの視錐台外描画除外
- Camera移動時のVisibility更新
- Static Model Resource差替え時のBounds再生成
- Scene Save / Load / TempSave / TempLoad
- Shadow Caster回帰
