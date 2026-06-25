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

### Static Entity Tag

Static性はModelRenderer固有フラグではなく、ECS全体で共有できるTag Componentとして定義する。

```cpp
struct StaticEntityComponent : IComponent {
    bool enabled = true;
};
```

将来的にTag専用Storageを導入する場合も、公開契約はStatic Entity判定として維持する。

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
    MeshHandle mesh;       // Instance Batchで使用
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

## Culling

巨大なBatchを1つだけ作るとFrustum Culling粒度が失われるため、空間セル単位にBatchを分割する。

初期実装では次を推奨する。

- Scene単位
- Render Pass単位
- Material / Pipeline単位
- 固定サイズSpatial Cell単位

Batch全体のAABBを保持し、Cell単位でFrustum Cullingする。

Entity単位Occlusion Cullingが必要になった場合はInstance Batchへ切り替えるか、Batchを細分化する。

---

## Editor契約

Project SettingsまたはInspectorでStatic指定を切り替え可能にする。

表示項目:

- Static Entity
- Batch Type: Auto / Instancing / Geometry Merge / Disabled
- Current Batch ID
- Batch Dirty状態
- Instance Count / Vertex Count / Index Count
- Rebuild CPU Time / Upload CPU Time

Transform編集時は即座にBatchをDirty化する。編集中はDynamic経路へ一時退避し、編集確定後にStatic Batchへ戻す方式も許容する。

---

## 実装順

### Step 18-A: Static Entity契約

- [ ] `StaticEntityComponent`またはTag Storage
- [ ] YAML Serialize / Deserialize
- [ ] Inspector表示
- [ ] Transform / Parent / Renderer変更Revision
- [ ] Static指定変更時のDirty Event

### Step 18-B: Static State Batching

- [ ] Render Packetへ永続Resource Keyを追加
- [ ] Pipeline / Material / Texture / Mesh Keyの固定
- [ ] Static Packet Cache
- [ ] State変更回数計測
- [ ] Dynamic Packetとの同一Pass提出

### Step 18-C: Static Instancing

- [ ] Instance Data Layout
- [ ] Instance Buffer生成・更新
- [ ] `DrawIndexedInstanced`経路
- [ ] Object ID / Selection ID対応
- [ ] Shadow / GBuffer対応
- [ ] Batch AABBとFrustum Culling

### Step 18-D: Static Geometry Batching

- [ ] CPU Mesh結合
- [ ] World TransformのVertex Bake
- [ ] Index Offset再構築
- [ ] Batch専用RHI Buffer Upload
- [ ] 32bit Index / Buffer Size上限
- [ ] Material境界によるSub Batch
- [ ] Source EntityとTriangle Rangeの対応

### Step 18-E: Invalidation / Editor回帰

- [ ] Transform変更時Rebuild
- [ ] Material / Texture変更時Rebuild
- [ ] Entity追加・削除時Rebuild
- [ ] Scene Load / Unload時Lifecycle
- [ ] Undo / Redo対応
- [ ] Play / Stop復元対応
- [ ] Picking / Outline / Shadow回帰

### Step 18-F: 計測と完了判定

- [ ] Static Entity数
- [ ] Batch数
- [ ] Batch Rebuild CPU Time
- [ ] Upload CPU Time
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

Static State BatchingはRenderWorld初期段階で導入できる。
Static InstancingとStatic Geometry BatchingはRenderWorldとRuntime Resource分離後に実装する。

---

## 完了条件

- Static / Dynamic Entityの描画結果が一致する
- Static BatchがFrame-local Component Pointerを保持しない
- Static Entity変更時に古いBatchを描画しない
- Shadow / GBuffer / Forwardで同一Batch契約を使用する
- Picking / Object IDがSource Entityへ戻せる
- Scene Unload後にRHI Resourceが残存しない
- Draw Call削減がPerformance Monitorで確認できる
- StaticでないEntityの更新コストを増やさない
