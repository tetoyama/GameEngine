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

連続走査を優先する通常Component向け。

- Transform
- ModelRenderer
- Collider
- CullingComponent
- Animation状態

`DenseComponentPool<T>`を使用する。

### DirectPaged

Entity indexからPage / Slotを直接計算するStorage。

Tag Componentは原則としてDirectPagedを使用する。

- DisabledComponent
- StaticEntityComponent
- HiddenComponent
- EditorOnlyComponent
- DontDestroyComponent

```text
pageIndex = entity.index / PageSize
slotIndex = entity.index % PageSize
```

Tagは値を保持せず、Presence BitとEntity generationだけを保持する。

### SparseStable

低頻度、大型、ポインタ安定性が必要なComponent向け。

既存`SparseStorage<T>`を移行する。

### Archetype

将来のArchetype Chunk Storage用。

現段階では実装しないが、`IComponentStorage`と登録契約を変更せず追加できる形にする。

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
    ComponentStorageStrategy::Dense
);

RegisterComponent<StaticEntityComponent>(
    ComponentStorageStrategy::DirectPaged
);

RegisterComponent<CullingComponent>(
    ComponentStorageStrategy::Dense
);
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
- Hash MapをPresence判定の主要経路に使用しない

---

## 実装手順

### Step 11-B.1 Strategy契約

- [ ] `ComponentStorageStrategy`
- [ ] 登録APIから`bool useDensePool`を除去
- [ ] Storage Factory
- [ ] Registryから具体Storageへの`dynamic_cast`追加分岐を除去

### Step 11-B.2 DirectPaged Tag Storage

- [ ] Page / Slot計算
- [ ] Lazy Page Allocation
- [ ] Presence Bit
- [ ] Entity generation検証
- [ ] Component generation
- [ ] Structure Version
- [ ] Entity列挙

### Step 11-B.3 Tag Component

- [ ] `DisabledComponent`
- [ ] `StaticEntityComponent`
- [ ] `HiddenComponent`
- [ ] Entity共通ヘッダーとのAttach / Detach接続
- [ ] YAML Presence保存
- [ ] Undo / Redo

### Step 11-B.4 検証

- [ ] ComponentなしEntity
- [ ] 高いEntity indexでのLazy Allocation
- [ ] Entity index再利用とgeneration拒否
- [ ] Remove / Reattach
- [ ] Dense / DirectPaged混在Query
- [ ] EntityCommandBuffer経由Attach / Detach
- [ ] Debug / Release x64

---

## 完了条件

- Tag ComponentがDirectPagedで保存される
- ComponentなしEntityが正常に保存・読込・破棄できる
- Registryが具体Storage型を知らずにAdd / Remove / Queryできる
- TagのContainsがHash探索を行わない
- Entity index再利用後に古いTag参照を受理しない
- Dense Componentの連続走査性能を悪化させない
- 将来Archetype Storageを同じ登録契約へ追加できる
