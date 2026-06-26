# Step 11-C Scene Storage Configuration

## 目的

Sceneごとに想定Entity数と主要Storageの初期確保量を設定し、Scene Load直後やRuntime中の再確保Spikeを減らす。

設定は最適化Hintであり、Entity数のHard Limitではない。

## 基本契約

- EntityはComponentを一つも持たない状態を許容する
- TransformComponentも必須ではない
- 初期確保量を超えた場合は、原則としてRuntime Growthを許可する
- Initial CapacityとHard Limitを混同しない
- DirectPagedのPage SizeはEngine全体のStorage契約とし、Sceneごとには変更しない
- Sceneごとに変更できるのは初期Page数とReserve数とする

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

`preallocatedTransformPages == 0`と`preallocatedTagPages == 0`は、Expected Countから必要Page数を自動計算する。

## 適用対象

### EntityRegistry

- Entity Slot配列Reserve
- Generation配列Reserve
- Free List Reserve
- Alive Entity配列Reserve

### ComponentRegistry

- Entity Mask管理領域Reserve
- Component登録補助Mapの初期化

### DirectPaged Storage

- Transform Page Table Reserve
- Transform Page事前確保
- Tag Page Table Reserve
- Tag Page事前確保

Page SizeはEngine固定値で、Scene設定では変更不可とする。

### DenseComponentPool

Component型ごとのReserve Hintを受け取る。

初期対象:

- ModelRendererComponent
- ColliderComponent
- CullingComponent
- Animation関連Component

### Rendering

- Render Packet Frame Buffer Reserve
- View単位Visible Entity Set Reserve
- Static Batch候補Reserve
- Static Instance Data Reserve

## Editor UI

Scene Settings Windowの`Storage`タブで次を編集する。

```text
Entity Storage
  Expected Entities
  Expected Transforms
  Expected Renderables
  Expected Culling Components
  Expected Static Entities

Direct Paged Storage
  Page Size                        Read Only
  Preallocated Transform Pages
  Preallocated Tag Pages

Rendering Reserve
  Render Packets
  Visible Entities
  Static Batches

Runtime Growth
  Allow Automatic Growth
  Log Capacity Growth
```

## 実測表示

```text
                         Configured   Current   Peak
Entities                       4096      3280   3512
Transforms                     3072      2860   2904
Renderables                    2048      1692   1740
Culling Components             2048      1610   1668
Transform Pages                  12        12     12
Static Tag Pages                  4         3      4
Render Packets                 2048      1518   1594
Visible Entities / View        2048      1420   1492

Entity Capacity Growth Events: 0
Visibility Capacity Growth Events: 0
```

Current / Peak / Growthは選択中Sceneの`SceneContextID`で分離する。

操作:

- Apply Measured Peak
- Apply Peak + Margin
- Reset Runtime Peaks

Safety Marginは0〜100%の割合指定とする。

`visibleEntityReserve`へ反映する値はCullingComponent総数ではなく、Viewごとの実測最大可視数を使う。

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

## Runtime Growth

初期確保を超えた場合は次の順で処理する。

1. Growth Eventを計測
2. 必要容量まで拡張
3. Debug LogまたはPerformance Monitorへ記録
4. Peak値を更新

`allowRuntimeGrowth=false`は固定容量検証向けとし、通常SceneではtrueをDefaultとする。

Growth拒否時はメモリ破壊や不完全なComponent追加を起こさず、操作全体を失敗としてRollbackする。

## 現在の実装状態

- `SceneStoragePreloader`がScene YAMLからStorage設定をRegistry生成前に先読みする
- `Scene::Initialize`が最初のEntity生成前に`SceneStorageRuntime::Apply`を呼ぶ
- `Scene::Save`と`Scene::TempSave`が`SceneSettings.Storage`を保存する
- 初期化後の別Scene読込でも、Entity生成前に設定をDecodeして再適用する
- 設定Nodeが存在しない旧Sceneでは既定設定を維持する
- Sceneごとの`renderPacketReserve`を最終`RenderPacketFrameBuffer`へ適用する
- Sceneごとの`visibleEntityReserve`をView単位Visibility Setへ適用する
- Visibility Setの確保容量はStable View間でFrameを跨いで再利用する
- Active Sceneを選択してStorage設定を編集・保存できる
- Configured / Current / Peakを表示できる
- EntityとVisibilityのGrowth EventをScene単位で表示・リセットできる
- 計測Peakをそのまま、またはSafety Margin付きでScene設定へ反映できる
- Component Storage、Render Packet、Static Batchを横断した統一Growth Event集計は未完了
- `staticBatchReserve`の実接続はStatic Batching基盤完成後に行う

## 実装順

### Step 11-C.1 Reserve契約

- [x] EntityRegistry Reserve
- [x] DenseComponentPool Reserve
- [x] DirectPaged Page Table Reserve
- [x] DirectPaged Page Preallocation
- [x] Culling Component Storage Reserve
- [x] Render Packet Frame Buffer ReserveのScene別接続
- [x] Visible Entity ReserveのScene別接続
- [ ] Static Batch ReserveのScene別接続

### Step 11-C.2 Scene設定

- [x] `SceneStorageConfig`
- [x] Scene YAML Encode / Decode
- [x] 旧Scene Default互換
- [x] Scene初期化前の設定適用
- [x] 通常保存 / 一時保存への接続
- [x] 初期化後のScene再読込経路への再適用

### Step 11-C.3 Editor UI

- [x] Scene Settings Window
- [x] Storageタブ
- [x] Configured / Current / Peak表示
- [x] Entity Growth Event表示
- [x] Visibility Current / Peak / Growth Event表示
- [x] Peak値の設定反映操作
- [ ] 全Storage統合Growth Event表示

### Step 11-C.4 検証

- [ ] ComponentなしEntityの事前確保回帰
- [ ] TransformなしEntityの事前確保回帰
- [x] Transform DirectPaged Page事前確保
- [x] Tag DirectPaged Page事前確保
- [x] Render Packet Frame Buffer Reserve
- [ ] 初期容量超過時Growth
- [ ] Growth無効時Rollback
- [ ] Scene Save / Load実機回帰
- [ ] Debug / Release x64

## 完了条件

- Scene Load前にStorage初期確保量を適用できる
- 初期確保量がEntity数のHard Limitにならない
- DirectPaged Page SizeをSceneごとに変更しない
- Transform / TagのPage事前確保が可能
- Dense / Render Packet / Culling / Static BatchへReserve Hintを適用できる
- Current / Peak / Growth EventをEditorで確認できる
- 旧Sceneを設定Nodeなしで読み込める
- ComponentなしEntityとTransformなしEntityを正常に扱える
