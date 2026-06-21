# ECS / Scheduler / RHI Migration Plan

## 目的

現在の疑似ECSとDirect3D 11へ密結合したRendererを、次の構造へ段階的に移行する。

- 世代付きEntity / Component参照
- Access Hazardから依存を構築するSystemTask Scheduler
- Job Systemによる安全な並列実行
- ECS WorldとRenderWorldの分離
- Direct3D 11 / Direct3D 12 / Vulkanを追加可能なMulti-Backend RHI
- Script DLLの安全な完全ホットリロード

既存のPlayer Viewとゲーム実行を各Stepで維持し、全面的な一括差し替えは行わない。

---

# 1. Scheduler確定仕様

## 1.1 実行単位

Schedulerが扱う最小単位は`SystemTask`とする。

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

## 1.2 論理順序

Taskの論理順序は次で決める。

1. `Phase`
2. `Priority`
3. `RegistrationOrder`

PhaseとPriority自体はBarrierではない。
Accessが競合するTask間だけ、論理順序に従って依存辺を生成する。

| 先行Access | 後続Access | 依存 |
|---|---|---|
| Read | Read | なし |
| Write | Read | あり |
| Write | Write | あり |
| Read | Write | あり |

明示的な`Before / After`は導入しない。

## 1.3 非同期処理

通常CPU Taskは関数終了時点でWrite完了とみなす。
PhysXやGPUなどの非同期処理はCompletion HandleまたはFenceをResourceへ保持し、後続TaskがAccessする時点で同期する。

## 1.4 Script

Scriptは当面並列実行しない。

- `MainThread`
- `WorldExclusive`
- Phase → Priority → RegistrationOrder順

## 1.5 フレーム境界

標準機能として次は並列実行しない。

- Frame NとFrame N+1
- Fixed Tick NとFixed Tick N+1
- Script同士

同一Frame / Fixed Tick内ではAccessが競合しないTaskの並列実行を許可する。

---

# 2. 実装手順

## Step 0: 既知不具合修正

状態: **完了**

- [x] Step時にSystemがActive Scene数だけ重複実行される問題
- [x] `PhysicSystem::Start()`の空Scene早期return
- [x] `PhysicSystem::FixedUpdate()`の未初期化`PxTransform`
- [x] SceneContext登録解除と寿命管理
- [x] Sceneロード時のEntity参照再マップ
- [x] Release Shader最適化設定

## Step 1: 世代付きEntity

状態: **完了**

- [x] `Entity = index + generation`
- [x] EntityRegistry世代配列
- [x] EntityRef世代検証
- [x] Hash / YAML / PhysX userData対応

## Step 2: Component世代管理

状態: **完了**

- [x] StorageごとのComponent Generation
- [x] 削除・再追加時の世代更新

## Step 3: 安全なComponentRef

状態: **完了**

- [x] Entity世代検証
- [x] Component世代検証
- [x] 生ポインタの永続保持を廃止
- [x] `TryGet()` / `GetChecked()`

## Step 4: SystemとSystemTaskの分離

状態: **完了**

- [x] Systemは状態・設定を所有
- [x] TaskはScheduler実行単位
- [x] 一つのSystemから複数Taskを登録

## Step 5: Phase / Priority / RegistrationOrder

状態: **完了**

- [x] Task登録時にRegistrationOrderを発行
- [x] Schedule再構築時も登録順を維持
- [x] 直列Executorで初期検証

## Step 6: SystemAccess

状態: **完了**

- [x] Component Read / Write
- [x] Resource Read / Write
- [x] WorldAccess
- [x] StructuralAccess
- [x] ThreadAffinity

## Step 7: 直列Schedule Compiler

状態: **完了**

- [x] Task収集
- [x] Hazard解析
- [x] 依存Graph構築
- [x] 循環検出
- [x] 直列Executor

## Step 8: Script実行順

状態: **完了**

- [x] ScriptへPhase / Priority / RegistrationOrderを追加
- [x] MainThread / WorldExclusiveで直列実行

## Step 9: EntityCommandBuffer

状態: **完了**

- [x] Entity生成・削除の遅延化
- [x] Component追加・削除の遅延化
- [x] 専用Commit Task
- [x] Fixed / Frame / Editor / Render末尾Commit
- [x] Script Lifecycle中の明示Commit

## Step 9.5: Script DLL境界の安全化

状態: **コード完了・実機Reload検証待ち**

CRT方針:

- GameEngine Debug: `/MTd`
- GameEngine Release: `/MT`
- Script Debug: `/MDd`
- Script Release: `/MD`
- DLL境界でSTL Container、YAML Object、Heap所有権を直接移動しない
- DLL内で生成したScript Instance / State BufferはDLL内で破棄する

進捗:

- [x] CRT設定
- [x] Script `/utf-8`
- [x] `CreateScript` / `DestroyScript`
- [x] ScriptComponentのmove-only化
- [x] DLL解放前のScript破棄
- [x] Serialize / Deserialize C Bridge
- [x] Hot Reload時の状態退避・復元
- [x] Inspector Parameter C Bridge
- [x] EntityCommandBuffer Engine Bridge
- [ ] Debug / Release実機Reload検証

## Step 10: Component純データ化

状態: **完了（現Storage互換段階）**

- [x] Transform Operations分離
- [x] Follow Operations分離
- [x] Camera Serialization / Inspector / Runtime分離
- [x] Light Operations分離
- [x] Particle Operations分離
- [x] Collider Operations / Runtime解放分離
- [x] ModelRenderer Operations / Runtime分離
- [x] Material Operations分離
- [x] Script Debug / Release x64
- [x] GameEngine Debug / Release x64

引継ぎ:

- Sparse Componentの`IComponent`除去は各所有権移行Stepで行う
- Camera / ModelRendererのNative描画資源はStep 17でRenderWorldへ移す

## Step 11: DenseComponentPool

状態: **完了（高頻度Component移行段階）**

- [x] Dense Component配列
- [x] Dense Entity配列
- [x] Entity IndexベースSparse Index
- [x] Component Generation
- [x] Structure Version
- [x] `std::span`
- [x] 型非依存`IComponentStorage`
- [x] `ComponentView` / `ConstComponentView`
- [x] 高頻度ComponentをDense Poolへ移行
- [x] Dense対象から`IComponent`継承を除去

Dense対象:

- Name
- Transform
- RenderLayer / OrderInLayer
- Material
- Light
- Particle
- Camera
- Follow
- EnvironmentMap

Script、Collider、ModelRendererはDLL境界・Native資源・Pointer安定性のためSparseを維持する。

検証: Windows Build run #335。

## Step 12: 無確保Query

状態: **完了**

- [x] `ComponentMask`遅延Query View
- [x] 結果vector確保を除去
- [x] `Read<T>` / `Write<T>`からSystemAccess生成
- [x] `AddQueryTask<QueryType>()`
- [x] Query外StructuralAccess明示
- [x] TransformSystem初期移行
- [x] 即時削除経路のiterator無効化防止

検証: Windows Build run #358。

## Step 13: Job System

状態: **完了**

- [x] 常駐Worker
- [x] Worker-local Deque
- [x] Work Stealing
- [x] Job Counter / Fence
- [x] Nested Wait支援
- [x] ParallelFor
- [x] Worker-local Command Buffer
- [x] Marker / Rewind Scratch Allocator
- [x] Job例外伝播
- [x] SystemRegistryによる起動・停止
- [x] Stop時Drain / Join
- [x] Job System Smoke Test

検証: Windows Build run #383。

## Step 14: Parallel Schedule Executor

状態: **完了**

- [x] Ready Queue
- [x] 後続Task依存数減算
- [x] AnyWorker → JobSystem
- [x] MainThread Queue
- [x] D3D11期間中のRenderThread → MainThread fallback
- [x] Worker例外時Graph排水
- [x] 例外後の後続Taskスキップ
- [x] JobThreadContext公開
- [x] ExclusiveWorldWrite直前のWorker Command Flush
- [x] JobSystem停止中の直列fallback
- [x] SystemRegistry接続
- [x] Schedule Executor Smoke Test

検証: Windows Build run #441。

詳細: `Docs/Step14_ParallelScheduleExecutor_Completion.md`

## Step 15: PhysX Task分割

状態: **コード・CI完了、実機Runtime確認待ち**

- [x] PhysicsUpload
- [x] PhysicsBegin
- [x] PhysicsFetch
- [x] PhysicsDownload
- [x] CollisionEventDispatch
- [x] PhysX callbackからScript直接実行を除去
- [x] Collision / Trigger EventをMainThread配送
- [x] Collider Native Resource解放をPhysics Bridgeへ集約
- [x] Collider内Native Pointerを非所有Aliasとして明示
- [x] Character Fixed回転がPhysicsより先に実行されるPhase修正
- [x] Transform Quaternion / Euler連続性修正

検証: Windows Build run #468、回転回帰修正run #482。

実機確認:

- [ ] Dynamic Actorへの重力
- [ ] Collision Enter / Stay / Exit
- [ ] Trigger Enter / Exit
- [ ] Stop → PlayでのActor再生成
- [ ] Player View / PhysX Debug描画
- [ ] Character回転とRotation Undo / Redo

詳細: `Docs/Step15_PhysX_Task_Split_Completion.md`

## Step 16: Multi-Backend RHI

状態: **実装中**

目的:

Direct3D 11コードを別名へ差し替えるWrapperではなく、同じRenderer / RenderWorld / RenderGraphからDirect3D 11、Direct3D 12、Vulkanを実装できる共通契約を構築する。

層構造:

```text
Renderer / RenderWorld / RenderGraph
                |
                v
       Backend-independent RHI
 Backend / Device / Queue / CommandList
 Handle / Descriptor / Pipeline / RenderPass
                |
       +--------+--------+
       |        |        |
     D3D11    D3D12    Vulkan
```

### Step 16-A: Backend選択とCapability

- [x] `BackendType`
- [x] `IRHIBackend`
- [x] `BackendRegistry`
- [x] `DeviceCreateDesc`
- [x] API非依存`NativeWindowHandle`
- [x] `AdapterInfo`
- [x] `DeviceCapabilities`
- [ ] 起動ConfigからBackend選択
- [ ] Null Backend登録
- [ ] D3D11 Backend Factory登録

### Step 16-B: Device / Queue / Synchronization

- [x] `IRHIDevice`
- [x] Graphics / Compute / Copy Queue種別
- [x] `IRHICommandQueue`
- [x] `IRHICommandList`
- [x] `IRHIFence`
- [x] `FenceHandle`
- [x] Queue Submit Descriptor
- [ ] Null Queue / Fence実装
- [ ] D3D11 Immediate ContextをGraphics Queueへ適合
- [ ] D3D11 Compute / Copy Queue fallback規則
- [ ] Timeline同期Capabilityによる分岐

正式契約では`GetImmediateCommandList()`を使用しない。
D3D11 Backendだけが内部でImmediate Contextへ変換する。

### Step 16-C: Resource

- [x] 世代付きResource Handle
- [x] Resource Pool
- [x] Buffer Descriptor
- [x] Texture Descriptor
- [x] Shader Descriptor
- [x] D3D11 Buffer生成
- [x] D3D11 Texture生成
- [x] D3D11 Shader生成
- [ ] Null Resource実装
- [ ] Buffer / Texture View Descriptor
- [ ] Sampler Handle / Descriptor
- [ ] Resource State / Barrier Descriptor

### Step 16-D: Pipeline State

- [x] API非依存`PipelineStateDesc`
- [x] Shader Handle参照検証
- [ ] Render Target Format列
- [ ] Depth Format
- [ ] D3D11 Input Layout生成
- [ ] D3D11 Rasterizer State生成
- [ ] D3D11 Depth / Stencil State生成
- [ ] D3D11 Blend State生成
- [ ] Null Pipeline実装

Backend変換:

- D3D11: 複数State Objectの集合
- D3D12: PSO
- Vulkan: `VkPipeline`

### Step 16-E: SwapChain / RenderPass

- [x] SwapChain Descriptor
- [x] `IRHISwapChain`
- [x] RenderPass Descriptor
- [x] Load / Store Operation
- [x] D3D11 SwapChain Runtime
- [x] D3D11 RenderPass Binding
- [ ] SwapChain Image Handle取得
- [ ] Present Queue経由化
- [ ] Resize時のGPU同期契約

### Step 16-F: RenderGraph

- [x] Logical Resource
- [x] Read / Write / ReadWrite Access
- [x] Hazard依存構築
- [x] 安定した実行順
- [ ] Transient Resource寿命解析
- [ ] Resource State遷移
- [ ] Backend Barrier生成
- [ ] Queue間同期
- [ ] Pass Culling

### Step 16-G: 検証

- [x] RHI Handle Smoke Test
- [x] RenderGraph依存Smoke Test
- [x] D3D11 Backend Header / Contract Build
- [ ] Null BackendによるAPI非依存実行Test
- [ ] D3D11 Queue適合Test
- [ ] D3D11実描画最小Triangle
- [ ] Debug / Release x64 Engine回帰

Step 16完了条件:

- Renderer上位層がNative API型を参照しない契約が完成
- Null BackendでRHI単体Testが実行可能
- D3D11 Backendが正式Queue契約へ適合
- D3D12 / Vulkan Backendを追加しても公開RHI契約を変更しない
- 既存Player Viewに回帰がない

詳細: `Docs/Step16_RHI_MultiBackend_Architecture.md`

## Step 17: RenderWorld

状態: **未着手**

- [ ] ECS WorldからRenderWorldへ抽出
- [ ] RendererからComponentRegistry直接参照を除去
- [ ] Camera / ModelRendererからNative描画資源所有権を分離
- [ ] RendererからNative API型参照を除去
- [ ] RenderWorldからRHI Commandを生成

## Step 18: 描画並列化の再検討

状態: **未着手**

- D3D11 Backend: Graphics Queueへ直列提出
- D3D12 / Vulkan Backend: Command List並列構築
- CapabilityによりAsync Compute / Multiple Queueを選択
- API名による上位層分岐は禁止

## Step 19: Scriptプロジェクト完全ホットリロード対応

状態: **未着手**

- [ ] Scriptソース変更監視と自動ビルド
- [ ] Debounce
- [ ] 一意な一時DLL / PDB
- [ ] ABI Version・必須Export検証
- [ ] 旧DLL継続とRollback
- [ ] Script状態・順序・Entity参照の完全復元
- [ ] Script型追加・削除・名称変更
- [ ] Reload中の実行停止と再開
- [ ] 一時ファイルCleanup
- [ ] Debug / Release連続Reload試験

## Step 20: README.md更新

状態: **未着手**

- [ ] Engine全体Architecture
- [ ] ECS / Scheduler
- [ ] Entity / ComponentRef
- [ ] SystemAccess / 並列実行規則
- [ ] Script DLL / Hot Reload
- [ ] Rendering Pipeline / Multi-Backend RHI
- [ ] Build / Dependencies / CRT
- [ ] Editor機能
- [ ] 制約とRoadmap

---

# 3. 現在の作業位置

1. Step 16-BのQueue契約へD3D11 / Null Backendを適合
2. Null Backend Smoke Test
3. Pipeline State Native変換
4. SwapChain Image / Present Queue
5. RenderGraph Resource State / Barrier
6. D3D11最小実描画Test
7. Step 16完了報告

Step 17のRenderWorld移行は、Step 16の公開契約を固定してから開始する。
