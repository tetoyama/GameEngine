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
- `AddComponent`の戻り生ポインタと`GetDenseComponentSpan`は、同型の後続追加によるStorage再確保 / rehashで無効化しうる。寿命をまたぐ保持は`ComponentRef<T>`へ限定する（Review M-4: `Registry/componentRegistry.h:171-206`。SparseStable戦略型が対象）
- Component型IDは`ComponentMask`(`std::bitset<256>`)のBit位置へ直結する。登録型数が`MAX_COMPONENTS`(256)へ達するとMask操作が`out_of_range`で例外化し捕捉されず`terminate`する。登録時と`AddComponent`でID上限をassertする（Review H-1: `Interface/IComponentStorage.h:23-24`。H1対応で64→256へ拡張済）
- 既存Componentへの再`Add`はコンストラクタ引数を黙って破棄する現仕様を明文化し、上書きは`Set` / `Replace`として別APIへ分離する（Review M-4）

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
- Query / View反復中の即時構造変更はイテレータ無効化(UB)となる。現状は「反復中は構造変更禁止」という契約のみで非強制のため、Debugビルドで反復開始時のStructure Versionを捕捉し`operator++` / `operator*`で不一致をassertする。即時構造変更APIはSystemから直接呼ばせず内部専用へ寄せる（Review H-4: `Query/ComponentQueryView.h`, `Registry/componentRegistry.h:353-365`）
- `ComponentRegistry`の各コンテナは無同期のため、並列Taskからの即時`AddComponent` / `RegisterComponent`は他Threadのreadと競合する。Access宣言漏れをDebug時に検出する（Review M-3相当。安全性がSystemのAccess宣言正確性へ全依存している点を強制側へ寄せる）

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
- [x] Queue間同期
- [x] Pass Culling

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
- [x] D3D11実描画最小Triangle
- [x] 既存Player View実機回帰

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
- `Docs/Step16F_Queue_Synchronization_Completion.md`
- `Docs/Step16F_Pass_Culling_Completion.md`
- `Docs/Step16G_D3D11_Real_Triangle_Completion.md`
- `Docs/Step16G_Player_View_Regression_Guard.md`
- `Docs/Step16G_Player_View_Regression_Completion.md`

## Step 17: Task命名統一とMainThread Task分割

状態: **実装中**

優先順:

1. SystemTask命名規則の統一
2. Render Packet基盤
3. Draw Performance計測内訳化
4. IRenderable内部のComponentRegistry参照除去
5. Animation CPU Build / GPU Upload分離
6. Terrain CPU Mesh Build / GPU Upload分離
7. Wave CPU Vertex Build / GPU Upload分離
8. Physics Begin–Fetch待機隠蔽の再評価

基本契約:

- `ScriptSystem` / `CustomScriptSystem`はMainThread・World Exclusiveを維持する
- Graphics API操作はMainThreadへ残す
- 純粋CPU計算のみAnyWorker Taskへ抽出する
- `Prepare / Build -> Commit / Upload / Submit`の二段階構造を採用する
- 物理待機隠蔽は実測効果が十分な場合だけ導入する

詳細:

- `Docs/Step17_Task_Naming_And_Render_Task_Decomposition_Plan.md`
- `Docs/Step17A_Task_Naming_Completion.md`
- `Docs/Step17B_Draw_Performance_Breakdown.md`

## Step 18: RenderWorld / Static Entity Batching

状態: **未着手**

### Step 18-A: RenderWorld基盤

- [ ] ECS WorldからRenderWorldへ抽出
- [ ] RendererからComponentRegistry直接参照を除去
- [ ] Camera / ModelRendererからNative描画資源所有権を分離
- [ ] RendererからNative API型参照を除去
- [ ] RenderWorldからRHI Commandを生成

### Step 18-B: Static Entity契約

- [ ] `StaticEntityComponent`またはTag Storage
- [ ] YAML Serialize / Deserialize
- [ ] Inspector表示
- [ ] Transform / Parent / Renderer変更Revision
- [ ] Static指定変更時のBatch Dirty化

### Step 18-C: Static State Batching

- [ ] Pipeline / Material / Texture / Meshの永続Resource Key
- [ ] Static Packet Cache
- [ ] Dynamic Packetとの同一Pass提出
- [ ] State変更回数計測

### Step 18-D: Static Instancing

- [ ] Instance Buffer
- [ ] `DrawIndexedInstanced`経路
- [ ] Object ID / Picking対応
- [ ] Shadow / GBuffer対応
- [ ] Spatial Cell単位AABB / Frustum Culling

### Step 18-E: Static Geometry Batching

- [ ] CPU Mesh結合
- [ ] World TransformのVertex Bake
- [ ] Index Offset再構築
- [ ] Batch専用RHI Buffer Upload
- [ ] Material境界によるSub Batch
- [ ] Source EntityとTriangle Rangeの対応

### Step 18-F: Invalidation / 計測

- [ ] Transform / Material / Texture / Entity変更時Rebuild
- [ ] Scene Load / Unload / Undo / Redo / Play / Stop回帰
- [ ] Batch数 / Draw Call数 / Rebuild CPU / Upload CPU計測
- [ ] GPU Pass Time / Memory増加量比較

詳細:

- `Docs/Step18_Static_Entity_Batching_Plan.md`

## Step 19: 描画並列化の再検討

状態: **未着手**

- D3D11 Backend: Graphics Queueへ直列提出
- D3D12 / Vulkan Backend: Command List並列構築
- CapabilityによりAsync Compute / Multiple Queueを選択
- API名による上位層分岐は禁止

## Step 20: Scriptプロジェクト完全ホットリロード対応

状態: **未着手**

目的:

Visual Studioのデバッグ実行を停止せず、Engine Editorの`Build & Reload DLL`操作からScriptプロジェクトのビルド、検証、差し替え、状態復元までを完結させる。
Visual Studio標準のソリューションビルドには依存せず、Editorから外部MSBuildプロセスを起動する。

基本契約:

- Engineがロード中の固定名`Script.dll`を直接上書きしない
- Scriptプロジェクトのビルド成果物と、実際に`LoadLibrary`するRuntime DLLを分離する
- ビルド中もEngineの描画・Editor操作を継続し、メインスレッドを待機させない
- DLLのUnload / LoadはScript実行が停止したフレーム境界のSafe Pointでのみ行う
- ビルド、コピー、ABI検証、Load、状態復元のいずれかが失敗した場合は、可能な限り現在のDLLとScript状態を維持する
- DLL側で生成したObjectはDLL側のDestroy Exportを通して破棄し、DLL境界を跨いだ`new / delete`を禁止する

### Step 20-A: Script Build Service

- [ ] `ScriptBuildService`を追加
- [ ] `vswhere.exe`から使用可能なMSBuildを解決
- [ ] Script `.vcxproj`だけを対象にConfiguration / Platformを明示してビルド
- [ ] `CreateProcessW`による外部MSBuild起動
- [ ] Build Processを非同期監視し、Editor / MainThreadをBlockしない
- [ ] 標準出力・標準エラーをPipeで取得してEngine Consoleへ転送
- [ ] Build成功 / 失敗 / Cancel / Exit Codeを明示的に管理
- [ ] Build時間と最終Build結果を記録
- [ ] 同時Build要求を直列化し、Build中の多重起動を拒否または再要求として集約

### Step 20-B: Editor Build & Reload UI

- [ ] Editorに`Build & Reload DLL`ボタンを追加
- [ ] `Idle / Building / WaitingSafePoint / Reloading / Completed / Failed`状態を表示
- [ ] Build進捗、Build時間、Reload時間、Exit Code、現在ロード中DLL名を表示
- [ ] Build失敗時にエラー概要とConsoleログへの導線を表示
- [ ] Reloadだけを行う操作とBuild込みReloadを内部API上で分離
- [ ] Play / Pause / Stop中の実行可否と挙動を定義

### Step 20-C: Shadow Copy DLL / PDB

- [ ] ビルド成果物の固定名`Script.dll / Script.pdb`を直接ロードしない
- [ ] `Temp/ScriptHotReload/Script_<generation>.dll`へ一意名コピー
- [ ] 対応するPDBも同じgeneration名でコピー
- [ ] コピー完了後にファイルサイズと存在を検証
- [ ] 同じgenerationの上書きを禁止
- [ ] 起動時・終了時・世代数上限超過時の一時ファイルCleanup

### Step 20-D: Safe Reload Pipeline

- [ ] Script Update / Fixed / Drawの新規実行受付を停止
- [ ] 実行中Script Taskと関連Jobの完了を待つ
- [ ] Script状態、登録順、型情報、Entity参照をSnapshot化
- [ ] DLL所有Script Instanceを旧DLLのDestroy Exportで破棄
- [ ] 旧関数ポインタとFactory参照を無効化
- [ ] Shadow Copy済み新DLLを`LoadLibrary`候補として読み込む
- [ ] ABI Version・必須Export・型Registryを検証
- [ ] 新DLLの検証成功後にActive Moduleを切り替える
- [ ] Script Instanceを再生成してSnapshotを復元
- [ ] Script実行順とStart済み状態を復元
- [ ] 次の安全なフレームから実行を再開

Reload Pipeline:

```text
Build Script Project
    -> Build Success
    -> Shadow Copy DLL / PDB
    -> Wait Safe Point
    -> Capture Script State
    -> Quiesce Script Execution
    -> Load and Validate Candidate DLL
    -> Destroy Old Script Instances
    -> Switch Active Module
    -> Recreate Script Instances
    -> Restore State and Order
    -> Resume Script Execution
```

### Step 20-E: Failure / Rollback

- [ ] Build失敗時は現在のDLLとScript Instanceを変更しない
- [ ] Copy失敗時は現在のDLLを維持する
- [ ] Candidate DLLのLoad / ABI / Export検証失敗時は旧DLLを維持する
- [ ] 新DLL切替後のInstance再生成失敗に備えたRollback可能範囲を定義
- [ ] 復元不能なScript型だけを無効化し、Scene全体を破壊しない
- [ ] Reload失敗理由を段階別Error Codeとして記録
- [ ] Reload失敗後も再Build & Reload可能な状態へ戻す

### Step 20-F: Script Type / State Migration

- [ ] Script型追加を検出してRegistryへ登録
- [ ] Script型削除時に該当ComponentをMissing Script状態として保持
- [ ] Script型名称変更にStable Type IDまたはMigration Aliasを導入
- [ ] Serialize / Deserialize対象FieldのVersion管理
- [ ] 追加Fieldへ既定値を適用
- [ ] 削除Fieldを安全に破棄
- [ ] Entity参照を世代付きEntityとして再検証
- [ ] ComponentRefをReload後に再解決し、stale参照を拒否

### Step 20-G: Automatic Build Extension

手動の`Build & Reload DLL`経路を先に完成させた後、自動化を追加する。

- [ ] Scriptソース変更監視
- [ ] File Watch EventのDebounce
- [ ] 保存途中ファイルと連続変更の集約
- [ ] Compile Automatically設定
- [ ] Compile on Save設定
- [ ] 自動Build成功後の自動Reload設定
- [ ] 手動Buildとの競合防止

### Step 20-H: 検証

- [ ] Debug x64で連続Build & Reload試験
- [ ] Release x64で連続Build & Reload試験
- [ ] Visual Studioデバッグ実行中に停止要求なしでBuild可能であることを確認
- [ ] Build失敗後に旧Scriptが継続動作することを確認
- [ ] ABI不一致DLLでRollbackすることを確認
- [ ] Script型追加・削除・名称変更試験
- [ ] Field追加・削除と状態Migration試験
- [ ] Play / Pause / Stop各状態でReload試験
- [ ] Scene切替中・終了処理中のReload拒否試験
- [ ] 100回以上の連続ReloadでModule / Handle / Temp File Leakがないことを確認
- [ ] PDBを含む新DLLへDebugger Symbolが切り替わることを確認

完了条件:

- Visual Studioのデバッグを停止せず、Editorの一操作でScript BuildからReloadまで完了する
- Build失敗またはDLL検証失敗時に、実行中の旧Script環境を破壊しない
- Script状態、実行順、Entity参照がReload後も整合する
- Debug / Release x64で連続Reloadを行ってもModule、Process、Handle、一時ファイルがリークしない

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

# 2.5 横断課題: 安定性・堅牢化（コードレビュー 2026-07 反映）

Step 19-A（GPU Pixel Cost最適化）と並行し、コードレビューで確認した実害候補を優先度順に解消する。
Correctness修正は性能作業より優先する（Step 19-A基本契約と同一方針）。
重大度はレビュー時分類。各項目は対象ファイル・観点・完了条件を持つ。

## H0. 起動時エラー伝播（Critical）

対象: `Engine/engine.cpp:44-70`, `gameApplication.cpp:27-29`, `engine.cpp:131-168`

`Engine::Initialize`はService初期化失敗時にvoidで早期returnするだけで、`GameApplication::Run`は結果を見ずに`Run`へ進む。`Run`内は取得Serviceをnull検証なしで参照するため、部分初期化のままnull `shared_ptr`の`->`で確定Crashする（例: `window->GetMainWindow()->ShouldClose()`）。単一で最も広範なCrash源。

- [x] `Initialize`を`bool`へ変更し各失敗で`false`を返す（`engine.cpp` `FailInitialize`で各Serviceをguard、55-127）
- [x] `Run`側で`Initialize`失敗時はRunへ入らずShutdownして終了コードを返す（`gameApplication.cpp:29` 成功時のみRun、失敗時`exitCode=-1`。`Run`内も全Serviceをnull検証`LogRunFailure` 207-215）
- [x] 失敗Serviceと失敗段階をログへ記録

完了条件: いずれかのService初期化失敗時に、null参照Crashではなく制御された終了になる。 → **達成(2026-07-10検証)**

## H1. Component Mask 64型上限（High）

対象: `Interface/IComponentStorage.h:23`, `Registry/componentRegistry.h:204,295,356,377`（契約は§1.2）

- [x] `AddComponent` / Query / `HasComponent`のMask設定前に`assert(id < MAX_COMPONENTS)`（`IsComponentMaskIndexValid`をRegister 80 / AddComponent 191 / Query 425 / QueryEntities 456で呼出）
- [x] 登録時点でMAX_COMPONENTS超過を検出
- [x] 恒久対応として64→128以上への拡張可否を評価 → **256へ拡張済**（`IComponentStorage.h:23-24` `MAX_COMPONENTS = 256` / `std::bitset<256>`）

完了条件: 型追加で無防備な`terminate`が発生しない。上限接近を早期警告できる。 → **達成(2026-07-10検証)**

## H2. デバイスロスト未処理（High）

対象: `graphicsContext.cpp:905(Present) / 1129(ResizeBuffers)`, `engine.cpp:241(EndFrame)`

`Present` / `ResizeBuffers`のHRESULTを破棄しており`DXGI_ERROR_DEVICE_REMOVED / RESET`を検出しない。Step 19-A.3のInternal Render Size Resizeで`ResizeBuffers`経路を触るため同工程で対応する。

- [x] Present / ResizeBuffersのHRESULT確認（Phase1 2026-07-10。`Present`が戻り値を破棄していたのを`HRESULT`受けに変更。`ResizeBuffers`失敗パスにも判定追加）
- [x] REMOVED / RESET時に`GetDeviceRemovedReason()`をログ（`GraphicsContext::HandleDeviceLostHResult`を新設。`DXGI_ERROR_DEVICE_REMOVED/RESET`判定→`GetDeviceRemovedReason`をhrと共にログ→`m_DeviceLost`フラグを立てる）
- [~] Device / SwapChain / 全RTV / DSV / Timestamp Query Poolを再生成、最低限Graceful終了
    - 【Phase1済】**Graceful終了**: `Engine::Run`メインループが`graphics->IsDeviceLost()`を検出したらログして`break`（無効Device/SwapChain参照によるCrash/黒画面固定を回避）
    - 【Phase2 未】Device/SwapChain/全View/Query Poolの**完全再生成**による復帰は未実装（規模大のため別工程へ分離）
- [ ] Step 19-A.1のPending Query破棄契約と統合（Phase2で対応）

完了条件: TDR / ドライバ更新 / GPU切替でCrashまたは黒画面固定にならない。
→ Phase1(検出+ログ+Graceful終了)実装・**VS Debug x64リビルド成功(2026-07-10)**。制御された終了でCrash/黒画面固定は回避。実デバイスロスト(TDR)発火による実機動作確認とPhase2(完全再生成復帰)は残。

## H3. Script Network null参照（High）

対象: `Scene/Script/GN31.h:112-116`

`gethostbyname`戻り値をnull検証せず`memcpy`。失敗時（オフライン等）にヌルデリファレンスでCrash。IPv4決め打ち・C形式Castも併存。

- [x] null検証を追加（`GN31.h:120` `if (!hostInfo || !hostInfo->h_addr_list || !hostInfo->h_addr_list[0])` を`memcpy`前にguard）
- [ ] `getaddrinfo`移行とIPv4決め打ち解消を検討（現状は`gethostbyname`+IPv4 memcpyのまま。※直近コミットでGN31のgetaddrinfo移行を試行→include衝突でrevert済。要再着手）

## H4. Query反復中の構造変更強制（High）

対象: §1.4に契約追記済。実装タスク。

- [x] Structure VersionをDebug assert化（`ComponentQueryView.h:104` `ValidateStructureVersion()`をbegin/end/`operator++`/deref 75,80,85,94,118で検証）
- [ ] 即時構造変更APIの内部専用化（`componentRegistry.h`の`AddComponent`184 / `RemoveComponent`は`public:`のまま。内部専用化は未）

## M. 中優先

- [ ] M-1 GBuffer UINT4(ObjectID) Clear（`GBufferPass.cpp:218-223`）→ Step 19-A.6で対応。非被覆Pixelへの前FrameID残留でPickが誤ID返却する現状を解消
    - 【部分】前Frame残留は解消（毎Frame共有`clearColor{0,0,0,0}`ループでUINT4スロットもClear）。ただし**無効ID sentinelではなく0**へClearのため、背景PixelがEntity ID=0を返す問題は未解消。専用`ClearRenderTargetView`+sentinel化が残
- [ ] M-2 LLAMA `ResetContext`とWorker Threadのデータ競合（`LLAMAAgent.cpp:963`）。生成ループとResetの相互排他、または中断フラグで安全点まで待つ 【未】
- [x] M-3 EngineContext Shutdown最終破棄の順序（`engineContext.cpp:72-83`）。`Shutdown()`逆順呼出に加え、破棄も`m_serviceOrder.rbegin()`逆順eraseへ変更済（2026-07-10検証）
- [ ] M-4 AddComponent戻り生ポインタ無効化 / 既存時の引数破棄（§1.2）。ComponentRef限定、`Set` / `Replace`分離
    - 【部分】`ComponentRef<T>`型と`SetComponent`/`ReplaceComponent`分離は実装済（`componentRegistry.h:232,242`）。ただし`AddComponent`(184)は依然として生`T*`を返す(227)。ComponentRef限定化が残
- [ ] M-5 定数バッファ毎セッター全体Upload（`graphicsContext.cpp:197-243`）。Pixel Costとは別のDraw Cost軸。Step 17-B計測後にCPU Mirror 1回Upload / DYNAMIC + Map化 【未】(全セッターが毎回`UpdateSubresource`でCB全体Upload)
- [ ] M-6 RenderPass / PhysX userDataの生new/delete（`PlayerPass.cpp:18-40`, `EditorPass`, `physicSystem.cpp:615-1110`）。`unique_ptr`化。Step 19-Aのrender scale Resource再生成と同工程で例外安全化
    - 【部分】RenderPass側は完了（`PlayerPass.cpp`全sub-pass+RTを`make_unique`、Finalizeで`.reset()`）。**PhysX側`physicSystem.cpp`は生`new ActorEntityInfo`(1063,1110)+生`delete`(615,621,694,700,1053,1100)が残**

## L. 低優先（記録のみ・README / 安定版前に一括）

- `Register`戻り値の握り潰し（`engineContext.h:37-49`）
- ImGui / SceneManager Shutdown順の依存確認（`engineContext.cpp:58-59`）
- Entity暗黙`uint32_t`変換・世代無視の整数比較・破棄済みでも`true`な`operator bool`（`Entity/Entity.h:33-64`）
- `EntityRegistry::CreateID`のindex飛ばしによるSlot Leak（`Registry/entityRegistry.h:65-95`）
- `GraphicsContext`のComPtr / 生ポインタ混在（`graphicsContext.h:296-310`）
- RenderPass末尾のOM未アンバインド、パス順序への暗黙依存（`GBufferPass.cpp:562-603`）
- `offsetof` + `reinterpret_cast`マクロのstandard-layout前提（`IComponent.h:159-194`）

## 実施順の指針

1. H0 → 最も広範なCrash源、単独で修正可能。最優先
2. H2 / M-1 / M-6 → Step 19-Aの作業対象と重なるため同工程で
3. H1 / H4 / M-4 → ECSコア。型追加・並列化を進める前に
4. H3 / M-2 / M-3 → 独立、順不同
5. L → README / 安定版前に一括

---

# 3. 現在の作業位置

> 実態同期(2026-07-10コード検証)。項目1〜3は実装済み、Static Batchingは本リストの想定より大幅に先行実装済み。実装順序が当初計画と食い違っている点に注意。

1. [x] Step 17-B.1 Performance MonitorでDraw CPU内訳（`PerformanceMonitor.h` `DrawTimingBreakdown`/`DrawSamples`/`DebugDrawSamples`/`PresentSamples`実装済）※実機での数値確認は別途
2. [x] VSync ON / OFF比較とGPU Frame Time計測（`GPUFrameTimeSamples`/`GpuFrameTimingStatus`、`VSyncEnabled`表示 実装済）
3. [x] IRenderable内部のComponentRegistry参照除去（`IRenderable::Execute`はRenderPacket駆動。`RenderableModel.cpp`はstaleな`#include componentRegistry.h`のみ残存＝実質除去。※includeの掃除が残）
4. [ ] Step 17-C Animation CPU Build / GPU Upload分離 【未着手】
5. [ ] Step 17-D Terrain CPU Mesh Build / GPU Upload分離 【未着手】
6. [ ] Step 17-E Wave CPU Vertex Build / GPU Upload分離 【未着手】
7. [ ] Step 18-A RenderWorld基盤 【未着手】（`RenderWorld`クラス/ファイルは未存在）
8. [~] Step 18-B以降 Static Entity / Static Batching 【大幅先行実装済】RenderPacket再利用の暫定経路で `System/Render/StaticBatch/`(約30ヘッダ: GeometryBinding/InstanceBuffer/DrawSubmission/PipelineResources/PickingContract/ShadowSubmission…), `RenderPacket/StaticBatch*`キャッシュ, `Shader/StaticBatchVS.hlsl`+`StaticBatchGBufferPS.hlsl`, `StaticBatchTelemetryUI.h` を実装済。真のDraw Call統合(RenderWorld+RHI Handle移行後)が残
9. **[~] H2 デバイスロスト未処理** — Phase1(検出+`GetDeviceRemovedReason`ログ+`IsDeviceLost`検出でGraceful終了)実装・VS Debugリビルド成功(2026-07-10)。Phase2(Device/SwapChain/View/Query Pool完全再生成による復帰)とTDR実機確認が残。

Step 17-AのTask命名統一は完了。以降のCapture、Profiler、YAML Export、依存解析では統一後のTask名を基準とする。
Step 17-BのPacket Build / Command Submit分離はSchedule Captureで確認済み。Performance MonitorのDraw全体とRender Scheduleの差をCPU区間、Present待機、GPU時間へ分解してから次の最適化対象を決定する。
Static BatchingはRender PacketのSort / Pass Mask / Transform Snapshotを再利用できるが、真のDraw Call統合はRenderWorldとRHI Resource Handleへの移行後に行う。

性能作業の現行トラックは`Docs/Step19A_GPU_Pixel_Cost_Optimization.md`（画面サイズ比例のPixel Cost削減）であり、Migration Plan上のStep 19「描画並列化の再検討」はその後段（Step 19-A優先度リストの項目9）に位置する。
Step 19-Aの各工程に着手する前後で、§2.5横断課題のうち作業対象と重なる項目（H0起動 / H2 Device Lost / M-1 GBuffer Clear / M-6 RenderPass所有権）を同時に解消する。H0はどの工程よりも先に単独で修正する。

---

# 4. 最終ゴール(プロジェクトビジョン)

本エンジンの最終目標は、Freejam『Robocraft』型の**キューブ&パーツ・ビルド&バトル**ゲームを自作エンジンで作り上げること。
現行の基盤トラック(ECS / Scheduler / RHI / PhysX / Static Batching / Network)は、この最終形へ本質的に直結する土台に相当する（本Migration PlanはロードマップのPhase R0）。

- 詳細と再現ロードマップ(Phase R0〜R6・設計判断): `Docs/Project_Vision_Robocraft_Roadmap.md`
- 当面の指針: Phase R0(基盤)を計画通り進め、特に **H2デバイスロスト**など残Critical/Highを先に解消する。Static BatchingとECSが固まり次第、R1「ブロック構造表現」の小さなプロトタイプ(グリッド上にキューブを並べ1剛体で動かす)から着手すると基盤の穴が早期に見える。
