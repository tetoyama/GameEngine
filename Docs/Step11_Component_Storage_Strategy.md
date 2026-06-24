# Step 11-B Component Storage Strategy

## 目的

Componentごとのアクセス特性とメモリ特性に応じて、次の3方式からStorageを選択できる構造へ再整理する。

- `SparseComponentStorage<T>`: メモリ効率優先
- `DenseComponentPool<T>`: 連続走査・バッチ処理優先
- `DirectPagedComponentStorage<T>`: Entity単体アクセス・位置安定性優先

`ChunkedDenseComponentPool`は独立Storageとして導入しない。現在の`DenseComponentPool`は、存在するComponentだけを連続配置する用途として十分であり、その役割を維持する。

本Stepでは、従来`COMPONENT_ARCHETYPE`と呼んでいたStorageの意図を明確化し、一般的なArchetype ECSとの名称衝突を解消する。

---

# 1. 背景

現在のComponent Registryは、Component型ごとに次のどちらかを選択している。

```text
SparseStorage<T>
DenseComponentPool<T>
```

現在の`COMPONENT_ARCHETYPE`は一般的なArchetype ECSではなく、`DenseComponentPool<T>`の別名として使用されている。

元の設計意図は、Componentごとに次のトレードオフを選択することだった。

```text
Sparse
    Componentを持つEntityだけ実体を確保
    メモリ消費量が少ない
    Entity単体アクセスはHash検索を伴う

旧Archetype
    Entity IDに対応する領域を確保
    メモリ消費量は増える
    Entity単体アクセスを軽くする
```

一方、現在の`DenseComponentPool<T>`は、存在するComponentだけを連続配置し、Entity IDからSparse Indexを経由してDense Indexへ変換する方式である。

```text
Entity ID
    ↓ Sparse Index
Dense Index
    ↓
Component Array
```

この方式は全件連続走査に強いが、元の「メモリを多く使う代わりにEntity IDから直接アクセスするStorage」とは役割が異なる。

したがって、現在のDenseを削除せず、DirectPagedを追加して3種類へ分離する。

---

# 2. Storage分類

## 2.1 SparseComponentStorage

### 目的

メモリ効率、低占有率、大型Component、ポインタ安定性を優先する。

### 基本構造

```cpp
template<typename T>
class SparseComponentStorage
{
    std::unordered_map<Entity, T> components;
};
```

### 強み

- Componentを持つEntity分だけ実体を確保する
- Entity IDが疎でも未使用領域を確保しない
- 大型・可変長Componentに適する
- ScriptやNative Resource参照を持つComponentを移動させにくい

### 弱み

- `GetComponent`でHash検索が必要
- Node配置により連続走査が弱い
- RehashやNode allocationのコストがある
- 大量バッチ処理には不向き

### 適用条件

- 所有Entityが少ない
- Componentが大きい
- `std::vector`、`std::string`、Resource Handleなど可変長・非自明型を持つ
- 全件走査より個別利用が多い
- Script / Editor / Runtime Resourceとの結合が強い

---

## 2.2 DenseComponentPool

### 目的

存在するComponentだけを隙間なく連続配置し、全件走査、バッチ生成、SIMD、範囲分割を優先する。

### 現在の基本構造

```cpp
template<typename T>
class DenseComponentPool
{
    std::vector<T> components;
    std::vector<Entity> entities;
    std::vector<uint32_t> sparseIndices;
};
```

### Entity IDに欠番がある場合

Entity ID 1～10、15～20だけがComponentを持つ場合、Component配列には11～14の穴を作らない。

```text
components
[T1][T2]...[T10][T15]...[T20]

entities
[E1][E2]...[E10][E15]...[E20]
```

欠番はSparse Index側で`InvalidDenseIndex`として表す。

```text
sparseIndices[11] = Invalid
sparseIndices[12] = Invalid
sparseIndices[13] = Invalid
sparseIndices[14] = Invalid
```

### 追加と容量拡張

`std::vector`のCapacityへ到達した場合、より大きな連続領域へ再確保される。

```text
新領域を確保
    ↓
既存ComponentをMove / Copy
    ↓
旧領域を解放
    ↓
新Componentを追加
```

通常Denseではこの動作を許容する。

必要に応じてScene Load時や大量生成前に`reserve()`を行い、Runtime中の再確保Spikeを抑える。

### 削除

Swap Removeを使用する。

```text
[A][B][C][D]
    B削除
[A][D][C]
```

移動したEntityのSparse Indexを更新する。

### 強み

- 存在するComponent本体が完全に連続する
- 全件走査のCache効率が高い
- Render Packet BuildやCullingのDriver列に適する
- `span`やRangeで分割し、Job Systemへ投入しやすい
- 未使用Entity ID分のComponent実体を確保しない
- Sparseよりメモリ局所性が高い

### 弱み

- Entity単体取得にはSparse Index変換が必要
- 容量拡張時に全Componentが移動する
- Swap Removeで別Componentの位置が移動する
- Component Pointerを長期間保持できない
- 複数Dense Pool間でDense Indexは一致しない

### 適用条件

- 毎フレーム大量に全件走査する
- Batch / Packet / Proxy生成のDriverになる
- Component移動を許容できる
- PointerをFrameやPhaseを跨いで保持しない
- 大量生成前に必要容量を予測できる

### DenseをChunk化しない理由

独立したChunked Dense Storageは、次の複雑性を増やす。

- Entity Locationが`chunk + slot`になる
- Chunk跨ぎSwap Removeが必要になる
- 空Chunk管理が必要になる
- Query / Span APIが複雑になる
- 現在の連続`std::span<T>`契約を分断する

現時点では、通常Denseの単純性と完全な連続性を優先する。

再確保Spikeは次で対処する。

- `reserve()`
- Scene / Prefab Load時の所有数事前集計
- Runtime大量生成前のCapacity予約
- Component Pointerを構造変更後に再解決する契約

Chunk化は、通常Denseでは実測上解決できない再確保または巨大連続領域問題が発生した場合だけ再検討する。

---

## 2.3 DirectPagedComponentStorage

### 目的

Entity IDからComponentへ直接到達し、単体アクセス、Pointer位置安定性、固定Page拡張を優先する。

### 基本構造

Entity IndexをPageとSlotへ分解する。

```cpp
pageIndex = entity.GetIndex() / PageCapacity;
slotIndex = entity.GetIndex() % PageCapacity;
```

```cpp
template<typename T>
class DirectPagedComponentStorage
{
    std::vector<std::unique_ptr<ComponentPage<T>>> pages;
};
```

Pageは固定Slot、生Storage、占有Bitを持つ。

```cpp
template<typename T, size_t Capacity>
struct ComponentPage
{
    RawStorage<T, Capacity> storage;
    OccupancyMask<Capacity> occupied;
};
```

Componentを持たないSlotについて、メモリ領域はPage内に存在するが、`T`オブジェクトは構築しない。

### Entity ID 1～10、15～20の例

```text
Page 0
slot 1～10   : Component構築済み
slot 11～14  : 未構築
slot 15～20  : Component構築済み
その他       : 未構築
```

### 取得

Hash検索やDense Index変換を行わない。

```cpp
T* Get(Entity entity)
{
    const uint32_t page = entity.GetIndex() / PageCapacity;
    const uint32_t slot = entity.GetIndex() % PageCapacity;

    if (page >= pages.size() || !pages[page]) return nullptr;
    if (!pages[page]->occupied.Test(slot)) return nullptr;
    return pages[page]->Get(slot);
}
```

### Free Listとの関係

Free ListはComponent StorageではなくEntity Registryが管理する。

```text
Entity破棄
    ↓
Entity IndexをFree Listへ返却
    ↓
次回Entity生成時にIndexを再利用
    ↓
既存Page内の空Slotを再利用
```

これによりEntity Indexの無制限増加とPageの不要拡張を抑える。

### 強み

- Entity IDからPage + Slotを直接計算できる
- Hash検索不要
- Sparse Index不要
- Page追加時に既存Componentを移動しない
- 他Entity削除時にComponent位置が変わらない
- Pageが存続する限りPointer位置が安定する
- Page単位で並列走査できる

### 弱み

- 未使用Slot分の生メモリ領域を確保する
- 低占有・大型Componentではメモリ浪費が大きい
- 穴が多い場合、全件走査はDenseより弱い
- Entity IDと空間的局所性は自動では一致しない

### Occupancy走査

空Slotを1件ずつ判定せず、64bit単位で占有Bitを走査する。

```cpp
uint64_t mask = occupiedWords[wordIndex];
while (mask != 0)
{
    const uint32_t bit = std::countr_zero(mask);
    const uint32_t slot = wordIndex * 64 + bit;
    Process(*GetBySlot(slot));
    mask &= mask - 1;
}
```

### Page容量

全Componentで同一個数にせず、目標Byte数を基準に決める。

```cpp
static constexpr size_t TargetPageBytes = 16 * 1024;
static constexpr size_t PageCapacity =
    std::max<size_t>(1, TargetPageBytes / sizeof(T));
```

### 適用条件

- 多くのEntityが所有する
- 小型・固定サイズである
- Entity単体から非常に頻繁に取得される
- Component位置を安定させたい
- 未使用Slot分のメモリを許容できる

---

# 3. 三方式の比較

| 特性 | Sparse | Dense | DirectPaged |
|---|---|---|---|
| Component実体確保 | 所有Entityのみ | 所有Entityのみ | Page全Slot分の生領域 |
| Entity単体取得 | Hash検索 | Sparse Index変換 | Page + Slot直接計算 |
| 全件走査 | 弱い | 最も強い | Occupancy次第 |
| Pointer安定性 | Node実装依存 | 再確保・Swap Removeで移動 | Page破棄まで高い |
| 追加時の全体移動 | Rehash可能性 | Capacity超過時にあり | なし |
| 削除時の他要素移動 | なし | あり | なし |
| 低占有率 | 強い | 強い | 弱い |
| 大型Component | 強い | 条件付き | 弱い |
| Range並列化 | 弱い | 強い | Page単位で可能 |
| 完全な連続配列 | なし | あり | Page内のみ |

---

# 4. Query契約

SystemはStorage種別を意識しない。

```cpp
auto query = registry.Query<
    Read<TransformComponent>,
    Read<ModelRendererComponent>,
    Write<VisibilityComponent>
>();
```

Registry内部でQuery Planを構築する。

```text
Driver Storage
    全件走査対象のうち最も効率的なStorage

Join Storage
    Entity IDから各Componentを取得
```

例:

```text
RenderProxy DenseをDriverとして連続走査
    ↓ Entity ID
Transform DirectPagedへ直接アクセス
Material DirectPagedへ直接アクセス
Texture SparseへHashアクセス
```

### Driver選択の基本優先度

1. `DenseComponentPool`のうち最小要素数
2. 高占有`DirectPagedComponentStorage`の有効Page
3. `SparseComponentStorage`のEntity列
4. 全Alive Entity走査はFallbackに限定

### Storage参照解決

`GetComponent<T>()`ごとに`m_storages.find(typeid(T))`を行わず、Query生成時またはSystem Task開始時にStorage Pointerを一度だけ解決する。

### Dense範囲分割

Denseは単一連続配列のまま、Index RangeをJobへ分割する。

```cpp
ParallelFor(
    dense.Size(),
    grainSize,
    [&](size_t begin, size_t end)
    {
        auto components = dense.Components().subspan(begin, end - begin);
        auto entities = dense.Entities().subspan(begin, end - begin);
        Process(entities, components);
    }
);
```

### 構造変更

- Query実行中にStorage構造を変更しない
- Add / RemoveはEntityCommandBufferへ記録する
- Playback後にStructure Versionを更新する
- Query ViewはStructure Version不一致時に無効化する

---

# 5. 現在のComponent暫定分類

## 5.1 DirectPaged候補

### TransformComponent

最終的にDirectPagedへ移行する。

理由:

- 多くのEntityが所有する
- Script、Physics、Editor、Camera、RenderからEntity指定で頻繁に取得される
- 他Entity削除による位置移動を避けたい

ただし現在は`std::vector<Entity> children`を保持しているため、先にHierarchy情報を専用Storageへ分離する。

```text
TransformComponent
    position / rotation / scale / parent

HierarchyStorage
    parent -> children adjacency
```

### MaterialComponent

DirectPagedへ移行する第一候補。

現在は次のみを保持する固定サイズ値型である。

```text
ShaderID
MATERIAL PBR parameters
```

RendererやRender Packet BuildからEntity IDで繰り返し取得される。

Materialへ可変長Texture配列、String、Native Resource所有を追加しない。必要な場合はHandle化し、実体はResource Managerへ置く。

### RenderLayerComponent / OrderInLayerComponent

小型・固定サイズであり、描画分類時にEntity IDから頻繁に取得されるためDirectPaged候補。

### NameComponent

多くのEntityが必ず持つ運用ならDirectPaged。

Editor用途だけで低占有になる場合はSparseも許容する。実測とScene契約で確定する。

---

## 5.2 Dense候補

### LightComponent

Lighting用に全Lightを毎フレーム列挙するためDenseを維持する。

### RenderProxy / RenderPacket入力

大型の永続Renderer Componentを直接Dense化せず、毎フレーム必要な軽量データをDenseへ抽出する。

```text
ModelRendererComponent: Sparse
    ↓ Prepare / Build
RenderProxy / RenderPacket Input: Dense
    ↓ Culling / Sort / Submit
```

### WorldBoundsComponent

将来追加時はDense。

Cullingで連続走査し、Index Range単位でWorkerへ分割する。

### VisibilityComponent

将来追加時はDense。

Culling結果とRender Packet Build入力として大量処理する。

### Animation Pose Result

Frame-local Runtime結果としてDense。

永続Animation設定とGPU / CPU Runtime結果を分離する。

### Particle Runtime Pool

粒子実体はEmitter Componentから分離し、専用Dense Runtime Poolへ置く。

---

## 5.3 Sparse候補

### ModelRendererComponent

現状は以下を持つ大型・非自明型である。

- `shared_ptr<ModelData>`
- `string`
- Animation用`vector`
- Blend用`vector`
- D3D11 Buffer参照用`vector`
- Destructor / Resource解放処理

現状のままDenseやDirectPagedへ移さない。

### Renderer設定Component

次を現状Sparseで維持する。

- `MeshRendererComponent`
- `BillBoardRendererComponent`
- `SpriteRendererComponent`
- `TerrainComponent`
- `WaveComponent`
- `EffectComponent`
- `OutlineComponent`

必要なRuntime描画データはRenderProxy / RenderPacketへ抽出する。

### ColliderComponent

複数Colliderの可変長配列とPhysics連携状態を持つためSparseを維持する。

将来、BroadPhase入力用のBounds / Shape ProxyだけDenseへ分離する。

### ParticleComponent

現在は`PARTICLE[512]`を内包しているため、Dense登録からSparseへ変更する。

将来構造:

```text
ParticleEmitterComponent
    小型設定 / Runtime Handle

ParticleRuntimePool
    粒子実体のDense配列
```

### CameraComponent

通常個数が少ないためSparse。

Active Camera参照は専用Handle / Indexで管理し、毎回全件走査しない。

### EnvironmentMapComponent / FollowComponent

低占有・特殊用途のためSparse。

### Script系

すべてSparseを維持する。

- `ScriptComponent`
- `CustomScriptComponent`
- Script派生型

ScriptはMainThread / World Exclusiveであり、全Resourceへアクセスできる例外Componentとして扱う。

### PrefabComponent / Editor専用状態

低頻度・保存用途中心のためSparse。

---

# 6. 暫定登録表

```cpp
enum class ComponentStorageKind
{
    Sparse,
    Dense,
    DirectPaged
};
```

```cpp
#define COMPONENT_LIST(X) \
    X(NameComponent,                DirectPaged) \
    X(TransformComponent,           DirectPaged) \
    X(CustomScriptComponent,        Sparse) \
    X(ColliderComponent,            Sparse) \
    X(AudioComponent,               Sparse) \
    X(RenderLayerComponent,         DirectPaged) \
    X(OrderInLayerComponent,        DirectPaged) \
    X(MaterialComponent,            DirectPaged) \
    X(TextureComponent,             Sparse) \
    X(BumpMapComponent,             Sparse) \
    X(LightComponent,               Dense) \
    X(MeshRendererComponent,        Sparse) \
    X(ModelRendererComponent,       Sparse) \
    X(BillBoardRendererComponent,   Sparse) \
    X(SpriteRendererComponent,      Sparse) \
    X(TerrainComponent,             Sparse) \
    X(WaveComponent,                Sparse) \
    X(OutlineComponent,             Sparse) \
    X(ParticleComponent,            Sparse) \
    X(EffectComponent,              Sparse) \
    X(CameraComponent,              Sparse) \
    X(PrefabComponent,              Sparse) \
    X(FollowComponent,              Sparse) \
    X(EnvironmentMapComponent,      Sparse)
```

この表は実装前の暫定判断であり、Component実体、保持率、Get回数、全件走査回数を計測して確定する。

---

# 7. Streamingとの関係

通常DenseはWorld全体の連続走査用Storageとして扱う。

Streaming Cell単位のメモリ解放や空間局所性は、Dense Storage自体をChunk化して解決しない。

Streamingは別の上位境界として設計する。

```text
World / Scene
 ├─ Persistent Entity Group
 ├─ Streaming Cell A Entity Group
 └─ Streaming Cell B Entity Group
```

### 基本契約

- EntityへStreaming Cell / Region所属を持たせる
- Active Cell Entity一覧またはMaskをQuery Filterとして渡す
- Storage内部配置とStreaming所属を直接一致させない
- Entity IDと空間的位置が一致するとは仮定しない
- Cell UnloadはEntityCommandBufferでまとめて破棄する
- Block単位解放が必要になった場合はStreaming専用World / Registry分割を検討する

通常Denseの役割は、ロード済みWorld内での連続走査に限定する。

---

# 8. 実装手順

## Phase 1: 名称と登録契約の整理

- [ ] `COMPONENT_ARCHETYPE`を廃止する
- [ ] `ComponentStorageKind`を`Sparse / Dense / DirectPaged`へ変更する
- [ ] `RegisterYAMLComponent<T>(bool)`をStorage Kind指定へ変更する
- [ ] 旧`ArchetypeStorage` aliasを削除する
- [ ] 一般的Archetype ECSとの名称衝突を解消する

## Phase 2: DirectPaged基盤

- [ ] `DirectPagedComponentStorage<T>`を追加する
- [ ] Raw Storage / placement constructionを実装する
- [ ] Occupancy Bitを実装する
- [ ] Entity IndexからPage + Slotを直接計算する
- [ ] Page未確保時のLazy Allocation
- [ ] Remove時のdestroy / generation更新
- [ ] Page追加で既存Pointerが移動しないことを確認する
- [ ] Occupancy Word単位Iterationを実装する

## Phase 3: 既存Denseの整備

- [ ] `DenseComponentPool<T>`を全件走査用Storageとして正式化する
- [ ] `Reserve<T>(count)` APIを追加する
- [ ] Scene / Prefab Load前のComponent数集計を検討する
- [ ] Dense `Components()` / `Entities()` Span契約を維持する
- [ ] Index Range単位`ParallelFor`を追加する
- [ ] 再確保・Swap Remove後のPointer無効化契約を文書化する
- [ ] DenseのAdd / Remove / Structure Version計測を追加する

## Phase 4: Query Planner

- [ ] Storage種別を隠蔽する共通Iteration契約
- [ ] Dense最小列をDriverに選択する
- [ ] DirectPaged JoinをPage + Slot直接取得にする
- [ ] Sparse JoinをHash Lookupにする
- [ ] 全Alive Entity走査をFallbackへ限定する
- [ ] Storage PointerをQuery生成時に一度だけ解決する
- [ ] Structure Version検証
- [ ] Query中構造変更禁止

## Phase 5: Component移行

- [ ] TransformからChildren配列をHierarchy Storageへ分離
- [ ] TransformをDirectPagedへ移行
- [ ] MaterialをDirectPagedへ移行
- [ ] RenderLayer / OrderInLayerをDirectPagedへ移行
- [ ] LightをDenseで維持する
- [ ] ParticleをSparseへ変更
- [ ] Particle Runtime PoolをDenseへ分離
- [ ] Renderer設定ComponentをSparse維持
- [ ] RenderProxy / RenderPacket入力をDense Driver化
- [ ] Camera / EnvironmentMap / FollowをSparseへ変更

## Phase 6: Streaming拡張準備

- [ ] EntityのStreaming Group ID契約を設計する
- [ ] Persistent GroupとStreaming Groupを区別する
- [ ] QueryへActive Group Filterを追加できるようにする
- [ ] Cell単位Entity破棄Commandを設計する
- [ ] World / Registry分割が必要になる条件を定義する

---

# 9. 計測項目

Storage選択は推測だけで確定せず、次をComponent型ごとに計測する。

- Component所有Entity数
- 全Alive Entityに対する占有率
- `GetComponent<T>()`呼び出し回数
- 全件走査回数
- Add / Remove回数
- Component平均・最大サイズ
- Query Join回数
- Pointer保持期間
- Dense `size / capacity`
- Direct Page占有率
- Sparse Node / Bucket推定メモリ
- Dense再確保時間
- Dense Swap Remove時間

### 最低比較対象

- Transform取得
- Render Packet Build
- Physics Transform同期
- Editor Inspector選択
- Hierarchy更新
- Light列挙
- Particle更新

---

# 10. 検証項目

## Unit / Smoke

- [ ] Sparse Add / Get / Remove
- [ ] Dense Add / Get / Swap Remove
- [ ] DirectPaged Add / Get / Remove
- [ ] Entity generation不一致拒否
- [ ] Component generation不一致拒否
- [ ] Free Entity Index再利用
- [ ] Page追加後の既存Pointer維持
- [ ] Dense Reserve後のCapacity確認
- [ ] Dense再確保後のStructure Version更新
- [ ] DirectPaged Occupancy Iteration
- [ ] Dense Span Iteration
- [ ] 複数Storage混在Query
- [ ] EntityCommandBuffer Playback

## Runtime

- [ ] Scene Load / Save回帰なし
- [ ] Inspector Add / Remove回帰なし
- [ ] Prefab回帰なし
- [ ] Script Component回帰なし
- [ ] Physics / Collider回帰なし
- [ ] Player / Editor描画回帰なし
- [ ] Render Packet Build回帰なし
- [ ] Entity大量生成・破棄
- [ ] Free List再利用後の古い参照拒否

## Performance

- [ ] Transformランダムアクセス比較
- [ ] Dense全件走査比較
- [ ] Mixed Query比較
- [ ] Dense Reserve有無の追加Spike比較
- [ ] Component削除時Spike比較
- [ ] Scene Load Peak Memory比較

---

# 11. 完了条件

- ComponentごとにStorage Kindを明示できる
- `Sparse / Dense / DirectPaged`の役割が重複せず説明できる
- System / Query利用側がStorage種別へ直接依存しない
- Queryが全Alive Entity走査を標準経路にしない
- TransformとMaterialのEntity単体取得がHash検索を行わない
- Dense Driver列をIndex Range単位で並列走査できる
- Dense再確保を`reserve()`で制御できる
- Script / Resource所有ComponentのPointer安定性が維持される
- Entity Free List再利用とDirect Page Slot再利用が整合する
- Scene / Prefab / YAML / Inspectorに回帰がない
- Storage選択前後のCPU時間とPeak Memoryを比較できる

---

# 12. 実装優先順位

現在進行中のFrame Pacing計測契約修正を完了後に着手する。

推奨順:

1. GPU / CPU Frame Serial対応を完了
2. Storage名称・登録契約を`Sparse / Dense / DirectPaged`へ変更
3. DirectPagedのUnit Test基盤
4. Dense Reserve / Range Parallel API
5. Query PlannerのStorage駆動化
6. Material / Layer系をDirectPagedへ移行
7. Transform Hierarchy分離
8. TransformをDirectPagedへ移行
9. Particle登録修正とRuntime Pool分離
10. Render Proxy / Packet DriverのDense活用
11. Streaming Group Filterを別Stepで開始

実装中は既存`SparseStorage`と`DenseComponentPool`を互換経路として維持し、Component型ごとに段階移行する。一括置換は行わない。
