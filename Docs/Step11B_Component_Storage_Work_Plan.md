# Step 11-B Component Storage Work Plan

## 目的

Componentごとのアクセス特性に応じて、次の3種類のStorageを正式に使い分ける。

- `SparseComponentStorage<T>`
- `DenseComponentPool<T>`
- `DirectPagedComponentStorage<T>`

詳細設計は `Docs/Step11_Component_Storage_Strategy.md` を参照する。

---

## 1. Storageの役割

### SparseComponentStorage

メモリ効率を優先する。

適用対象:

- 所有Entityが少ないComponent
- 大型または可変長Component
- Script、Editor、Resource参照を持つComponent
- 全件走査より個別アクセスが中心のComponent

特徴:

- 所有Entity分だけ実体を確保する
- Entity IDが疎でも無駄な実体領域を持たない
- `GetComponent`はHash検索を伴う
- 連続走査には不向き

### DenseComponentPool

全件連続走査とBatch処理を優先する。

適用対象:

- 毎Frame大量に走査するComponent
- Render Packet、Culling、VisibilityなどのDriver列
- Range単位でJob Systemへ分割するComponent
- Component移動を許容できるRuntime Data

特徴:

- 存在するComponentだけを連続配置する
- Sparse IndexからDense Indexへ変換する
- Swap Removeを使用する
- `std::vector`の再確保とSwap RemoveでPointerが移動する
- Chunked Denseは導入せず、通常Denseを維持する
- 再確保対策は`reserve()`と事前個数集計で行う

### DirectPagedComponentStorage

Entity IDからの直接アクセスと位置安定性を優先する。

適用対象:

- 多くのEntityが所有する小型・固定長Component
- Entity指定で頻繁に取得されるComponent
- 他Entity削除によるComponent移動を避けたいComponent

特徴:

```text
Entity Index
    ↓
Page Index + Slot Index
    ↓
Component
```

- Entity IDからPageとSlotを直接計算する
- Hash検索とSparse Index変換を行わない
- Pageは必要になった時だけ確保する
- 未使用Slotは生領域だけを持ち、Componentを構築しない
- Occupancy Bitで存在判定とPage走査を行う
- Entity RegistryのFree ListでEntity Indexを再利用する

---

## 2. Component暫定分類

### DirectPaged候補

- `TransformComponent`
  - Hierarchyの`children`配列を専用Storageへ分離した後に移行
- `MaterialComponent`
- `RenderLayerComponent`
- `OrderInLayerComponent`
- `NameComponent`
  - 全Entity所有を標準契約とする場合

### Dense候補

- `LightComponent`
- `RenderProxy` / Render Packet Build用の軽量入力列
- `WorldBoundsComponent`
- `VisibilityComponent`
- Animation Pose Result
- Particle Runtime Pool

大型の永続Renderer Componentを直接Dense化せず、処理に必要な軽量Runtime DataをDenseへ抽出する。

### Sparse候補

- `ModelRendererComponent`
- `MeshRendererComponent`
- `SpriteRendererComponent`
- `BillBoardRendererComponent`
- `TerrainComponent`
- `WaveComponent`
- `EffectComponent`
- `ColliderComponent`
- `ParticleComponent`
- `CameraComponent`
- `EnvironmentMapComponent`
- `FollowComponent`
- `ScriptComponent`
- `CustomScriptComponent`
- `PrefabComponent`

---

## 3. Query契約

SystemはStorage種別へ直接依存しない。

```cpp
auto query = registry.Query<
    Read<TransformComponent>,
    Read<ModelRendererComponent>,
    Write<VisibilityComponent>
>();
```

Registry内部でDriverとJoin方法を決定する。

優先順位:

1. 最小要素数のDense StorageをDriverにする
2. DirectPagedをEntity IDから直接Joinする
3. SparseをHash LookupでJoinする
4. 全Alive Entity走査はFallbackに限定する

`GetComponent<T>()`ごとの`type_index`検索を避け、Query生成時またはTask開始時にStorage Pointerを一度だけ解決する。

---

## 4. 実装手順

### Phase 1: 登録契約

- [ ] `ComponentStorageKind`を追加する

```cpp
enum class ComponentStorageKind
{
    Sparse,
    Dense,
    DirectPaged
};
```

- [ ] `RegisterComponent<T>(bool useDensePool)`をStorage Kind指定へ変更
- [ ] `RegisterYAMLComponent<T>(bool useDensePool)`をStorage Kind指定へ変更
- [ ] `COMPONENT_ARCHETYPE`名称を廃止
- [ ] 旧`ArchetypeStorage` aliasを廃止
- [ ] 既存Scene / YAML登録との互換経路を維持

### Phase 2: DirectPaged基盤

- [ ] `DirectPagedComponentStorage<T>`を追加
- [ ] Raw Storageとplacement construction
- [ ] Occupancy Bit
- [ ] Entity IndexからPage + Slotを計算
- [ ] PageのLazy Allocation
- [ ] Remove時のdestructorとgeneration更新
- [ ] Page追加後のPointer安定性Test
- [ ] Occupancy Word単位Iteration

### Phase 3: Dense整備

- [ ] `Reserve<T>(count)` API
- [ ] `Size<T>()` / `Capacity<T>()` API
- [ ] Scene / Prefab Load前の所有数集計
- [ ] Dense SpanのIndex Range分割
- [ ] Range単位`ParallelFor`
- [ ] 再確保回数と時間の計測
- [ ] Swap Remove回数と時間の計測
- [ ] Structure Version変更時のPointer無効化契約

### Phase 4: Query Planner

- [ ] Storage種別を隠蔽する共通Iteration契約
- [ ] Dense最小列をDriverに選択
- [ ] DirectPaged Join
- [ ] Sparse Join
- [ ] Storage Pointer Cache
- [ ] Structure Version検証
- [ ] Query中の構造変更禁止
- [ ] EntityCommandBuffer Playback後のView無効化

### Phase 5: Component移行

- [ ] MaterialをDirectPagedへ移行
- [ ] RenderLayerをDirectPagedへ移行
- [ ] OrderInLayerをDirectPagedへ移行
- [ ] TransformからHierarchy childrenを分離
- [ ] TransformをDirectPagedへ移行
- [ ] LightをDenseで維持
- [ ] ParticleComponentをSparseへ変更
- [ ] Particle Runtime PoolをDenseへ分離
- [ ] Renderer設定ComponentをSparseで維持
- [ ] RenderProxy / Packet DriverでDenseを使用
- [ ] Camera / EnvironmentMap / FollowをSparseへ変更

### Phase 6: Streaming準備

- [ ] EntityのStreaming Group ID契約
- [ ] Active Group Query Filter
- [ ] Cell単位Entity破棄Command
- [ ] Persistent GroupとStreaming Groupの分離
- [ ] Block単位解放が必要になる条件を定義

Dense Storage自体はChunk化しない。Streaming境界はEntity Group、Query Filter、必要に応じたWorld / Registry分割で扱う。

---

## 5. 実施位置

現在のStop / Material Alpha / RectTransform回帰修正と、Step 17のRender Packet / Frame Pacing関連を安定化した後、Step 18 RenderWorld基盤へ進む前に実施する。

実装順:

1. Stop → TempLoad → Effect再描画の実機確認
2. Material Alpha半透明描画の実機確認
3. RectTransform正規化契約の実機確認
4. Storage登録契約の変更
5. DirectPaged基盤とUnit Test
6. Dense Reserve / Range Parallel API
7. Storage駆動Query Planner
8. Material / Layer系の移行
9. Transform Hierarchy分離とDirectPaged移行
10. Particle / RenderProxy Runtime Data分離
11. Step 18 RenderWorld基盤

---

## 6. 計測項目

Component型ごとに次を計測する。

- 所有Entity数
- 全Alive Entityに対する占有率
- `GetComponent<T>()`回数
- 全件走査回数
- Add / Remove回数
- Component Size
- Query Join回数
- Dense `size / capacity`
- Dense再確保回数と時間
- Direct Page占有率
- Sparse Node / Bucket推定メモリ

最低比較対象:

- Transformランダムアクセス
- Render Packet Build
- Physics Transform同期
- Inspector選択
- Hierarchy更新
- Light列挙
- Particle更新

---

## 7. 完了条件

- Componentごとに`Sparse / Dense / DirectPaged`を明示できる
- `COMPONENT_ARCHETYPE`名称が残っていない
- SystemとQueryがStorage実装種別へ直接依存しない
- Queryが全Alive Entity走査を標準経路にしない
- TransformとMaterialの取得がHash検索を行わない
- DenseをIndex Range単位で並列走査できる
- Dense再確保を`reserve()`で制御できる
- DirectPagedのPage追加で既存Component Pointerが移動しない
- Entity Free ListとDirectPaged Slot再利用が整合する
- Scene / Prefab / YAML / Inspector / Play / Stopに回帰がない
- Storage選択前後のCPU時間とPeak Memoryを比較できる

既存`SparseStorage`と`DenseComponentPool`は移行中も互換経路として維持し、Component型ごとに段階移行する。一括置換は行わない。
