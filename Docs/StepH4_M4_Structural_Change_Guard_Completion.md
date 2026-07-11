# H4 / M-4 Structural Change Guard Completion

## 状態

**コード実装完了・VSビルド確認待ち（2026-07-10）**

`Docs/ECS_Scheduler_Migration_Plan.md` §2.5で残っていた次の2件を実装した。

- H4残: 即時構造変更APIの内部専用化
- M-4残: `AddComponent`戻り値の`ComponentRef<T>`限定化

## 設計判断

### M-4: 戻り値のComponentRef限定

`ComponentRegistry::AddComponent`の戻り生`T*`は、同型の後続追加によるStorage
再確保 / rehashで無効化しうる（§1.2）。公開APIから生ポインタ返却を排除し、
世代検証付き`ComponentRef<T>`へ限定した。

- 公開`AddComponent` / `ReplaceComponent` / `SetComponent`は`ComponentRef<T>`を返す
- 生`T*`返却の実体は`private: AddComponentRaw`へ隔離
  - 使用箇所はRegistry内部のYAML Factory / Editor用Adder / RuntimeTypeName Adderのみ
- 一時的な生ポインタが必要な呼出側は戻り値の`TryGet()`を明示的に使用する

`componentRegistry.h`は`ComponentRef<T>`を前方宣言し、ヘッダ末尾で
`Reference/ComponentRef.h`をincludeする。テンプレートの実体化は利用側で
行われるため、両方向のinclude順序で完全型が保証される（循環はpragma onceで解決）。

### H4: 即時構造変更の内部専用化とDebug強制

アクセス制御だけではEditor（Inspector / Undo / Redo）とScene Loadの正当な
即時変更をfriend列挙で登録する必要があり、RegistryがEditor型へ結合してしまう。
そこでAPI隔離（上記AddComponentRaw私有化）に加えて、実行時Debug検出を採用した。

新設`Registry/StructuralChangeGuard.h`:

- `SystemRegistry::ExecuteTasks`がEditor以外のDomain（Fixed / Frame / Render）
  実行中に`ScopedScheduleLock`を立てる
- Lock中の`AddComponentRaw` / `RemoveComponent` / `RemoveComponentByID` /
  `OnEntityDestroyed` / `RegisterComponent`はDebug assertで停止する
- `EntityCommandBuffer::Commit`のPlayback区間だけ`ScopedPlaybackUnlock`
  （thread_local）で適用を許可する
- Editor DomainはMainThreadの編集操作による即時構造変更を許容するためLock対象外

これによりSystem / ScriptはSchedule実行中、`EntityCommandBuffer`経由の
遅延変更へ強制される。`CustomScriptComponent::AddComponent`（即時API）は
互換のため残るが、Update / Fixed / Draw中の使用はDebugで検出される。

## 安全性の確認

- `SceneManager::LoadScene`は`SceneManager::Update`末尾の遅延切替ブロックで
  実行され、`ExecuteTasks`のScope外のためassertは発火しない
  （`DeferredLoadScene`はフラグ設定のみ）
- `Scene::Initialize`はresolver / contextID設定後に`BuildDefaultScene`を
  呼ぶため、生成される`ComponentRef`は有効
- Engine側Script（`Scene/Script/*.h`）に即時`AddComponent`使用は無し
- `CustomScriptSystem` / `EntityCommandCommitSystem`のCommitはPlayback
  Unlockで許可される

## 変更ファイル

- `Source/GameApplication/Engine/Scene/Registry/StructuralChangeGuard.h`（新設）
- `Source/GameApplication/Engine/Scene/Registry/componentRegistry.h`
- `Source/GameApplication/Engine/Scene/Registry/systemRegistry.h`
- `Source/GameApplication/Engine/Scene/Command/EntityCommandBuffer.h`
- `Source/GameApplication/Engine/Scene/Component/CustomScriptComponent.h`
- `Source/GameApplication/Engine/Scene/scene.cpp`（呼出側25箇所を`ComponentRef`受けへ）
- `Source/GameApplication/Engine/Scene/Prefab/PrefabSystem.cpp`（`TryGet()`化 2箇所）
- `Tests/`配下11ファイル（`TryGet()` / `ComponentRef`受けへ移行。
  あわせてテスト用`SceneContext`へ自己解決resolverを配線。従来の
  `resolver == nullptr`のままでは`ComponentRef`が常に無効になるため）

## 完了条件

- [x] 公開`AddComponent` / `ReplaceComponent` / `SetComponent`が生`T*`を返さない
- [x] 生ポインタ返却経路がRegistry内部専用（private）である
- [x] Schedule実行中（Editor Domain除く）の即時構造変更をDebug assertで検出する
- [x] `EntityCommandBuffer::Commit`のPlaybackは検出対象外である
- [x] 呼出側（Engine / Editor / Tests）を新契約へ移行した
- [ ] Windows Debug x64 Build
- [ ] Windows Release x64 Build
- [ ] Tests配下Smoke Test実行
- [ ] Debug実機でPlay / Stop / Inspector Add・Remove / Prefab操作の回帰確認
  （特にassert誤発火が無いこと）

## 補足

- `.git/index.lock`が残留している場合がある（本作業中のsandbox側git操作の残骸）。
  Windows側で`git add`等が失敗する場合は手動削除すること。
- Query反復中の構造変更検出（H4前半のStructure Version assert）と本ガードは
  補完関係にある。前者はイテレータ無効化、後者はSchedule実行中の並行変更を対象とする。
