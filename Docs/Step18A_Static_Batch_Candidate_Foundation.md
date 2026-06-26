# Step 18-A Static Batch Candidate Foundation

## 目的

Static Entityを直ちに別描画経路へ移さず、既存Render Packet Merge後の安定したPacket列からStatic Batch候補と連続Groupを構築する。

この段階ではDraw Call、Render Pass、Renderable Executeを変更しない。候補Storageが失敗またはOverflowしても、通常Render Packet提出を必ず維持する。

## Candidate Key

```cpp
struct StaticBatchCandidateKey {
    RenderPacketKind kind;
    RenderLayer layer;
    uint32_t materialKey;
};
```

現在確定しているPacket分類だけを使用する。

Pointer、Native API Object、Component PointerはKeyへ含めない。

将来追加するKey:

- Pipeline Handle
- Texture Set Handle
- Mesh / Model Resource Handle
- Vertex Layout Handle
- Shadow / GBuffer / Forward Variant

これらはRHI世代付きHandleまたはAsset IDが完成してから追加する。

## Candidate Data

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

## Candidate Group

Sort後の連続候補をSceneとKey単位でGroup化する。

```cpp
struct StaticBatchCandidateGroup {
    StaticBatchCandidateKey key;
    uint32_t sceneContextID;
    size_t firstCandidate;
    size_t candidateCount;
};
```

Groupは候補配列のRangeだけを保持し、EntityやPacketを複製しない。

Sort順:

1. Packet Kind
2. Render Layer
3. Material Key
4. Scene Context ID
5. Stable Sequence

同じKeyでもSceneを跨いでGroup化しない。

Group StorageはCandidate Storageと同じ`staticBatchReserve`で事前確保する。Group数はCandidate数以下であるため、同じReserveを安全な上限Hintとして利用できる。

## 抽出条件

候補へ追加する条件:

- Published Render Packetである
- `StaticEntityComponent`が存在する
- Packet KindがModelまたはMeshである

Hidden、Disabled、破棄済みEntityはRender Packet Publish段階で既に除外される。

## 所有位置

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
Candidate Stable Sort
    ↓
Scene / Key Group Build
    ↓
通常Render Packet Submit
```

Candidate / Group Storageは描画提出を置き換えない。

## Scene Storage接続

`SceneStorageConfig::staticBatchReserve`をSceneごとに一度だけ合算し、CandidateとGroupの両Storageへ適用する。

明示ReserveはRuntime Growth Eventへ計上しない。

公開Telemetry:

- Candidate Current / Peak / Capacity
- Group Current / Peak / Capacity
- Growth Event Count
- Overflowed

Scene Storage Runtime Telemetry画面ではRender Packet、Static Batch Candidate、Static Batch Groupを分けて表示する。

## Growth禁止時の契約

`allowRuntimeGrowth=false`でCandidate容量が不足した場合、部分的なCandidate / Groupを公開しない。

```text
Candidate Capacity不足
    ↓
Candidate / GroupをClear
    ↓
overflowed = true
    ↓
Candidate Build停止
    ↓
通常Render Packet Submitを継続
```

Static Batchは最適化であり、候補生成失敗を描画結果へ影響させない。

Growth再許可後は既存Capacityを再利用し、不足時だけ通常のVector Growthを許可する。

## 検証

`StaticBatchCandidateStorageSmokeTest.cpp`で次を確認する。

- Static Model / Meshだけを抽出
- Dynamic Modelを除外
- Kind / Layer / Material / Stable Sequenceによる決定的Sort
- 同一Scene / Keyを一つのGroupへ集約
- Group Rangeの`firstCandidate` / `candidateCount`
- Scene設定Reserveを初期Growthへ計上しない
- Candidate / Group Current / Peak / Capacity
- Reserve超過時のGrowth Event
- Growth禁止時にCandidate / Groupを全破棄
- Growth禁止時もPublished Render Packetを維持
- Peak / Growth Reset契約

専用Workflow:

- `.github/workflows/static-batch-candidate.yml`

## 完了済み

- [x] Static Model / Mesh Candidate抽出
- [x] 決定的Candidate Sort
- [x] Scene / Key単位Group生成
- [x] Scene設定Reserve接続
- [x] Candidate / Group Telemetry
- [x] Growth禁止時の通常Packet Fallback
- [x] Runtime Telemetry UI接続
- [x] Smoke Test / Workflow

## 未完了

- [ ] Pipeline / Texture / Meshの永続Resource Key
- [ ] Static Packet Cache
- [ ] Dirty Revision / Invalidation
- [ ] Instance Data Build
- [ ] GPU Instance Buffer Upload
- [ ] `DrawIndexedInstanced`
- [ ] Picking / Object ID
- [ ] Shadow / GBuffer提出
- [ ] Spatial Cell単位Batch AABB / Culling
- [ ] Static Batch専用CPU / GPU計測

## 次工程

1. Render Packet Bindingから永続Resource Handleを抽出する
2. Candidate KeyへPipeline / Texture / Mesh Keyを追加する
3. Scene単位Static Packet Cacheを構築する
4. Transform / Material / Resource変更RevisionからDirty化する
5. Static Instance BuildをAnyWorker Taskへ分離する
6. Instance Buffer UploadとSubmitをMainThread Taskへ分離する
