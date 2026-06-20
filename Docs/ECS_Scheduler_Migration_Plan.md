# ECS / Scheduler Migration Plan

## 目的

現在の疑似ECSを、安全な参照・自動依存解析・将来のマルチスレッド実行に対応した構造へ段階的に移行する。

---

## Schedulerの確定仕様

### 実行単位

Schedulerが扱う単位は `SystemTask` とする。

各Taskは次の情報を持つ。

```cpp
struct SystemTaskDescriptor
{
    SystemPhase phase;
    int32_t priority;
    uint64_t registrationOrder;

    SystemAccess access;
    ThreadAffinity threadAffinity;
};
```

### 論理順序

Task間の論理順序は次で決める。

1. `Phase`
2. `Priority`
3. `RegistrationOrder`

ただし、これらはBarrierではない。

Accessが競合するTask間だけ、論理順序に従って依存辺を生成する。Accessが競合しなければ、PhaseやPriorityをまたいで並列実行できる。

### Access Hazard

| 先行Access | 後続Access | 依存 |
|---|---|---|
| Read | Read | なし |
| Write | Read | あり |
| Write | Write | あり |
| Read | Write | あり |

同一Phase・Priorityで競合する場合は、登録順で依存方向を決める。

明示的な `Before / After` は導入しない。

### 非同期処理

通常のCPU Taskは関数終了時点でWrite完了とみなす。

PhysXなどの非同期WriteはCompletion HandleをResourceへ保持し、後続TaskがそのResourceへAccessする時点でFetchする。

### Script

Scriptは並列実行しない。

Scriptの順序も次で決める。

1. `Phase`
2. `Priority`
3. `RegistrationOrder`

Scriptは当面 `MainThread` かつ `WorldExclusive` として扱う。

### フレーム境界

標準機能として次は並列実行しない。

- Frame N と Frame N+1
- Fixed Tick N と Fixed Tick N+1
- Script同士

一つのFrame内部、一つのFixed Tick内部では、Accessが競合しない限りPhaseをまたいだ並列実行を許可する。

---

## 実装手順

### Step 0: 既知不具合修正

- [x] Step時にSystemがActive Scene数だけ重複実行される問題
- [x] `PhysicSystem::Start()` の空Sceneでの早期 `return`
- [x] `PhysicSystem::FixedUpdate()` の未初期化 `PxTransform` 参照
- [x] SceneContext登録解除と寿命管理
- [x] シーンロード時のEntity参照再マップ
- [x] Release Shader最適化設定の確認

### Step 1: 世代付きEntity

- [x] `Entity` を `index + generation` に変更
- [x] `EntityRegistry` に世代配列を追加
- [x] `EntityRef` の有効性判定を世代対応
- [x] Hash / YAML / PhysX userData対応

### Step 2: Component世代管理

- [x] StorageごとにComponent Generationを保持
- [x] 削除・再追加で世代を更新

### Step 3: 安全なComponentRef

- [x] Entity世代を検証
- [x] Component世代を検証
- [x] 生ポインタを永続保持しない
- [x] `TryGet()` / `GetChecked()` を提供

### Step 4: SystemとSystemTaskの分離

- [x] Systemは状態・設定を所有
- [x] TaskはSchedulerの実行単位
- [x] 一つのSystemが複数Taskを登録可能

### Step 5: Phase / Priority / RegistrationOrder

- [x] Task登録時にRegistrationOrderを発行
- [x] Schedule再構築でも登録順を維持
- [x] まず直列Executorで動作確認

### Step 6: SystemAccess

- [x] Component Read / Write
- [x] Resource Read / Write
- [x] WorldAccess
- [x] StructuralAccess
- [x] ThreadAffinity

### Step 7: 直列Schedule Compiler

- [x] Task収集
- [x] Hazard解析
- [x] 依存Graph構築
- [x] 循環検出
- [x] 直列Executor

### Step 8: Script実行順

- [x] ScriptへPhase / Priority / RegistrationOrderを追加
- [x] ScriptをMainThread / WorldExclusiveで直列実行

### Step 9: EntityCommandBuffer

- [x] Entity生成・削除を遅延化
- [x] Component追加・削除を遅延化
- [x] 専用Commit Taskで適用
- [x] Fixed / Frame / Editor / Renderの各Domain末尾でCommit
- [x] Script Lifecycle中のCommandを明示Commit

### Step 9.5: Script DLL境界の安全化

CRT方針:

- GameEngineはDebug x64で `/MTd`、Release x64で `/MT`
- Script DLLはDebug x64で `/MDd`、Release x64で `/MD`
- GameEngineはDirectXTex、Effekseer、PhysXなどの静的ライブラリへ合わせる
- Script DLLは既存のyaml-cppライブラリへ合わせる
- Module間ではSTLコンテナ、YAMLオブジェクト、ヒープ所有権を直接移動しない
- DLL内で生成したScriptインスタンスと状態バッファはDLL内で破棄する

進捗:

- [x] GameEngineとScript DLLを各依存ライブラリに合うCRTへ設定
- [x] Scriptプロジェクトへ `/utf-8` を追加
- [x] `CreateScript` と対になる `DestroyScript` をDLLへ追加
- [x] `ScriptComponent`をコピー禁止・move-only所有型へ変更
- [x] DLL解放前に旧DLL由来のScriptを破棄
- [x] Script状態のSerialize / DeserializeをC境界Bridge化
- [x] Hot Reload時の状態退避・再生成を実装
- [x] Inspector Parameter操作をC境界Bridge化
- [x] EntityCommandBuffer操作をEngine側Bridge経由へ移行
- [ ] Debug / Release x64でDLL Reloadを実機検証

### Step 10: Component純データ化

状態: **完了（現Storage互換段階）**

このStepの完了条件は、Component本体を状態定義と互換API宣言へ縮小し、YAML・Inspector・計算・Runtime処理を `Operations` 側へ分離することとする。

- [x] Transform
  - YAML / Inspectorを外部化
  - World行列計算を反復処理へ移行し、親循環を検出
  - UI Rect変換を専用Utilityへ分離
- [x] Follow
  - YAML / Inspectorを外部化
- [x] Camera
  - Serialization / Inspector / PostEffect Graph / Runtimeを分離
- [x] Light
  - YAML / Inspectorを外部化
  - `BOOL*` を `bool*` として扱う不正なInspector処理を除去
- [x] Particle
  - YAML / Inspectorを外部化
  - ImGui ItemWidthスタック不整合を除去
- [x] Collider
  - 反射マクロとpolymorphic型への `offsetof` 依存を除去
  - YAML / Inspector / Runtime解放処理を分離
  - 再Decode時の重複追加、空Layer参照、Layer選択添字を修正
- [x] ModelRenderer
  - Runtime / Serialization / Inspectorを分離
  - 無効Context・未ロードModel・Animation null参照を安全化
- [x] Material
  - YAML / Inspectorを外部化
  - Shader未登録・範囲外ShaderIDを安全化
- [x] Script Debug / Release x64ビルド
- [x] GameEngine Debug / Release x64ビルド

後続Stepへ引き継ぐ事項:

- `IComponent` 継承とvirtual互換APIの除去は、`IComponentStorage` の型制約を外すStep 11で行う
- ColliderのPhysXネイティブ資源所有権はStep 15で `PhysicsBridge` 側へ移す
- Camera / ModelRendererのD3D11資源所有権はStep 17で `RenderWorld` 側へ移す

### Step 11: DenseComponentPool

状態: **完了（高頻度データComponent移行段階）**

- [x] Dense Component配列
- [x] Dense Entity配列
- [x] Entity IndexベースのSparse Index
- [x] Component Generation
- [x] Structure Version
- [x] `std::span`アクセス
- [x] `IComponentStorage` の `IComponent*` 依存を除去
- [x] `ComponentView` / `ConstComponentView` による型消去アクセス
- [x] 高頻度データComponentをDense Poolへ移行
- [x] Dense対象Componentから `IComponent` 継承とvirtual overrideを除去

Dense移行対象:

- NameComponent
- TransformComponent
- RenderLayerComponent
- OrderInLayerComponent
- MaterialComponent
- LightComponent
- ParticleComponent
- CameraComponent
- FollowComponent
- EnvironmentMapComponent

Script、Collider、ModelRendererなど、DLL境界・ネイティブ資源・ポインタ安定性を優先する型はSparse Storageを維持する。これらの継承除去は各所有権移行Stepで行う。

検証: Windows Build run #335でScript / GameEngineのDebug・Release x64が成功。

### Step 12: 無確保Query

状態: **完了**

- [x] `ComponentMask` による遅延フィルタQuery View
- [x] Query生成時の結果vector確保を除去
- [x] `Read<T>` / `Write<T>` から `SystemAccess` を自動生成
- [x] `SystemScheduleBuilder::AddQueryTask<QueryType>()`
- [x] Query外の構造変更を `StructuralAccess` で明示
- [x] TransformSystemを初期移行
- [x] Schedule外の即時削除経路はスナップショット列挙でiterator無効化を防止

検証: Windows Build run #358でScript / GameEngineのDebug・Release x64が成功。

### Step 13: Job System

- [ ] 常駐Worker
- [ ] Work Stealing
- [ ] Job Counter / Fence
- [ ] ParallelFor
- [ ] Thread-local Command Buffer
- [ ] Thread-local Scratch Allocator

### Step 14: Parallel Schedule Executor

- [ ] 依存数0のTaskをReady Queueへ投入
- [ ] Task完了時に後続依存数を減算
- [ ] MainThread Queue対応

### Step 15: PhysX Task分割

- [ ] PhysicsUpload
- [ ] PhysicsBegin
- [ ] PhysicsFetch
- [ ] PhysicsDownload
- [ ] CollisionEventDispatch
- [ ] ColliderからPhysXネイティブ資源所有権を分離

### Step 16: RHI抽象化

- [ ] Resource Handle
- [ ] Buffer / Texture
- [ ] Shader
- [ ] Pipeline State
- [ ] Command List
- [ ] SwapChain
- [ ] RenderPass
- [ ] RenderGraph

### Step 17: RenderWorld

- [ ] ECS WorldからRenderWorldへ抽出
- [ ] RendererからComponentRegistry直接参照を除去
- [ ] Camera / ModelRendererからD3D11資源所有権を分離

### Step 18: 描画並列化の再検討

D3D11 Backendでは制約に合わせて直列提出し、D3D12 / Vulkan BackendではCommand List並列構築を可能にする。

### Step 19: Scriptプロジェクト完全ホットリロード対応

- [ ] Scriptソース変更監視と自動ビルド
- [ ] ビルド完了検出とDebounce
- [ ] 一意な一時DLL / PDB名へのコピー
- [ ] 新DLLのABI Version・必須Export検証
- [ ] 新DLLロード失敗時の旧DLL継続とRollback
- [ ] Script状態・Phase・Priority・RegistrationOrder・Entity参照の完全復元
- [ ] 追加・削除・名称変更されたScript型への対応
- [ ] Reload中のScript実行停止と安全な再開
- [ ] 古い一時DLL / PDBのクリーンアップ
- [ ] Debug / Releaseで連続Reload試験

### Step 20: README.md更新

- [ ] Engine全体アーキテクチャ
- [ ] ECS / Scheduler仕様
- [ ] Entity / ComponentRef安全性
- [ ] SystemAccessと並列実行規則
- [ ] Script DLLとHot Reload仕様
- [ ] Rendering PipelineとRHI方針
- [ ] ビルド手順・依存ライブラリ・CRT構成
- [ ] Editor機能と基本操作
- [ ] 現在の制約・今後のRoadmap
