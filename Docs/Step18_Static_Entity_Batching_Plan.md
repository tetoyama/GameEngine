# Step 18 Static Entity / Static Batching Plan

## 目的

移動・変形・描画資源変更を行わないEntityをStatic Entityとして明示し、RenderWorld抽出時に永続的なStatic Draw Dataへ変換する。

同一描画条件を持つStatic Entityをまとめ、CPU側のPacket Build負荷、D3D11 State変更回数、Draw Call数を削減する。

この工程は既存のRender Packetを置き換えず、Dynamic Packet経路とStatic Batch経路を同じRender Passへ提出できる構造とする。

---

## 現在設計との適合性

現在のRender Packetには次の基盤がある。

- RenderLayer / PacketKind / Material Keyによる決定的Sort
- World Matrix Snapshot
- Shadow / GBuffer / Forward / OverlayのPass Mask
- Worker-local BuildとMainThread Submitの分離
- Entity / Sceneを含む安定したTie Break

このため、Static Entityを抽出してBatch Key単位にグループ化する前段は既に成立している。

一方、現在のModel描画はEntityごとにWorld定数を更新し、MeshごとにVertex / Index BufferをBindして`DrawIndexed`している。SortによるState変更削減は可能だが、複数Entityを1 Draw Callへ統合する真のStatic Batchingには追加実装が必要である。

---

## 基本契約

### Entity状態Component / Tag

`Enable`、`Static`、`Culling`、`Visible`はRenderer固有設定ではなく、Entity全体へ適用される共通状態として扱う。

存在そのものが状態を表すComponentまたはTagを使用し、別途`bool enabled`を持たせない。

```cpp
struct DisabledComponent : IComponent {};
struct StaticEntityComponent : IComponent {};
struct CullingComponent : IComponent {
    AABB localBounds;
    AABB worldBounds;
    uint64_t sourceRevision = 0;
    uint64_t transformRevision = 0;
    bool boundsValid = false;
};
struct HiddenComponent : IComponent {};
```

公開上の意味:

- `DisabledComponent`が存在しない: Entity Enable
- `StaticEntityComponent`が存在する: Static
- `CullingComponent`が存在する: Culling対象
- `HiddenComponent`が存在しない: Visible

`VisibleComponent`ではなく`HiddenComponent`を採用する理由は、通常Entityを可視状態として扱い、非表示だけを明示するためである。

### CullingComponentの契約

`CullingComponent`は自動生成しない。

存在すること自体が、対象EntityへCullingを適用する明示的な宣言となる。

内部に保持するBoundsやRevisionはCullingSystemが生成・更新するDerived Dataであり、ユーザーが直接編集しない。

```cpp
struct CullingComponent : IComponent {
    AABB localBounds;
    AABB worldBounds;
    uint64_t sourceRevision = 0;
    uint64_t transformRevision = 0;
    bool boundsValid = false;
};
```

契約:

- ComponentのAttach / DetachはユーザーまたはEditor操作で決定する
- BoundsはInspector編集対象外
- BoundsはScene YAMLへ直接保存しない
- CullingSystemがRenderableとTransformから再構築する
- Componentが存在しないEntityはCulling判定を行わず常に描画候補とする
- 対応Boundsを生成できない場合は安全側へ倒し、描画対象として残す

### Static Entityの保証

Static EntityとしてBatchへ登録されている間、次を変更しない。

- TransformのWorld Matrix
- Parent階層とParent World Matrix
- Mesh / Model Resource
- Material / Texture / Shader Variant
- Render Layer / Pass Mask
- Shadow Cast設定
- Vertex Layout / Index Format

変更を検出した場合は対象BatchをDirty化し、次の安全なRebuild Pointで再構築する。

Static指定中の変更を禁止するのではなく、変更時にBatchから除外または再構築する契約とする。

---

## Editor共通Entityヘッダー

`Enable`、`Static`、`Culling`、`Visible`は通常Component一覧とは分離し、Hierarchy選択時のInspector上部へ共通チェックボックスとして表示する。

```text
[✓] Enable    [ ] Static    [✓] Culling    [✓] Visible
```

各チェックボックスは対応Component / TagのAttach / Detachへ変換する。

```text
Enable OFF   -> DisabledComponent Attach
Enable ON    -> DisabledComponent Detach

Static ON    -> StaticEntityComponent Attach
Static OFF   -> StaticEntityComponent Detach

Culling ON   -> CullingComponent Attach
Culling OFF  -> CullingComponent Detach

Visible OFF  -> HiddenComponent Attach
Visible ON   -> HiddenComponent Detach
```

構造変更は即時Registry変更ではなく、Editor CommandまたはEntityCommandBufferを経由する。

Undo / RedoではAttach / Detach操作をそのまま復元する。

複数Entity選択時は三状態表示を使用する。

```text
Checked       全Entityに存在する
Unchecked     全Entityに存在しない
Mixed         一部Entityだけ存在する
```

### EnableとVisibleの違い

`Enable`と`Visible`は同じ意味にしない。

- Enable OFF: Entity全体をSystem Queryと更新処理から除外する
- Visible OFF: Update / Physics / Scriptは維持し、描画提出だけを除外する

### StaticとCullingの独立性

StaticとCullingは独立して設定可能とする。

```text
Static OFF + Culling OFF
    動的Entity、常に描画候補

Static OFF + Culling ON
    動的Entity、Entity AABBでFrustum Culling

Static ON + Culling OFF
    Static Batch対象、Batch Cullingを行わず常に提出

Static ON + Culling ON
    Static Batch対象、Batch / Spatial Cell AABBでCulling
```

Static指定時にCullingを自動Attachしない。
Culling指定時にStaticを自動Attachしない。

---

## Culling処理

### Bounds生成

CullingSystemは`CullingComponent`を持つEntityだけを対象にする。

Renderable種別からLocal Boundsを構築し、TransformからWorld Boundsを更新する。

```text
ModelRenderer       ModelDataの頂点Min / Max
MeshRenderer        Mesh ResourceのLocal Bounds
SpriteRenderer      Size / Pivot
BillboardRenderer   Size / Pivot
Terrain             Terrain Mesh範囲
Wave                最大変位を含む範囲
Particle / Effect   設定値または保守的なBounds
```

Skinned Model、Particle、Waveなど動的に形状が変化するRenderableでは、Animation Boundsまたは保守的なPaddingを使用する。

### 可視結果

単一の`isVisible`をCullingComponentへ保存しない。
Editor CameraとPlayer Cameraでは可視結果が異なるため、Viewごとの一時結果をCullingSystemまたはRenderWorldへ保持する。

```cpp
struct CullingViewResult {
    CameraID camera;
    std::vector<Entity> visibleEntities;
};
```

RenderWorld移行後は`RenderView`のVisible Item配列へ変換する。

### 実行順

```text
TransformSystem.WorldMatrix.Update
    ↓
CullingSystem.Bounds.Update
    ↓
CullingSystem.Visibility.Build
    ↓
RenderSystem.Packet.Build
    ↓
RenderSystem.Command.Submit
```

`CullingComponent`が存在しないEntityとBounds生成に失敗したEntityは、Visibility Buildを通さず描画候補へ残す。

---

## Batch分類

### 1. Static State Batch

同じPipeline / Material / Texture / Meshを連続配置し、State変更を最小化する。

これは既存Render Packet Sortの拡張であり、Draw Call自体はEntity数だけ残る。

### 2. Static Instance Batch

同一Mesh、同一Material、同一Pipelineを持つEntityをInstance Bufferへまとめ、`DrawIndexedInstanced`で描画する。

適用条件:

- 同一Vertex / Index Buffer
- 同一Material / Texture Set
- 同一Shader Variant
- Entityごとの差分がWorld MatrixとObject IDなどInstance Dataに限定される

### 3. Static Geometry Batch

異なるMeshをCPU側で結合し、Batch専用Vertex / Index Bufferを生成して1回の`DrawIndexed`へまとめる。

適用条件:

- 同一Pipeline / Material / Texture Set
- Vertex Layoutが一致
- 32bit Index範囲内
- Lightmap / UV / Tangent等の意味が保持可能

D3D11ではMulti Draw Indirectを前提にしないため、Draw Call削減効果を最大化するにはInstance BatchまたはGeometry Batchを使用する。

---

## Batch Key

```cpp
struct StaticBatchKey {
    RenderPacketKind kind;
    RenderLayer layer;
    RenderPacketPassMask passMask;
    PipelineHandle pipeline;
    MaterialHandle material;
    TextureSetHandle textures;
    MeshHandle mesh;
    VertexLayoutHandle vertexLayout;
};
```

Geometry Batchでは`mesh`をKeyから除外し、結合可能性を別途検証する。

Pointer値を永続Keyとして使用しない。RHI世代付きHandleまたはResource Asset IDを使用する。

---

## RenderWorld構造

```cpp
struct StaticRenderInstance {
    Entity sourceEntity;
    RenderPacketMatrix4x4 world;
    uint32_t objectID;
};

struct StaticRenderBatch {
    StaticBatchKey key;
    RHI::BufferHandle vertexBuffer;
    RHI::BufferHandle indexBuffer;
    RHI::BufferHandle instanceBuffer;
    AABB worldBounds;
    uint32_t indexCount;
    uint32_t instanceCount;
    bool dirty;
};

struct RenderWorld {
    std::vector<StaticRenderBatch> staticBatches;
    RenderPacketBuffer dynamicPackets;
};
```

Static BatchはFrame-localなComponent Pointerを保持しない。Entityとの対応が必要な場合は世代付きEntityとBatch Instance IndexのMappingをRenderWorld側へ保持する。

---

## 更新フロー

```text
ECS World変更
    ↓
StaticBatchDirtyEvent / Revision更新
    ↓
RenderWorld.StaticBatch.Build      AnyWorker
    ↓
RenderWorld.StaticBatch.Upload     MainThread
    ↓
RenderPass.StaticBatch.Submit      MainThread
```

通常FrameではDirtyでないStatic Batchを再Buildしない。

Dynamic Entityは従来どおり毎Frame Render Packetを生成する。

---

## Static Batch Culling

巨大なBatchを1つだけ作るとFrustum Culling粒度が失われるため、Culling対象のStatic Entityは空間セル単位にBatchを分割する。

初期実装では次を推奨する。

- Scene単位
- Render Pass単位
- Material / Pipeline単位
- 固定サイズSpatial Cell単位

Batch全体のAABBを保持し、Cell単位でFrustum Cullingする。

`StaticEntityComponent`を持つが`CullingComponent`を持たないEntityは、常時提出されるStatic Batchへ分類する。

Entity単位Occlusion Cullingが必要になった場合はInstance Batchへ切り替えるか、Batchを細分化する。

---

## 実装順

### Step 18-A: 共通Entity状態契約

- [ ] `DisabledComponent`
- [ ] `StaticEntityComponent`
- [ ] `CullingComponent`
- [ ] `HiddenComponent`
- [ ] EntityヘッダーのEnable / Static / Culling / Visibleチェックボックス
- [ ] Attach / Detach Editor Command
- [ ] 複数選択時のMixed表示
- [ ] Undo / Redo
- [ ] Scene YAML Serialize / Deserialize

### Step 18-B: Culling基盤

- [ ] 共通AABB型
- [ ] Model / Mesh ResourceのLocal Bounds
- [ ] Renderable別Bounds Provider
- [ ] Transform RevisionによるWorld Bounds更新
- [ ] Camera Frustum生成
- [ ] View単位Visible Entity構築
- [ ] Packet Build前のFrustum Culling
- [ ] Bounds生成失敗時の安全側Fallback
- [ ] Editor View / Player View別判定

### Step 18-C: Static State Batching

- [ ] Render Packetへ永続Resource Keyを追加
- [ ] Pipeline / Material / Texture / Mesh Keyの固定
- [ ] Static Packet Cache
- [ ] State変更回数計測
- [ ] Dynamic Packetとの同一Pass提出
- [ ] Culling有効 / 無効Static Batchの分離

### Step 18-D: Static Instancing

- [ ] Instance Data Layout
- [ ] Instance Buffer生成・更新
- [ ] `DrawIndexedInstanced`経路
- [ ] Object ID / Selection ID対応
- [ ] Shadow / GBuffer対応
- [ ] Batch AABBとFrustum Culling

### Step 18-E: Static Geometry Batching

- [ ] CPU Mesh結合
- [ ] World TransformのVertex Bake
- [ ] Index Offset再構築
- [ ] Batch専用RHI Buffer Upload
- [ ] 32bit Index / Buffer Size上限
- [ ] Material境界によるSub Batch
- [ ] Source EntityとTriangle Rangeの対応

### Step 18-F: Invalidation / Editor回帰

- [ ] Transform変更時Rebuild
- [ ] Material / Texture変更時Rebuild
- [ ] Entity追加・削除時Rebuild
- [ ] Static / Culling Attach・Detach時Rebuild
- [ ] Enable / Visible状態反映
- [ ] Scene Load / Unload時Lifecycle
- [ ] Undo / Redo対応
- [ ] Play / Stop復元対応
- [ ] Picking / Outline / Shadow回帰

### Step 18-G: 計測と完了判定

- [ ] Culling対象Entity数
- [ ] Frustum Culled Entity数
- [ ] Static Entity数
- [ ] Static Batch数
- [ ] Culled Static Batch数
- [ ] Batch Rebuild CPU Time
- [ ] Upload CPU Time
- [ ] Before / After Packet数
- [ ] Before / After Draw Call数
- [ ] Before / After Render Schedule CPU
- [ ] Before / After GPU Pass Time
- [ ] Memory増加量

---

## 優先順位

この工程は次の前提完了後に開始する。

1. Render Packet直接参照化の完了
2. ModelRendererComponentからD3D11 Runtime Resourceを分離
3. RenderWorldの基本抽出
4. RHI Resource HandleによるMesh / Material識別
5. GPU Pass単位Timestamp計測

共通EntityヘッダーとTag Component契約はRenderWorldより前に実装できる。
CullingSystemのEntity単位Frustum Cullingも既存Render Packetへ先行導入できる。
Static State BatchingはRenderWorld初期段階で導入する。
Static InstancingとStatic Geometry BatchingはRenderWorldとRuntime Resource分離後に実装する。

---

## 完了条件

- Enable / Static / Culling / Visibleが共通Entityヘッダーから操作できる
- チェック状態とComponent / Tagの存在が常に一致する
- CullingComponentが存在しないEntityへCullingを適用しない
- CullingComponent内部Boundsをユーザー操作項目として公開しない
- Editor CameraとPlayer Cameraで独立した可視結果を使用する
- Static / Dynamic Entityの描画結果が一致する
- Static BatchがFrame-local Component Pointerを保持しない
- Static Entity変更時に古いBatchを描画しない
- Shadow / GBuffer / Forwardで同一Batch契約を使用する
- Picking / Object IDがSource Entityへ戻せる
- Scene Unload後にRHI Resourceが残存しない
- Packet数とDraw Call削減がPerformance Monitorで確認できる
- StaticでもCulling対象でもないEntityの更新コストを増やさない
