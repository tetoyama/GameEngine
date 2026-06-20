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

- [ ] `Entity` を `index + generation` に変更
- [ ] `EntityRegistry` に世代配列を追加
- [ ] `EntityRef` の有効性判定を世代対応
- [ ] Hash / YAML / PhysX userData対応

### Step 2: Component世代管理

- [ ] StorageごとにComponent Generationを保持
- [ ] 削除・再追加で世代を更新

### Step 3: 安全なComponentRef

- [ ] Entity世代を検証
- [ ] Component世代を検証
- [ ] 生ポインタを永続保持しない
- [ ] `TryGet()` / `GetChecked()` を提供

### Step 4: SystemとSystemTaskの分離

- [ ] Systemは状態・設定を所有
- [ ] TaskはSchedulerの実行単位
- [ ] 一つのSystemが複数Taskを登録可能

### Step 5: Phase / Priority / RegistrationOrder

- [ ] Task登録時にRegistrationOrderを発行
- [ ] Schedule再構築でも登録順を維持
- [ ] まず直列Executorで動作確認

### Step 6: SystemAccess

- [ ] Component Read / Write
- [ ] Resource Read / Write
- [ ] WorldAccess
- [ ] StructuralAccess
- [ ] ThreadAffinity

### Step 7: 直列Schedule Compiler

- [ ] Task収集
- [ ] Hazard解析
- [ ] 依存Graph構築
- [ ] 循環検出
- [ ] 直列Executor

### Step 8: Script実行順

- [ ] ScriptへPhase / Priority / RegistrationOrderを追加
- [ ] ScriptをMainThread / WorldExclusiveで直列実行

### Step 9: EntityCommandBuffer

- [ ] Entity生成・削除を遅延化
- [ ] Component追加・削除を遅延化
- [ ] 専用Commit Taskで適用

### Step 10: Component純データ化

優先順位:

1. Transform
2. Follow
3. Camera
4. Light
5. Particle
6. Collider
7. ModelRenderer
8. Material

### Step 11: DenseComponentPool

- [ ] Dense Component配列
- [ ] Dense Entity配列
- [ ] Sparse Index
- [ ] Component Generation
- [ ] Structure Version
- [ ] Spanアクセス

### Step 12: 無確保Query

- [ ] Query View
- [ ] Query型からSystemAccessを自動生成

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

### Step 18: 描画並列化の再検討

D3D11 Backendでは制約に合わせて直列提出し、D3D12 / Vulkan BackendではCommand List並列構築を可能にする。
