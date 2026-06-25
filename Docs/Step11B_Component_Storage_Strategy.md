# Step 11-B Component Storage Strategy

## 目的

Componentの性質に応じてStorage実装を選択できる共通契約を導入する。

EntityはComponentを一つも持たない状態を許容し、Entity本体へ状態Flagを追加しない。

`Disabled`、`Static`、`Hidden`などの状態はTag Componentの存在で表現する。

---

## Storage Strategy

```cpp
enum class ComponentStorageStrategy : uint8_t {
    Dense,
    DirectPaged,
    SparseStable,
    Archetype
};
```

### Dense

連続走査を最優先する通常Component向け。

- ModelRenderer
- Collider
- CullingComponent
- Animation状態

`DenseComponentPool<T>`を使用する。

### DirectPaged

Entity indexからPage / Slotを直接計算するStorage。

DirectPagedは次の2種類を持つ。

```text
DirectPagedComponentStorage<T>
    実データをPage内Slotへ保持する

DirectPagedTagStorage<T>
    Component実体を持たずPresenceだけを保持する
```

用途:

- TransformComponent
- DisabledComponent
- StaticEntityComponent
- HiddenComponent
- EditorOnlyComponent
- DontDestroyComponent

```text
pageIndex = entity.index / PageSize
slotIndex = entity.index % PageSize
```

Transformは値を持つため`DirectPagedComponentStorage<TransformComponent>`を使用する。

Tagは値を保持せず、Presence BitとEntity generationだけを保持する。

### SparseStable

低頻度、大型、ポインタ安定性が必要なComponent向け。

既存`SparseStorage<T>`を移行する。

### Archetype

将来のArchetype Chunk Storage用。

現段階では実装しないが、`IComponentStorage`と登録契約を変更せず追加できる形にする。

---

## DirectPaged Data Storage

```cpp
template<typename T, uint32_t PageSize = 256>
class DirectPagedComponentStorage final : public IComponentStorage {
public:
    void Add(Entity entity, T component);
    void Remove(Entity entity) override;
    bool Contains(Entity entity) const override;
    T* Get(Entity entity);
    const T* Get(Entity entity) const;

private:
    struct Slot {
        alignas(T) std::byte storage[sizeof(T)];
        uint32_t entityGeneration = 0;
        uint32_t componentGeneration = 0;
        bool occupied = false;
    };

    struct Page {
        std::array<Slot, PageSize> slots;
        std::bitset<PageSize> occupied;
    };

    std::vector<std::unique_ptr<Page>> pages;
};
```

### Transform移行

`TransformComponent`はDirectPagedへ移行する。

```cpp
RegisterComponent<TransformComponent>(
    ComponentStorageStrategy::DirectPaged
);
```

理由:

- Entity indexからTransformへ直接到達できる
- Transform参照の取得経路でSparse Index検索を省ける
- Swap RemoveによるTransformアドレス移動を避けられる
- Parent / ChildやEntity参照との対応が単純になる
- Entity index再利用時のgeneration検証をSlot単位で行える
- Transformを持たないEntityもPage未占有Slotとして自然に表現できる

注意点:

- DirectPagedは全Transformの完全連続配列ではない
- TransformSystemの全件走査ではPage内Occupied Bitを使う
- Page間は非連続なため、Dense Span前提APIを使用しない
- Transform追加・削除で既存Transformのアドレスを移動しない
- Pageを跨ぐParallelFor単位を用意する
- 空Pageの解放は参照安定性と計測結果を見て判断する

### Transform走査契約

```text
for each allocated page
    for each occupied bit
        TransformComponentを処理
```

TransformSystem向けに型消去されたPage ViewまたはTyped Page Viewを提供する。

```cpp
template<typename T>
struct DirectPagedPageView {
    std::span<T> slots;
    std::span<const uint64_t> occupancyWords;
    uint32_t firstEntityIndex;
};
```

`GetDenseComponentSpan<TransformComponent>()`への依存は除去し、DirectPaged用の走査APIへ移行する。

---

## DirectPaged Tag Storage

```cpp
template<typename T, uint32_t PageSize = 256>
class DirectPagedTagStorage final : public IComponentStorage {
public:
    void Add(Entity entity);
    void Remove(Entity entity) override;
    bool Contains(Entity entity) const override;

private:
    struct Page {
        std::bitset<PageSize> occupied;
        std::array<uint32_t, PageSize> entityGenerations{};
        std::array<uint32_t, PageSize> componentGenerations{};
    };

    std::vector<std::unique_ptr<Page>> pages;
};
```

### 保持データ

Tag Componentの実体Objectは保持しない。

各Slotは次だけを保持する。

- Presence Bit
- Entity generation
- Component generation

`GetRaw()`との互換が必要な間は、Storage内の型共有Dummy Objectを返す。

ただしDummy Objectへの書き込みは禁止する。

将来的にはTagを`ComponentView`のData Pointerなしで表現できるよう、View契約を拡張する。

---

## CullingComponentの扱い

`CullingComponent`は存在がCulling有効を表すが、内部にLocal / World AABBとRevisionを持つ。

したがってTag Storageにはしない。

```cpp
struct CullingComponent : IComponent {
    AABB localBounds;
    AABB worldBounds;
    uint64_t sourceRevision;
    uint64_t transformRevision;
    bool boundsValid;
};
```

初期戦略は`Dense`とする。

理由:

- CullingSystemが対象Entityを毎FrameまたはRevision更新時に連続走査する
- Bounds更新とFrustum判定でデータ局所性が重要
- Entity index直接検索より一括走査性能を優先する

存在による機能宣言とStorage方式は分離して考える。

---

## 登録契約

現在の`bool useDensePool`を廃止する。

```cpp
RegisterComponent<TransformComponent>(
    ComponentStorageStrategy::DirectPaged
);

RegisterComponent<StaticEntityComponent>(
    ComponentStorageStrategy::DirectPaged
);

RegisterComponent<CullingComponent>(
    ComponentStorageStrategy::Dense
);
```

`DirectPaged`選択時は、Tag TraitによりData StorageとTag Storageを切り替える。

```cpp
template<typename T>
inline constexpr bool IsTagComponent = false;

template<>
inline constexpr bool IsTagComponent<StaticEntityComponent> = true;
```

YAML登録も同じStrategyを受け取る。

```cpp
RegisterYAMLComponent<T>(name, strategy);
```

Tag Componentは通常Component一覧へ出さず、Entity共通ヘッダーからAttach / Detachする。

---

## IComponentStorage契約

既存の共通基底を維持する。

```cpp
struct IComponentStorage {
    virtual void Remove(Entity entity) = 0;
    virtual bool Contains(Entity entity) const = 0;
    virtual void* GetRaw(Entity entity) = 0;
    virtual std::vector<Entity> GetEntityList() const = 0;
    virtual size_t Size() const noexcept = 0;
    virtual uint32_t GetComponentGeneration(Entity entity) const = 0;
    virtual uint64_t GetStructureVersion() const noexcept = 0;
};
```

RegistryやEntityCommandBufferは具体Storage型を判定しない。

`AddComponent`内の`dynamic_cast<DenseComponentPool<T>*>`分岐を廃止し、型消去されたAdd操作をStorage契約へ追加するか、登録時Factoryへ委譲する。

---

## DirectPaged設計条件

- Pageは必要になった時だけ確保する
- Entity index最大値まで連続配列を確保しない
- 古いEntity generationをContainsで拒否する
- Remove / ReattachでComponent generationを進める
- 未割当PageへのContainsはfalse
- 空Pageの解放は初期実装では行わない
- PageSizeは計測後に128 / 256 / 512から決定する
- Tag列挙が必要な場合はOccupied Bitを走査する
- Data Component列挙もOccupied Bitを走査する
- Hash MapをPresence判定の主要経路に使用しない
- Slot内ComponentのアドレスはPage寿命中安定させる
- Page配列の再配置でComponent本体を移動させない

---

## 実装手順

### Step 11-B.1 Strategy契約

- [ ] `ComponentStorageStrategy`
- [ ] 登録APIから`bool useDensePool`を除去
- [ ] Storage Factory
- [ ] Tag Trait
- [ ] Registryから具体Storageへの`dynamic_cast`追加分岐を除去

### Step 11-B.2 DirectPaged Data Storage

- [ ] Page / Slot計算
- [ ] Lazy Page Allocation
- [ ] Placement Construct / Destroy
- [ ] Entity generation検証
- [ ] Component generation
- [ ] Structure Version
- [ ] Occupied Bit列挙
- [ ] Typed Page View
- [ ] Page単位ParallelFor

### Step 11-B.3 Transform DirectPaged移行

- [ ] Transform登録StrategyをDirectPagedへ変更
- [ ] Add / Get / Remove移行
- [ ] Parent / Child参照回帰
- [ ] World Matrix更新走査のPage View対応
- [ ] `GetDenseComponentSpan<TransformComponent>()`依存除去
- [ ] Transform参照アドレス安定性確認
- [ ] Entity index再利用回帰
- [ ] Scene Save / Load回帰

### Step 11-B.4 DirectPaged Tag Storage

- [ ] Page / Slot計算
- [ ] Lazy Page Allocation
- [ ] Presence Bit
- [ ] Entity generation検証
- [ ] Component generation
- [ ] Structure Version
- [ ] Entity列挙

### Step 11-B.5 Tag Component

- [ ] `DisabledComponent`
- [ ] `StaticEntityComponent`
- [ ] `HiddenComponent`
- [ ] Entity共通ヘッダーとのAttach / Detach接続
- [ ] YAML Presence保存
- [ ] Undo / Redo

### Step 11-B.6 検証

- [ ] ComponentなしEntity
- [ ] 高いEntity indexでのLazy Allocation
- [ ] Entity index再利用とgeneration拒否
- [ ] Remove / Reattach
- [ ] Dense / DirectPaged混在Query
- [ ] Transform / Tag DirectPaged混在
- [ ] EntityCommandBuffer経由Attach / Detach
- [ ] Transform大量生成時のDense比較計測
- [ ] Transform全件走査のDense比較計測
- [ ] Debug / Release x64

---

## 完了条件

- TransformComponentがDirectPagedで保存される
- Tag ComponentがDirectPagedで保存される
- ComponentなしEntityが正常に保存・読込・破棄できる
- Registryが具体Storage型を知らずにAdd / Remove / Queryできる
- Transform取得とTag ContainsがHash探索を行わない
- Transform追加・削除で既存Transformのアドレスが移動しない
- Entity index再利用後に古いTransform / Tag参照を受理しない
- TransformSystemがPage単位走査できる
- Dense Componentの連続走査性能を悪化させない
- 将来Archetype Storageを同じ登録契約へ追加できる
