# Step 18-A Static Batch Candidate Foundation

## 目的

Static Entityを直ちに別描画経路へ移さず、既存Render Packet Merge後の安定したPacket列からStatic Batch候補だけを抽出する。

この段階ではDraw Call、Render Pass、Renderable Executeを変更しない。候補Storageが失敗またはOverflowしても、通常Render Packet提出を必ず維持する。

## 実装済み

### Candidate Key

```cpp
struct StaticBatchCandidateKey {
    RenderPacketKind kind;
    RenderLayer layer;
    uint32_t materialKey;
};
```

現在確定している永続性のあるPacket分類だけを使用する。

Pointer、Native API Object、Component PointerはKeyへ含めない。

将来追加するKey:

- Pipeline Handle
- Texture Set Handle
- Mesh / Model Resource Handle
- Vertex Layout Handle
- Shadow / GBuffer / Forward Variant

これらはRHI世代付きHandleまたはAsset IDが完成してから追加する。

### Candidate Data

```cpp
struct StaticBatchCandidate {
    StaticBatchCandidateKey key;
    uint32_t sceneContextID;
    Entity entity;
    size_t packetIndex;
    uint64_t stableSequence;
};
```

- Source Entityは世代付きEntityで保持する
- `packetIndex`は同一FrameのPublished Packet列だけを参照する
- Component Pointerを保持しない
- Stable Sort後のPacket列から構築する

### 抽出条件

候補へ追加する条件:

- Published Render Packetである
- `StaticEntityComponent`が存在する
- Packet KindがModelまたはMeshである

Hidden、Disabled、破棄済みEntityはRender Packet Publish段階で既に除外される。

### 所有位置

`RenderPacketFrameBuffer`が`StaticBatchCandidateStorage`を所有する。

```text
Worker-local Packet Build
    ↓
RenderPacket Merge
    ↓
Published Packet Stable Sort
    ↓
Static Batch Candidate Build
    ↓
通常Render Packet Submit
```

候補Storageは描画提出を置き換えない。

## Scene Storage接続

`SceneStorageConfig::staticBatchReserve`を、Sceneごとに一度だけ合算して候補Storageへ適用する。

明示ReserveはRuntime Growth Eventへ計上しない。

公開Telemetry:

- Current
- Peak
- Capacity
- Growth Event Count
- Overflowed

Scene Storage Runtime Telemetry画面では、Render PacketとStatic Batch Candidateを別Storageとして表示する。

## Growth禁止時の契約

`allowRuntimeGrowth=false`で候補容量が不足した場合、部分的な候補配列を公開しない。

```text
Candidate Capacity不足
    ↓
候補配列をClear
    ↓
overflowed = true
    ↓
Candidate Build停止
    ↓
通常Render Packet Submitを継続
```

Static Batchは最適化であり、候補生成失敗を描画欠落へ変換しない。

Growth再許可後は既存Capacityを再利用し、不足時だけ通常のVector Growthを許可する。

## 検証

`StaticBatchCandidateStorageSmokeTest.cpp`で次を確認する。

- Static Model / Meshだけを抽出
- Dynamic Modelを除外
- Kind / Layer / Material / Stable Sequenceによる決定的Sort
- Scene設定Reserveを初期Growthへ計上しない
- Reserve超過時のGrowth Event
- Growth禁止時に候補を全破棄
- Growth禁止時もPublished Render Packetを維持
- Peak / Capacity / Reset契約

専用Workflow:

- `.github/workflows/static-batch-candidate.yml`

## 未完了

- Pipeline / Texture / Meshの永続Resource Key
- Static Packet Cache
- Dirty Revision / Invalidation
- Instance Data Build
- GPU Instance Buffer Upload
- `DrawIndexedInstanced`
- Picking / Object ID
- Shadow / GBuffer提出
- Spatial Cell単位Batch AABB / Culling
- Static Batch専用CPU / GPU計測

## 次工程

1. Render Packet Bindingから永続Resource Handleを抽出する
2. `StaticBatchCandidateKey`へPipeline / Texture / Mesh Keyを追加する
3. Scene単位Static Packet Cacheを構築する
4. Transform / Material / Resource変更RevisionからDirty化する
5. Static Instance BuildをAnyWorker Taskへ分離する
6. Instance Buffer UploadとSubmitをMainThread Taskへ分離する
