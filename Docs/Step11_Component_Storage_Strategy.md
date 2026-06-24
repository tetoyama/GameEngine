# Step 11-B Component Storage Strategy

## 目的

現在のComponent Storageを、単一方式へ統一するのではなく、Componentごとのアクセス特性とメモリ特性に応じて次の3方式から選択できる構造へ再整理する。

- `SparseComponentStorage<T>`: メモリ効率優先
- `ChunkedDenseComponentPool<T>`: 連続走査・バッチ処理優先
- `DirectPagedComponentStorage<T>`: Entity単体アクセス・位置安定性優先

本Stepは、従来`COMPONENT_ARCHETYPE`と呼んでいたStorageの意図を明確化し、一般的なArchetype ECSとの名称衝突を解消する。

---

## 背景

現在のComponent Registryは、Component型ごとに次のどちらかを選択している。

```text
SparseStorage<T>
DenseComponentPool<T>
```

現在の`COMPONENT_ARCHETYPE`は、一般的なArchetype ECSではなく`DenseComponentPool<T>`の別名として使われている。

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

現在の`DenseComponentPool<T>`は、存在するComponentだけを連続配置し、Entity IDからSparse Indexを経由してDense Indexへ変換する方式である。

```text
Entity ID
    ↓ Sparse Index
Dense Index
    ↓
Component Array
```

これは全件連続走査には強いが、元の「メモリを多く使う代わりにEntity IDから直接アクセスするStorage」とは役割が異なる。

したがって、Dense Poolを削除するのではなく、3種類を明確に分離する。

---

# 1. Storage分類

## 1.1 SparseComponentStorage

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

## 1.2 ChunkedDenseComponentPool

### 目的

存在するComponentだけを隙間なく配置し、全件走査、バッチ生成、SIMD、Worker分割を優先する。

### 基本構造

```cpp
template<typename T>
class ChunkedDenseComponentPool
{
    std::vector<std::unique_ptr<DenseChunk<T>>> chunks;
    std::vector<ComponentLocation> locations;
};
```

各Chunkは次を持つ。

```cpp
template<typename T, size_t Capacity>
struct DenseChunk
{
    std::array<T, Capacity> components;
    std::array<Entity, Capacity> entities;
    uint32_t count = 0;
};
```

### 拡張

Chunkが満杯になった場合、既存Component全体を再配置せず、新しいChunkを追加する。

```text
Chunk 0: Full
Chunk 1: Full
Chunk 2: Active
        ↓ Capacity到達
Chunk 3を新規確保
```

### 強み

- 存在するComponent本体が連続する
- 全件走査のCache効率が高い
- Render Packet BuildやCullingのDriver列に適する
- Chunk単位で`ParallelFor`へ分割できる
- 単一巨大`std::vector`の再確保を避けられる
- 新規Chunk追加時に既存Chunk内Componentが移動しない

### 弱み

- Entity単体取得には`Entity -> chunk + slot`のLocation変換が必要
- Swap Remove時に移動したComponentのLocation更新が必要
- Componentポインタは削除・圧縮で無効化され得る
- 複数Dense Pool間でDense Indexは一致しない

### 削除方式

基本はChunk内または末尾ChunkからのSwap Removeとする。

```text
[A][B][C][D]
    B削除
[A][D][C]
```

移動したEntityのLocationを更新する。

### 適用条件

- 毎フレーム大量に全件走査する
- Batch / Packet / Proxy生成のDriverになる
- Component移動を許容できる
- PointerをFrameやPhaseを跨いで保持しない
- Job SystemでChunk分割したい

---

## 1.3 DirectPagedComponentStorage

### 目的

Entity IDからComponentへ直接到達し、単体アクセス、Pointer位置安定性、固定ページ拡張を優先する。

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

取得はHash検索やDense Index変換を行わない。

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
- 穴が多い場合、全Slot走査は効率が悪い
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

ただし、Entity IDからPageを直接算出する契約では型ごとにPageCapacityが異なる点を許容する。

### 適用条件

- 多くのEntityが所有する
- 小型・固定サイズである
- Entity単体から非常に頻繁に取得される
- Component位置を安定させたい
- 未使用Slot分のメモリを許容できる

---

# 2. 三方式の比較

| 特性 | Sparse | Chunked Dense | DirectPaged |
|---|---|---|---|
| Component実体確保 | 所有Entityのみ | 所有Entityのみ | Page全Slot分の生領域 |
| Entity単体取得 | Hash検索 | Location変換 | Page + Slot直接計算 |
| 全件走査 | 弱い | 最も強い | Occupancy次第 |
| Pointer安定性 | Node実装依存 | Swap Removeで移動 | Page破棄まで高い |
| 追加時の全体移動 | Rehash可能性 | なし | なし |
| 削除時の他要素移動 | なし | あり | なし |
| 低占有率 | 強い | 強い | 弱い |
| 大型Component | 強い | 条件付き | 弱い |
| Chunk並列化 | 弱い | 強い | 強い |
| Streaming境界 | 別Indexが必要 | Chunkへ付与可能 | Page外側へCellが必要 |

---

# 3. Query契約

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
ModelRenderer / RenderProxy DenseをDriverとして連続走査
    ↓ Entity ID
Transform DirectPagedへ直接アクセス
Material DirectPagedへ直接アクセス
Texture SparseへHashアクセス
```

### Driver選択の基本優先度

1. `ChunkedDenseComponentPool`のうち最小要素数
2. 高占有`DirectPagedComponentStorage`の有効Page
3. `SparseComponentStorage`のEntity列
4. 全Alive Entity走査はFallbackに限定

### Storage参照解決

`GetComponent<T>()`ごとに`m_storages.find(typeid(T))`を行わず、Query生成時またはSystem Task開始時にStorage Pointerを一度だけ解決する。

### 構造変更

- Query実行中にStorage構造を変更しない
- Add / RemoveはEntityCommandBufferへ記録する
- Playback後にStructure Versionを更新する
- Query ViewはStructure Version不一致時に無効化する

---

# 4. 現在のComponent暫定分類

## 4.1 DirectPaged候補

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

## 4.2 Chunked Dense候補

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

Cullingで連続走査し、Chunk単位でWorkerへ分割する。

### VisibilityComponent

将来追加時はDense。

Culling結果とPacket Build入力として大量処理する。

### Animation Pose Result

Frame-local Runtime結果としてDense。

永続Animation設定とGPU / CPU Runtime結果を分離する。

### Particle Runtime Pool

粒子実体はEmitter Componentから分離し、専用Dense Runtime Poolへ置く。

---

## 4.3 Sparse候補

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

# 5. 暫定登録表

```cpp
enum class ComponentStorageKind
{
    Sparse,
    ChunkedDense,
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
    X(LightComponent,               ChunkedDense) \
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

# 6. Streamingとの関係

単一Dense Poolは、Streaming Cell単位でComponentが混在し、Cell単位解放と空間局所性が弱い。

Chunk化後は、Dense ChunkへStreaming Group / Cell IDを付与できる。

```text
ComponentStorage<T>
 ├─ Persistent Group
 │   ├─ Chunk 0
 │   └─ Chunk 1
 ├─ Streaming Cell A
 │   ├─ Chunk 0
 │   └─ Chunk 1
 └─ Streaming Cell B
     └─ Chunk 0
```

ただし本Stepでは、Storage方式の基盤整備を先に行い、Streaming Cell統合は別Stepとする。

### Streaming統合時の基本契約

- 外側の単位はStreaming Cell / Region
- 内側はComponent Storage方式
- ChunkまたはPageへCell所属を付与する
- Cell Unload時は個別Entity RemoveよりBlock単位破棄を優先する
- Entity IDと空間的位置が一致するとは仮定しない

---

# 7. 実装手順

## Phase 1: 名称と登録契約の整理

- [ ] `COMPONENT_ARCHETYPE`を廃止する
- [ ] `ComponentStorageKind`を`Sparse / ChunkedDense / DirectPaged`へ変更する
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

## Phase 3: Dense PoolのChunk化

- [ ] 単一`std::vector<T>`を固定容量Chunk列へ置換する
- [ ] Chunk容量を目標Byte数から算出する
- [ ] `Entity -> Chunk + Slot` Locationを実装する
- [ ] Chunk追加で既存Componentが移動しないことを確認する
- [ ] Chunk内Swap Remove
- [ ] 移動EntityのLocation更新
- [ ] 空末尾Chunkの解放
- [ ] Chunk Span / Iteration API
- [ ] Chunk単位`ParallelFor`対応

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
- [ ] LightをChunked Denseへ維持・移行
- [ ] ParticleをSparseへ変更
- [ ] Particle Runtime PoolをDenseへ分離
- [ ] Renderer設定ComponentをSparse維持
- [ ] RenderProxy / RenderPacket入力をDense Driver化
- [ ] Camera / EnvironmentMap / FollowをSparseへ変更

## Phase 6: Streaming拡張準備

- [ ] Dense Chunk / Direct PageへStorage Group IDを付与可能にする
- [ ] Persistent GroupとStreaming Groupを区別する
- [ ] Block単位破棄APIを設計する
- [ ] Active CellだけをQuery対象にするFilter契約を設計する

---

# 8. 計測項目

Storage選択は推測だけで確定せず、次をComponent型ごとに計測する。

- Component所有Entity数
- 全Alive Entityに対する占有率
- `GetComponent<T>()`呼び出し回数
- 全件走査回数
- Add / Remove回数
- Component平均・最大サイズ
- Query Join回数
- Pointer保持期間
- Dense Chunk占有率
- Direct Page占有率
- Sparse Node / Bucket推定メモリ
- Frame中のStorage再配置時間

### 比較条件

同じScene、同じEntity数、同じView、同じFrame範囲で比較する。

### 最低比較対象

- Transform取得
- Render Packet Build
- Physics Transform同期
- Editor Inspector選択
- Hierarchy更新
- Light列挙
- Particle更新

---

# 9. 検証項目

## Unit / Smoke

- [ ] Sparse Add / Get / Remove
- [ ] Chunked Dense Add / Get / Swap Remove
- [ ] DirectPaged Add / Get / Remove
- [ ] Entity generation不一致拒否
- [ ] Component generation不一致拒否
- [ ] Free Entity Index再利用
- [ ] Page追加後の既存Pointer維持
- [ ] Dense Chunk追加後の既存Chunk Pointer維持
- [ ] DirectPaged Occupancy Iteration
- [ ] Dense Chunk Iteration
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
- [ ] Component追加時Spike比較
- [ ] Component削除時Spike比較
- [ ] Scene Load Peak Memory比較

---

# 10. 完了条件

- ComponentごとにStorage Kindを明示できる
- `Sparse / ChunkedDense / DirectPaged`の役割が重複せず説明できる
- System / Query利用側がStorage種別へ直接依存しない
- Queryが全Alive Entity走査を標準経路にしない
- TransformとMaterialのEntity単体取得がHash検索を行わない
- Dense Driver列がChunk単位で並列走査できる
- Script / Resource所有ComponentのPointer安定性が維持される
- Entity Free List再利用とDirect Page Slot再利用が整合する
- Scene / Prefab / YAML / Inspectorに回帰がない
- Storage選択前後のCPU時間とPeak Memoryを比較できる

---

# 11. 実装優先順位

本変更は広範囲なため、現在進行中のFrame Pacing計測契約修正を完了後に着手する。

推奨順:

1. GPU / CPU Frame Serial対応を完了
2. Step 11-B Storage Strategyの名称・登録契約だけ先行
3. DirectPagedのUnit Test基盤
4. Chunked DenseのUnit Test基盤
5. Query PlannerのStorage駆動化
6. Material / Layer系をDirectPagedへ移行
7. Transform Hierarchy分離
8. TransformをDirectPagedへ移行
9. Particle登録修正とRuntime Pool分離
10. Render Proxy / Packet DriverのDense Chunk化
11. Streaming Cell統合を別Stepで開始

実装中は既存`SparseStorage`と`DenseComponentPool`を互換経路として維持し、Component型ごとに段階移行する。一括置換は行わない。
