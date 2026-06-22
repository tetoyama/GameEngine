# ECS / Scheduler / RHI Migration Plan

## 目的

現在の疑似ECSとDirect3D 11へ密結合したRendererを、次の構造へ段階的に移行する。

- 世代付きEntity / Component参照
- Access Hazardから依存を構築するSystemTask Scheduler
- Job Systemによる安全な並列実行
- ECS WorldとRenderWorldの分離
- Direct3D 11 / Direct3D 12 / Vulkanを同じ上位層から利用できるMulti-Backend RHI
- Script DLLの安全な完全Hot Reload

既存機能を壊さず、各Stepで単体検証とDebug / Release x64回帰を行う。

---

# 1. 移行後の基本契約

## 1.1 Entity

```cpp
struct Entity {
    uint32_t index;
    uint32_t generation;
};
```

- index再利用時にgenerationを進める
- `index`だけを永続参照として保存しない
- 破棄済みEntityと再利用後Entityを区別する

## 1.2 Component

- Componentはデータのみを保持する
- GUI、GPU Resource、PhysX Object、DLL関数ポインタを所有しない
- Component参照は世代付き`ComponentRef<T>`で検証する

## 1.3 SystemTask

- SystemはTaskを生成する
- SchedulerはPhase / Priority / Access / RegistrationOrderから依存を構築する
- Access ConflictはResource Key単位で判定する
- 同一条件の順序はRegistrationOrderで安定化する
- Task名は`<SystemName>.<Feature>.<Stage>`を基本とし、Captureだけで責務を判別できるようにする

## 1.4 構造変更

- Entity / Component追加削除は`EntityCommandBuffer`へ記録する
- 並列実行中にRegistryを直接変更しない
- Phase終端で決定的順序にPlaybackする

## 1.5 Rendering

- ECS WorldからRenderWorldを抽出する
- RendererはComponentRegistryを直接参照しない
- 上位描画層はNative API型を参照しない
- Backend差はCapabilityで選択し、API名で分岐しない
- MainThread必須処理から純粋CPU計算を`Build` Taskとして分離し、GPU反映を`Upload / Submit` TaskとしてMainThreadへ残す

---

# 2. 実行順と進捗

## Step 0: 既知不具合修正

状態: **完了**

- [x] Transform階層の検証強化
- [x] 順序依存処理の安定化
- [x] Script DLL境界のCRT整合
- [x] Debug / Release x64回帰

## Step 1: 世代付きEntity

状態: **完了**

- [x] `Entity { index, generation }`
- [x] Free List再利用
- [x] `IsAlive()`
- [x] 古いEntity参照の拒否

## Step 2: Component世代管理

状態: **完了**

- [x] Component Slotのgeneration管理
- [x] 再配置・削除後の古い参照拒否
- [x] Entity generationとの整合確認

## Step 3: 安全なComponentRef

状態: **完了**

- [x] `ComponentRef<T>`
- [x] Owner Entity / Component generation検証
- [x] Null / stale参照の安全な拒否

## Step 4: SystemとSystemTaskの分離

状態: **完了**

- [x] System定義と実行Taskの分離
- [x] Task Callback
- [x] Scheduler入力形式の固定

## Step 5: Phase / Priority / RegistrationOrder

状態: **完了**

- [x] Phase
- [x] Priority
- [x] RegistrationOrder
- [x] 同一条件での決定的順序

## Step 6: SystemAccess

状態: **完了**

- [x] Read / Write Access宣言
- [x] Resource Key
- [x] Read-Write / Write-Write Hazard判定
- [x] 構造変更Access

## Step 7: 直列Schedule Compiler

状態: **完了**

- [x] Phase順序
- [x] 明示依存
- [x] Access Hazard依存
- [x] Stable Topological Sort
- [x] Cycle検出

## Step 8: Script実行順

状態: **完了**

- [x] ScriptをSystemTaskとして登録
- [x] Start / Update / Fixed / Draw / Stop順序
- [x] Script登録順の安定化
- [x] DLL境界を越える寿命の分離

## Step 9: EntityCommandBuffer

状態: **完了**

- [x] Entity生成 / 破棄Command
- [x] Component追加 / 削除Command
- [x] Phase終端Playback
- [x] 決定的Merge順

## Step 9.5: Script DLL境界の安全化

状態: **コード完了・実機Reload検証待ち**

- [x] DLL所有Objectのホスト側破棄回避
- [x] ABI Version検証
- [x] Export検証
- [x] Reload失敗時の旧DLL維持
- [ ] Debug / Release x64連続Reload実機試験

## Step 10: Component純データ化

状態: **完了**

- [x] ComponentからEditor GUI責務を除去
- [x] ComponentからNative GPU Resource所有を除去する方針固定
- [x] ComponentからPhysX Object所有を除去する方針固定
- [x] Runtime Service / System側へ責務移動

## Step 11: DenseComponentPool

状態: **完了**

- [x] Dense / Sparse構造
- [x] O(1)追加・削除・取得
- [x] Swap Remove
- [x] generation整合

## Step 12: 無確保Query

状態: **完了**

- [x] Query結果の毎Frame Allocation除去
- [x] Dense Pool走査
- [x] 複数Component Query
- [x] stale Entity除外

## Step 13: Job System

状態: **完了**

- [x] 常駐Worker
- [x] WorkerごとのDequeとWork Stealing
- [x] Job Counter / Fence
- [x] Nested Wait中のJob実行支援
- [x] ParallelFor
- [x] Worker-local Command Buffer
- [x] Marker / Rewind対応Scratch Allocator
- [x] Job例外伝播
- [x] 安全なDrain / Join
- [x] Windows Smoke Test

## Step 14: Parallel Schedule Executor

状態: **完了**

- [x] Ready Queue
- [x] indegree管理
- [x] Hazard非競合Taskの並列投入
- [x] Phase Barrier
- [x] RegistrationOrderによる安定化
- [x] Worker-local Command Bufferの決定的Merge
- [x] 例外伝播と中断
- [x] Schedule Executor Smoke Test

## Step 15: PhysX Task分割

状態: **コード完了・実機Runtime確認待ち**

- [x] Physics Simulation Task
- [x] Collision / Trigger収集Task
- [x] Transform同期Task
- [x] ComponentからPhysX Object所有責務を分離
- [x] Stop / Play境界整理
- [x] Transform Rotation回帰Smoke Test
- [ ] Player ViewでPhysics / Collision / Triggerを実機確認
- [ ] Stop → Play再開を実機確認
- [ ] Character Rotation操作を実機確認

詳細:

- `Docs/Step15_PhysX_Task_Decomposition.md`
- `Docs/Step15_PhysX_Runtime_Validation.md`

## Step 16: Multi-Backend RHI

状態: **実装中**

目的:

Direct3D 11コードを別名へ差し替えるWrapperではなく、同じRenderer / RenderWorld / RenderGraphからDirect3D 11、Direct3D 12、Vulkanを実装できる共通契約を構築する。

### Step 16-A: Backend選択とCapability

- [x] `BackendType`
- [x] `IRHIBackend`
- [x] `BackendRegistry`
- [x] `DeviceCreateDesc`
- [x] API非依存`NativeWindowHandle`
- [x] `AdapterInfo`
- [x] `DeviceCapabilities`
- [x] Null Backend登録
- [x] D3D11 Backend Factory登録
- [x] 起動ConfigからBackend選択

### Step 16-B: Device / Queue / Synchronization

- [x] `IRHIDevice`
- [x] Graphics / Compute / Copy Queue種別
- [x] `IRHICommandQueue`
- [x] `IRHICommandList`
- [x] `IRHIFence`
- [x] `FenceHandle`
- [x] Queue Submit Descriptor
- [x] Null Queue / Fence実装
- [x] D3D11 Immediate Contextを論理Queueへ適合
- [x] D3D11 Compute / Copy Queue fallback規則
- [x] D3D11 EVENT QueryによるGPU完了Fence
- [x] Timeline同期Capability公開
- [ ] 上位層のTimeline / 非Timeline分岐

D3D11はGraphics / Compute / Copyの論理Queueを公開するが、実体はImmediate Context一本へ直列化する。
`supportsAsyncCompute=false`、`supportsMultipleCommandQueues=false`を維持する。

### Step 16-C: Resource / View / Sampler / State

- [x] 世代付きResource Handle
- [x] Resource Pool
- [x] Buffer / Texture / Shader Descriptor
- [x] D3D11 Buffer / Texture / Shader生成
- [x] Null Resource実装
- [x] Buffer View Descriptor / Handle
- [x] Texture View Descriptor / Handle
- [x] Sampler Descriptor / Handle
- [x] D3D11 Buffer SRV / UAV
- [x] D3D11 Texture SRV / UAV / RTV / DSV
- [x] D3D11 Sampler State
- [x] Null View / Sampler実装
- [x] View生存中の親Resource破棄拒否
- [x] Resource State / Barrier Descriptor
- [x] D3D11論理State追跡
- [x] Null Barrier契約検証

### Step 16-D: Pipeline State

- [x] API非依存`PipelineStateDesc`
- [x] Shader Handle参照検証
- [x] Render Target Format列
- [x] Depth Format
- [x] D3D11 Input Layout生成
- [x] D3D11 Rasterizer State生成
- [x] D3D11 Depth / Stencil State生成
- [x] D3D11 Blend State生成
- [x] Null Pipeline実装

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
- [x] Texture ViewベースD3D11 RenderPass Binding
- [x] SwapChain Image Handle取得
- [x] Present Queue経由化
- [x] Resize時のGPU同期契約

### Step 16-F: RenderGraph

- [x] Logical Resource
- [x] Read / Write / ReadWrite Access
- [x] Hazard依存構築
- [x] 安定した実行順
- [x] 外部Buffer / Texture Import
- [x] Resource初期State
- [x] Pass要求State
- [x] Transition Barrier自動生成
- [x] 連続UAV Write時のUAV Barrier生成
- [x] Backend Barrier発行
- [x] 最終Resource State取得
- [x] 競合State検出
- [x] Subresource単位State宣言
- [x] Transient Resource寿命解析
- [ ] Queue間同期
- [ ] Pass Culling

### Step 16-G: 検証

- [x] RHI Handle Smoke Test
- [x] RenderGraph依存Smoke Test
- [x] RenderGraph Barrier Smoke Test
- [x] D3D11 Backend Header / Contract Build
- [x] Null BackendによるAPI非依存実行Test
- [x] D3D11 Queue / View / Sampler / Fence適合Test
- [x] SwapChain lifecycle contract Smoke Test
- [x] Subresource State / Barrier Smoke Test
- [x] Debug / Release x64 Engine回帰
- [ ] D3D11実描画最小Triangle
- [ ] 既存Player View実機回帰

直近検証:

- Migration Plan Validation run #202: success
- RHI Smoke Test run #256: success
- Windows Build run #776: success

Step 16完了条件:

- Renderer上位層がNative API型を参照しない契約が完成
- Null BackendでRHI単体Testが実行可能
- D3D11 Backendが正式Queue契約へ適合
- D3D12 / Vulkan Backendを追加しても公開RHI契約を変更しない
- 既存Player Viewに回帰がない

詳細:

- `Docs/Step16_RHI_MultiBackend_Architecture.md`
- `Docs/Step16A_Backend_Config_Completion.md`
- `Docs/Step16B_D3D11_LogicalQueue_Completion.md`
- `Docs/Step16C_ResourceState_Barrier_Completion.md`
- `Docs/Step16C_View_Sampler_Completion.md`
- `Docs/Step16E_SwapChain_Completion.md`
- `Docs/Step16F_Subresource_State_Completion.md`

## Step 17: Task命名統一とMainThread Task分割

状態: **未着手**

優先順:

1. SystemTask命名規則の統一
2. Render Packet基盤
3. Animation CPU Build / GPU Upload分離
4. Terrain CPU Mesh Build / GPU Upload分離
5. Wave CPU Vertex Build / GPU Upload分離
6. Physics Begin–Fetch待機隠蔽の再評価

基本契約:

- `ScriptSystem` / `CustomScriptSystem`はMainThread・World Exclusiveを維持する
- Graphics API操作はMainThreadへ残す
- 純粋CPU計算のみAnyWorker Taskへ抽出する
- `Prepare / Build -> Commit / Upload / Submit`の二段階構造を採用する
- 物理待機隠蔽は実測効果が十分な場合だけ導入する

詳細:

- `Docs/Step17_Task_Naming_And_Render_Task_Decomposition_Plan.md`

## Step 18: RenderWorld

状態: **未着手**

- [ ] ECS WorldからRenderWorldへ抽出
- [ ] RendererからComponentRegistry直接参照を除去
- [ ] Camera / ModelRendererからNative描画資源所有権を分離
- [ ] RendererからNative API型参照を除去
- [ ] RenderWorldからRHI Commandを生成

## Step 19: 描画並列化の再検討

状態: **未着手**

- D3D11 Backend: Graphics Queueへ直列提出
- D3D12 / Vulkan Backend: Command List並列構築
- CapabilityによりAsync Compute / Multiple Queueを選択
- API名による上位層分岐は禁止

## Step 20: Scriptプロジェクト完全ホットリロード対応

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

## Step 21: README.md更新

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

1. Step 17-A SystemTask命名規則の統一
2. Step 16-FのQueue間同期
3. Step 16-FのPass Culling
4. D3D11最小実描画Triangle Test
5. 既存Player View実機回帰
6. Step 17-B Render Packet基盤
7. Step 17-C Animation CPU Build / GPU Upload分離

Task命名統一は、以降のCapture比較と依存解析の前提になるため最優先で実施する。
Step 17-B以降は、既存Player View回帰を維持しながら段階的に進める。
