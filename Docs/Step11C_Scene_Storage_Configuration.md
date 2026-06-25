# Step 11-C Scene Storage Configuration

## 目的

Sceneごとに想定Entity数と主要Storageの初期確保量を設定できるようにし、Scene Load直後やRuntime中の再確保Spikeを減らす。

設定は最適化Hintであり、Entity数のHard Limitにはしない。

---

## 基本契約

- EntityはComponentを一つも持たない状態を許容する
- TransformComponentも必須ではない
- 初期確保量を超えた場合は、原則としてRuntime Growthを許可する
- Initial CapacityとHard Limitを混同しない
- DirectPagedのPage SizeはEngine全体のStorage実装契約とし、Sceneごとには変更しない
- Sceneごとに変更できるのは初期Page数やReserve数とする

---

## 設定データ

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

`expectedTransformCount`から必要Page数を自動計算できる場合、`preallocatedTransformPages`はAdvanced設定として扱う。

---

## 適用対象

### EntityRegistry

- Entity Slot配列Reserve
- Generation配列Reserve
- Free List Reserve
- Alive Entity配列Reserve

### ComponentRegistry

- Entity Mask管理領域Reserve
- Component登録補助Mapの初期化

### DirectPagedComponentStorage

- Transform Page Table Reserve
- Transform Page事前確保
- 将来のDirectPaged Data Component Page事前確保

### DirectPagedTagStorage

- Tag Page Table Reserve
- 必要に応じたPage事前確保

Tagごとに全Pageを事前生成するのではなく、Tag Storage単位のHintとして扱う。

### DenseComponentPool

Component型ごとのReserve Hintを受け取れる契約を追加する。

初期対象:

- ModelRendererComponent
- ColliderComponent
- CullingComponent
- Animation関連Component

### Rendering

- Render Packet Buffer Reserve
- View単位Visible Entity配列Reserve
- Culling結果Reserve
- Static Batch候補Reserve
- Static Instance Data Reserve

---

## Editor UI

Scene Settings Windowへ`Storage`タブを追加する。

```text
Scene Storage

Entity Storage
  Expected Entities             4096
  Expected Transforms           3072
  Expected Renderables          2048
  Expected Culling Components   2048
  Expected Static Entities      1024

Direct Paged Storage
  Transform Page Size            256  (Read Only)
  Preallocated Transform Pages    12
  Preallocated Tag Pages           4

Rendering Reserve
  Render Packets                2048
  Visible Entities              2048
  Static Batches                 128

Runtime Growth
  Allow Automatic Growth          [x]
  Log Capacity Growth             [x]
```

Page SizeはScene設定では変更不可とし、現在のEngine設定値をRead Onlyで表示する。

---

## 実測表示

設定値だけでなく、現在値とPeak値を表示する。

```text
                         Configured   Current   Peak
Entities                       4096      3280   3512
Transforms                     3072      2860   2904
Renderables                    2048      1692   1740
Culling Components             2048      1610   1668
Transform Pages                  12        12     12
Tag Pages                         4         3      4
Render Packets                 2048      1518   1594
Static Batches                  128        74     81

Capacity Growth Events: 0
```

将来的に次のEditor操作を追加する。

```text
Apply Measured Peak as Scene Defaults
Apply Peak + Safety Margin
Reset Runtime Peaks
```

Safety Marginは固定値ではなく割合指定を可能にする。

---

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

---

## Runtime Growth

初期確保を超えた場合は次の順で処理する。

1. Growth Eventを計測
2. 必要容量まで拡張
3. Debug LogまたはPerformance Monitorへ記録
4. Peak値を更新

`allowRuntimeGrowth=false`はDebug・固定容量検証向けとし、通常SceneではtrueをDefaultとする。

Growth拒否時はメモリ破壊や不完全なComponent追加を起こさず、操作全体を失敗としてRollbackする。

---

## 実装順

### Step 11-C.1 Reserve契約

- [ ] EntityRegistry Reserve
- [ ] DenseComponentPool Reserve
- [ ] DirectPaged Page Table Reserve
- [ ] DirectPaged Page Preallocation
- [ ] Render Packet / Culling / Static Batch Reserve

### Step 11-C.2 Scene設定

- [ ] `SceneStorageConfig`
- [ ] Scene YAML Encode / Decode
- [ ] 旧Scene Default互換
- [ ] Scene初期化前の設定適用

### Step 11-C.3 Editor UI

- [ ] Scene Settings Window
- [ ] Storageタブ
- [ ] Configured / Current / Peak表示
- [ ] Growth Event表示
- [ ] Peak値の設定反映操作

### Step 11-C.4 検証

- [ ] ComponentなしEntityの事前確保回帰
- [ ] TransformなしEntityの事前確保回帰
- [ ] Transform DirectPaged Page事前確保
- [ ] Tag DirectPaged Page事前確保
- [ ] 初期容量超過時Growth
- [ ] Growth無効時Rollback
- [ ] Scene Save / Load
- [ ] Debug / Release x64

---

## 完了条件

- Scene Load前にStorage初期確保量を適用できる
- 初期確保量がEntity数のHard Limitにならない
- DirectPaged Page SizeをSceneごとに変更しない
- Transform / TagのPage事前確保が可能
- Dense / Render Packet / Culling / Static BatchへReserve Hintを適用できる
- Current / Peak / Growth EventをEditorで確認できる
- 旧Sceneを設定Nodeなしで読み込める
- ComponentなしEntityとTransformなしEntityを正常に扱える
