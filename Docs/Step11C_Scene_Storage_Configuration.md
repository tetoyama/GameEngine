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
- Static Packet Cache
- Static Instance Data Buffer

未接続:

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
- Render Packet Safety Growth Override
- Static Batch Candidate Growth
- Static Packet Cache Growth
- Static Instance Data Growth
- 全Runtime Storage Growth合計
- Render Packet Current / Peak / Capacity
- Static Batch Candidate / Group Current / Peak / Capacity / Overflow
- Static Packet Cache Entry / Instance Current / Peak / Capacity
- Static Instance Group / Data Current / Peak / Capacity / Overflow
- 全計測値の一括Reset

Render Packet BufferとStatic Batch系Storageは全Scene共通、それ以外は選択Scene単位で表示する。

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

### Static Batch CPU Storage

Static Batchは最適化であり、Candidate / Packet Cache / Instance Dataのいずれかが失敗しても描画結果へ影響させない。

```text
Static Batch Capacity不足
    -> 対象Stageの部分データを全破棄
    -> overflowed=true
    -> 後続Static Stageを無効化
    -> 通常Render Packet提出を継続
```

`staticBatchReserve`はSceneごとに合算し、Candidate、Group、Packet Cache、Instance Dataの全CPU Storageへ適用する。明示ReserveはGrowth Eventへ含めない。

### Render Packet Frame Buffer

Render Packetは描画正当性に直結するため、容量不足時にPacketを拒否してはならない。

Merge前に`ShouldPublish`と同じ条件で公開予定Packet数を正確にPreflightする。設定Reserve適用後も容量が不足し、かつ対象Sceneのいずれかで`allowRuntimeGrowth=false`の場合は、描画欠落を避けるため完全Frameを保持できる容量までSafety Growthを行う。

```text
公開予定Packet数 > Capacity
    -> configured reserveを先に適用
    -> Growth許可あり: 通常Growth
    -> Growth許可なし: Safety Growth Override
    -> 全Packetを公開
    -> growthEventCountを加算
    -> safetyGrowthOverrideCountを加算
    -> Editor Telemetryへ警告表示
```

これは設定を黙って無視する挙動ではない。描画欠落より一時的な再確保を優先する明示的なCorrectness Fallbackであり、Safety Override回数と直近Frame使用有無を別Telemetryとして記録する。次回以降、確保済み容量内で収まる場合はOverrideを再計上しない。

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
- [x] Static Packet Cache Reserve
- [x] Static Instance Data Reserve
- [x] Componentなし / TransformなしEntity回帰
- [x] Dense / Sparse / DirectPaged Telemetry
- [x] Render Packet Telemetry
- [x] Static Batch Candidate / Cache / Instance Telemetry
- [x] 全Storage Growth統合表示
- [x] Entity Growth拒否Rollback
- [x] Component追加拒否時のMask非更新
- [x] Visibility容量不足時の全描画Fallback
- [x] Static Batch CPU Stage容量不足時の通常Packet Fallback
- [x] Render Packet Growth禁止時の完全Frame Safety Growth Fallback
- [x] Render Packet Safety Override Telemetry / Editor警告
- [x] 専用Smoke Test / Workflow

## 未完了

- [ ] GPU Instance Buffer
- [ ] Scene Save / Load実機回帰
- [ ] Debug / Release x64実機回帰

## 完了条件

- Scene Load前にStorage初期確保量を適用できる
- 初期確保量がHard Limitにならない
- Current / Peak / Capacity / GrowthをEditorで確認できる
- Growth禁止時に部分更新、誤Culling、描画欠落を発生させない
- Render Packetを含む全Storageで安全なOverflow処理を持つ
- 旧Sceneを設定Nodeなしで読み込める
