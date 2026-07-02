# Entity / Component State / Culling Contract

## 目的

Entity本体を世代付きIDと寿命管理だけに限定し、状態・機能・描画最適化の責務をComponentとSystemへ分離する。

この文書はStep 11-B Component Storage Strategy、Step 11-C Scene Storage Configuration、Step 18 RenderWorld / Static Batchingの共通契約を定義する。

---

## Entity基本契約

EntityはComponentを一つも持たない状態を正式に許容する。

```text
Entity
    index
    generation
```

Entity本体へ次のFlagを追加しない。

- Enabled
- Static
- Visible
- Culling
- Transform有無
- Renderable有無

何もComponentを持たないEntityの標準状態は次とする。

```text
Enabled
Dynamic
Visible
No Culling
No Transform
No Rendering
```

空EntityもScene保存・読込・複製・破棄・Undo / Redoの対象として正常に扱う。

---

## 責務分離

```text
Entity
    識別子と寿命

Component
    状態・機能・データ

System
    Component構成に応じた処理

Storage
    Componentの性質に応じた保存方式
```

SystemはEntityにTransformやRendererが必ず存在すると仮定しない。

必要なComponent構成だけをQueryする。

---

## 共通Entity状態

次の状態はComponentの存在で表す。

```cpp
struct DisabledComponent : IComponent {};
struct StaticEntityComponent : IComponent {};
struct HiddenComponent : IComponent {};
```

意味:

```text
DisabledComponentなし
    Enabled

StaticEntityComponentあり
    Static

HiddenComponentなし
    Visible
```

標準状態をComponentなしで表すため、`EnabledComponent`や`VisibleComponent`は作らない。

---

## CullingComponent

`CullingComponent`は自動生成しない。

Componentが存在すること自体が、対象EntityへCullingを適用する明示的な宣言となる。

```cpp
struct CullingComponent : IComponent {
    AABB localBounds;
    AABB worldBounds;
    uint64_t sourceRevision = 0;
    uint64_t transformRevision = 0;
    bool boundsValid = false;
};
```

Attach / DetachはユーザーまたはEditor操作で決める。

内部データはCullingSystemが生成・更新する。

- Local Bounds
- World Bounds
- Source Revision
- Transform Revision
- Bounds Validity

これらはInspector編集対象にしない。

Bounds自体はScene YAMLへ保存せず、Renderable ResourceとTransformから再構築する。

Bounds生成に失敗したEntityは安全側へ倒し、描画候補として残す。

---

## Editor共通ヘッダー

Enable、Static、Culling、Visibleは通常Component一覧ではなく、Inspector上部のEntity共通ヘッダーへ表示する。

```text
[✓] Enable    [ ] Static    [✓] Culling    [✓] Visible
```

操作はComponent / TagのAttach / Detachへ変換する。

```text
Enable OFF
    DisabledComponent Attach

Enable ON
    DisabledComponent Detach

Static ON
    StaticEntityComponent Attach

Static OFF
    StaticEntityComponent Detach

Culling ON
    CullingComponent Attach

Culling OFF
    CullingComponent Detach

Visible OFF
    HiddenComponent Attach

Visible ON
    HiddenComponent Detach
```

構造変更はEditor CommandまたはEntityCommandBufferを経由する。

Undo / RedoはAttach / Detach操作を復元する。

複数Entity選択時はChecked / Unchecked / Mixedを表示する。

---

## EnableとVisible

EnableとVisibleは独立した責務を持つ。

```text
Enable OFF
    Entity全体を更新対象から除外
    Script / Physics / Animation / Rendering等を停止

Visible OFF
    Script / Physics / Animation等は維持
    Renderingだけを除外
```

SystemごとのDisabled Entity除外契約を統一する。

---

## StaticとCulling

StaticとCullingは独立して設定できる。

```text
Static OFF + Culling OFF
    Dynamic、常に描画候補

Static OFF + Culling ON
    Dynamic、Entity AABBでCulling

Static ON + Culling OFF
    Static Batch対象、常時提出

Static ON + Culling ON
    Static Batch対象、Batch / Spatial Cell AABBでCulling
```

Static Attach時にCullingを自動Attachしない。

Culling Attach時にStaticを自動Attachしない。

---

## Component Storage

### TransformComponent

Transformは必須Componentではない。

Storageは`DirectPagedComponentStorage<TransformComponent>`へ移行する。

目的:

- Entity indexから直接取得
- Transform参照アドレスの安定化
- Swap Remove回避
- Parent / Child参照の単純化
- TransformなしEntityの自然な表現
- Page単位走査とParallelFor

### Tag Component

次のTagは`DirectPagedTagStorage<T>`を使用する。

- DisabledComponent
- StaticEntityComponent
- HiddenComponent
- EditorOnlyComponent
- DontDestroyComponent

Tag実体は保持せず、Presence Bit、Entity generation、Component generationだけを持つ。

### CullingComponent

CullingComponentは値を持つためTag Storageにはしない。

初期StorageはDenseとする。

CullingSystemによる連続走査とBounds更新の局所性を優先する。

### 将来Archetype

RegistryとEntityCommandBufferは具体Storage型を知らない構造にする。

Dense、DirectPaged、SparseStable、Archetypeを同じ`IComponentStorage`契約で扱う。

---

## Culling処理

Renderable ResourceがLocal Boundsを提供する。

```text
Model
    頂点生成時のMin / Max

Mesh
    Mesh Resource Local Bounds

Sprite / Billboard
    Size / Pivot

Terrain
    Terrain Mesh範囲

Wave
    最大変位を含む範囲

Particle / Effect
    設定値または保守的Bounds
```

CullingSystemはTransform Revisionが変化した時だけWorld Boundsを更新する。

Skinned Model、Particle、Waveなど形状が変化するものは事前計算Boundsまたは保守的Paddingを使用する。

単一の`isVisible`はComponentへ保存しない。

Editor CameraとPlayer Cameraでは可視結果が異なるため、View単位の一時結果をCullingSystemまたはRenderWorldへ保持する。

---

## CullingとRender Packet

実行順:

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

Culling対象外EntityはVisibility判定を行わず描画候補へ残す。

Culling対象Entityは可視な場合だけRender Packetを生成する。

これにより次を削減する。

- Packet Build
- Sort対象数
- Submit処理
- Draw Call
- Vertex / Pixel処理

---

## Static Batching

Static Batchingは次の段階で導入する。

```text
Static State Batching
    State変更削減

Static Instancing
    DrawIndexedInstanced

Static Geometry Batching
    Mesh結合によるDraw Call統合
```

Culling対象のStatic EntityはSpatial Cell単位へ分類し、Batch AABBで判定する。

巨大な単一Batchを作らず、Scene、Pass、Material / Pipeline、Spatial Cellで分割する。

Static BatchはFrame-local Component Pointerを保持しない。

---

## Scene Storage Configuration

Sceneごとに初期確保Hintを設定できるようにする。

対象:

- Entity数
- Transform数
- Renderable数
- CullingComponent数
- Static Entity数
- Transform DirectPaged Page数
- Tag DirectPaged Page数
- Render Packet Reserve
- Visible Entity Reserve
- Static Batch Reserve

設定はHard Limitではなく初期確保量とする。

DirectPaged Page SizeはEngine全体で固定し、Scene UIではRead Only表示する。

Scene Settings WindowでConfigured / Current / Peak / Growth Eventを確認できるようにする。

詳細:

- `Docs/Step11B_Component_Storage_Strategy.md`
- `Docs/Step11C_Scene_Storage_Configuration.md`
- `Docs/Step18_Static_Entity_Batching_Plan.md`

---

## 実装順

1. Component Storage Strategy契約
2. DirectPaged Data Storage
3. Transform DirectPaged移行
4. DirectPaged Tag Storage
5. Disabled / Static / Hidden Tag
6. Entity共通ヘッダー
7. Scene Storage Reserve契約
8. Scene Settings Storage UI
9. CullingComponentと共通AABB
10. CullingSystem
11. Packet Build前Frustum Culling
12. RenderWorld
13. Static State Batching
14. Static Instancing
15. Static Geometry Batching

---

## 完了条件

- ComponentなしEntityが正式に動作する
- TransformなしEntityが正式に動作する
- Entity本体へ状態Flagを持たせない
- Enable / Static / Culling / Visibleを共通ヘッダーから操作できる
- Tag ComponentがDirectPagedで保存される
- TransformComponentがDirectPagedで保存される
- CullingComponentの存在がCulling有効を表す
- Culling Boundsをユーザー編集項目にしない
- Editor / Player Viewで独立した可視結果を使用する
- Sceneごとの事前確保Hintを設定できる
- Static BatchとCullingを独立して使用できる
