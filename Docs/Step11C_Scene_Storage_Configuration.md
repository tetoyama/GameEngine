# Step 11-C Scene Storage Configuration

## 目的

Sceneごとに主要Storageの初期確保量を設定し、Scene Load直後とRuntime中の再確保Spikeを減らす。

初期確保量は最適化HintでありHard Limitではない。EntityはComponentなし、Transformなしの状態を許容する。

## 設定

```cpp
struct SceneStorageConfig {
    uint32_t expectedEntityCount = 1024;
    uint32_t expectedTransformCount = 768;
    uint32_t expectedRenderableCount = 512;
    uint32_t expectedCullingCount = 512;
    uint32_t expectedStaticEntityCount = 256;
    uint32_t preallocatedTransformPages = 0;
    uint32_t preallocatedTagPages = 0;
    uint32_t renderPacketReserve = 512;
    uint32_t visibleEntityReserve = 512;
    uint32_t staticBatchReserve = 64;
    bool allowRuntimeGrowth = true;
    bool logCapacityGrowth = true;
};
```

DirectPaged Page SizeはEngine固定値とし、Sceneごとには初期Page数だけを指定する。

## 適用済みStorage

- Entity Slot / Generation / Free List / Alive Set
- Dense Component Storage
- Sparse Stable Component Storage
- Transform DirectPaged Data Storage
- Disabled / Static / Hidden DirectPaged Tag Storage
- Culling Component Storage
- Render Packet Frame Buffer
- View単位Visibility Set
- Static Batch Candidate Storage

未接続:

- Static Packet Cache
- Static Instance Data
- GPU Instance Buffer

## Telemetry

共通項目:

- Current
- Peak
- Capacity
- Growth Event Count

Scene Settings表示中のRuntime Telemetry Windowへ次を統合した。

- Entity Growth
- Component Storage Growth
- Visibility Growth
- Render Packet Growth
- Static Batch Candidate Growth
- 全Runtime Storage Growth合計
- Render Packet Current / Peak / Capacity
- Static Batch Candidate Current / Peak / Capacity / Overflow
- 全計測値の一括Reset

Render Packet BufferとStatic Batch Candidate Storageは全Scene共通、それ以外は選択Scene単位で表示する。

## Runtime Growth Policy

### Entity Registry

`allowRuntimeGrowth=false`では容量を変更前に判定する。

拒否時にEntity index、Free List、Alive Count、Peak、Growth Event、Slot配列を変更しない。

### Component Storage

Dense / Sparse / DirectPaged Data / DirectPaged Tagは、実データ容量の拡張が必要なAddを変更前に拒否する。

`ComponentRegistry::AddComponent`はStorage追加成功後にだけEntity Maskを更新する。YAML生成とRuntime型追加も失敗を伝播する。

### Visibility Set

容量不足時は部分集合を公開しない。

```text
Capacity不足
    -> Viewをoverflowedへ変更
    -> 部分Visibility Setを破棄
    -> HasView=false
    -> ShouldRender=trueの安全Fallback
```

### Static Batch Candidate Storage

Static Batchは最適化であり、候補生成失敗を描画結果へ影響させない。

```text
Candidate Capacity不足
    -> 部分候補を全破棄
    -> overflowed=true
    -> 通常Render Packet提出を継続
```

`staticBatchReserve`はSceneごとに合算して候補Storageへ適用する。明示ReserveはGrowth Eventへ含めない。

### Render Packet Frame Buffer

Current / Peak / Capacity / Growth Event計測は実装済み。

ただしPacket追加拒否は描画欠落につながるため、`allowRuntimeGrowth=false`の強制適用は未完了。

安全な代替提出経路として、Worker Buffer直接提出、Overflow Packet Legacy Submit、正確なPreflightと別Storageのいずれかが必要になる。

## YAML

```yaml
SceneSettings:
  Storage:
    ExpectedEntities: 4096
    ExpectedTransforms: 3072
    ExpectedRenderables: 2048
    ExpectedCullingComponents: 2048
    ExpectedStaticEntities: 1024
    PreallocatedTransformPages: 12
    PreallocatedTagPages: 4
    RenderPacketReserve: 2048
    VisibleEntityReserve: 2048
    StaticBatchReserve: 128
    AllowRuntimeGrowth: true
    LogCapacityGrowth: true
```

設定Nodeが存在しない旧SceneはDefault値で読み込む。

## 完了済み

- [x] Entity / Dense / Sparse Reserve
- [x] DirectPaged Page Table Reserve / Preallocation
- [x] Culling Component Reserve
- [x] Render Packet Reserve
- [x] Visible Entity Reserve
- [x] Static Batch Candidate Reserve
- [x] Componentなし / TransformなしEntity回帰
- [x] Dense / Sparse / DirectPaged Telemetry
- [x] Render Packet Telemetry
- [x] Static Batch Candidate Telemetry
- [x] 全Storage Growth統合表示
- [x] Entity Growth拒否Rollback
- [x] Component追加拒否時のMask非更新
- [x] Visibility容量不足時の全描画Fallback
- [x] Static Batch候補容量不足時の通常Packet Fallback
- [x] 専用Smoke Test / Workflow

## 未完了

- [ ] Render Packet Growth禁止時の安全な提出Fallback
- [ ] Static Packet Cache / Instance Data
- [ ] Scene Save / Load実機回帰
- [ ] Debug / Release x64実機回帰

## 完了条件

- Scene Load前にStorage初期確保量を適用できる
- 初期確保量がHard Limitにならない
- Current / Peak / Capacity / GrowthをEditorで確認できる
- Growth禁止時に部分更新、誤Culling、描画欠落を発生させない
- Render Packetを含む全Storageで安全なOverflow処理を持つ
- 旧Sceneを設定Nodeなしで読み込める
