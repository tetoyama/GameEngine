# RenderPipeline Graph / Step 18-A Execution Order Amendment

Status: **Normative Amendment v1.2 — 2026-08-02**

対象:

- `refactor/ecs-scheduler-foundation`
- PR #45

関連文書:

- `Docs/Step18A_RenderWorld_Runtime_Ownership_Progress.md`
- `Docs/RenderPipeline_Graph_Architecture.md`
- `Docs/RenderPipeline_Graph_Integration_Plan.md`
- `Docs/RenderPipeline_Graph_Resource_And_DLL_HotReload_Contract.md`

本書は、実装順序、着手ゲート、Pipeline Instance導入時期、DLL Hot Reload実装時期についての規範的な追補である。

既存文書と衝突する場合、以下を優先する。

---

# 1. 修正理由

PR #45の共有Model Geometry Runtime実装とSmoke Testにより、次の依存関係が実証された。

- GPU Resource RuntimeをComponent Pointerから直接同期すると、Runtime StorageへEditor、YAML、ImGui、Inspector系依存が流入する
- RHI HandleだけではResourceを生成したDevice Instanceを識別できない
- Geometry内容の変更をModel identityとVertex / Index Countだけでは検出できない
- Shadow Correctnessを移行前に一度確認するだけでは、後続のGeometry / Command経路変更による回帰を検出できない
- PostProcess互換化はCamera / Viewport固有Runtime Stateを必要とするため、最小`RenderPipelineInstance`より先に着手できない
- DLL Hot Reload全機能をRPG-0〜3へ含めると、Pipeline Graph本体のクリティカルパスが過大になる

したがって、所有権、Snapshot、Revision、最小Instanceを従来計画より前へ移動する。

---

# 2. Step 18-Aの修正版実装順序

Step 18-AからRenderPipeline Graph着手までの順序を次で固定する。

```text
1. 必須Smoke Test修復
2. RHI Device Ownership / Reset / Abandon契約
3. Model Geometry Revision契約
4. RenderPacket Resource Snapshot / Handle境界
5. 共有RHI Geometry Runtime安定化
6. Shadow Baseline Gate
7. Legacy Build / Submit / Compatibility Facade削除
8. 通常Renderable Legacy Native Geometry Fallback撤去
9. ModelData Legacy Native Geometry生成・破棄撤去
10. RenderWorld -> RHI Command生成境界
11. Shadow Parity Gate
12. RPG-0 Contract Alignment
```

## 2.1 必須Smoke Test修復

次をすべて緑化する。

- Windows Debug / Release x64 Build
- RenderWorld Foundation
- RenderWorld Extraction
- Model Renderer GPU Runtime
- D3D11 Static Batch Interop
- Static Batch Foundation
- RHI Smoke
- Lighting Diagnostic Contract

Smoke修復では、単にLink対象を増やして依存を隠さない。

Runtime境界がEditor / YAML / ImGui依存を引き込んだ場合は、Snapshot、Handle、Descriptorへ分離する。

## 2.2 RHI Device Ownership契約

共有Geometry Runtimeをさらに移行する前に、次を固定する。

- RHI Handleを生成したDevice Instanceの識別方法
- Device再生成時の旧Handle無効化
- 旧Device Resourceを新Deviceへ渡さないこと
- `Reset`がResource破棄成功を確認すること
- `Abandon`をDevice破棄済みと証明できる経路だけに限定すること
- Release失敗時にEntryとTelemetryを成功扱いしないこと

`Synchronize`開始時に所有Deviceを無条件上書きする構造は禁止する。

## 2.3 Model Geometry Revision契約

`ModelData` identity、SubMesh数、Vertex / Index CountだけをGeometry一致条件にしない。

次のいずれかを必須とする。

- `ModelData::geometryRevision`
- Immutable ModelDataの差し替え
- Content Hash / Source Revision

Runtime Entryは生成時Revisionを保持し、Revision変更時だけトランザクション再生成する。

## 2.4 RenderPacket Resource Snapshot / Handle境界

RenderPacketのComponent Pointerは段階的に縮小する。

初期対象:

- Model Resource
- Material Resource
- Texture Resource
- Scene / View identity

原則:

- Runtime StorageはComponentを直接includeしない
- Extraction時に必要ResourceをFrame-local PacketへSnapshotする
- Asset lifetimeはFrame終端まで所有する
- Component PointerはLegacy Submit互換に限定する
- Snapshot / Handle移行後にSchedulerのComponent Read Hazardを削除する

PR #45では第一段階として、Model PacketへFrame-owned `ModelData`参照を保持し、共有Geometry同期Taskの`ModelRendererComponent`直接依存を撤去する。

## 2.5 Shadow Baseline Gate

所有権・描画経路撤去前に、正常なShadow結果を記録する。

対象:

- CSM 4 Cascade
- Point Shadow 6 Face
- Spot Shadow
- Cascade境界
- 後段CascadeだけにCasterが存在する配置
- 高層Casterが手前Cascade Near面より前にある配置
- Resize後
- Scene Reload後

保存するもの:

- 実機画像
- Scene / Camera / Light条件
- Shader / Pipeline Revision
- GPU Timing
- D3D11 Debug Layer出力

## 2.6 Shadow Parity Gate

Legacy Geometry撤去とRenderWorld -> RHI Command境界接続後に、同じ条件で再検証する。

Baseline取得済みであっても、Parity Gateを通過するまでShadow Correctness完了とは扱わない。

許容差を超えた場合、RenderPipeline Graph着手を停止する。

---

# 3. RenderPipeline Graphの修正版順序

従来のRPG-4〜7順序を次へ変更する。

```text
RPG-0  Contract Alignment
RPG-1  Asset / ID / Registry Foundation
RPG-2  Compiler / Validation
RPG-3  RenderOperation / Existing RenderGraph Lowering
RPG-7A Minimal Pipeline Instance
RPG-4  Existing PostProcess Compatibility
RPG-5  Legacy RenderPass Adapter
RPG-6  Prepare Event / DrawList / RenderView
RPG-7B Advanced History / Runtime State / Hot Reload State
RPG-8  BuiltinPipeline / Editor Integration
RPG-9  Node Preview / Render Observability
RPG-10 Native Operation Migration / Optimization
```

## 3.1 RPG-7A Minimal Pipeline Instance

RPG-4より前に実装する。

最低限の責務:

- Instance Key: Scene Context + Camera Entity + Viewport Scope
- Asset Revision / Compiled Revision
- Camera / Viewport固有Parameter Storage
- Node Runtime State Container
- History Resource Ownership境界
- Resolution変更Invalidation
- Camera削除 / Scene Reload Invalidaton
- Pipeline切替時Reset

RPG-7Aでは高度なState MigrationやDLL Reloadを実装しない。

完了条件:

- Game View、Editor View、Preview、複数CameraのStateが混線しない
- PostProcess AdapterがCamera Component内部へRuntime Stateを戻さない
- Scene ReloadとResizeで旧Stateを参照しない

## 3.2 RPG-4 Existing PostProcess Compatibility

RPG-7Aを前提とする。

既存`CameraPostEffect` Runtime Storageは`RenderPipelineInstance`へ接続する。

Temporal Nodeを含まない場合でも、Camera / Viewport identityをInstance Keyとして使用する。

## 3.3 RPG-7B Advanced History / Runtime State

RPG-4〜6の契約が安定した後に実装する。

対象:

- Previous / Current Frame Edge
- Camera Cut検出
- Temporal History Migration
- Exposure / TAA等のState
- Extension Runtime State Record
- DLL State Serialize / Migrate / Deserialize
- Instance Lifetime Telemetry

---

# 4. RenderGraph工程の名称修正

Architecture文書の「Phase 5: RenderGraph」は、低水準RenderGraphの新規再実装を意味しない。

正式名称を次とする。

```text
Phase 5: Existing RenderGraph Gap Completion / Operation Lowering
```

責務:

- `RenderOperation`から既存Step 16-F RenderGraphへのLowering
- 不足しているResource Version / Hazard APIの補完
- ExternalOperation隔離
- Pass Culling Root
- Required / Final State変換

高水準`RenderPipelineGraph`がBarrier、Physical Resource Lifetime、Queue同期を重複実装しない。

---

# 5. DLL Hot Reloadの段階分離

## 5.1 RPG-0〜3で固定する契約

前半では次だけを必須とする。

- DLL定義C++ ObjectをCompiled Pipelineへ保持しない
- Host-owned Descriptor / Payload
- Operation Type Key / Type Version
- Registry Revision
- C ABI Function Table
- Host Allocator境界
- Builtin OperationへのLowering可能性
- Missing Operation Placeholder

この段階では実際のDLL Unload / ReloadをRuntimeへ接続しない。

## 5.2 RPG-7Bまたは独立RPG-HRで実装するもの

- Shadow Copy Candidate DLL
- Module Generation
- Module Lease
- Candidate Registry
- Affected Pipeline再Compile
- Frame Boundary Atomic Swap
- Runtime State Migration
- Rollback
- Old Module Lease残存診断

RenderPipeline Graphの基本描画が成立する前にHot Reload実行基盤をクリティカルパスへ入れない。

---

# 6. 修正版依存グラフ

```text
必須Smoke緑化
    ↓
RHI Device Ownership / Geometry Revision
    ↓
RenderPacket Resource Snapshot / Handle
    ↓
共有Geometry Runtime安定化
    ↓
Shadow Baseline Gate
    ↓
Legacy Facade / Native Geometry撤去
    ↓
RenderWorld -> RHI Command境界
    ↓
Shadow Parity Gate
    ↓
RPG-0 -> RPG-1 -> RPG-2 -> RPG-3
                              ↓
                         RPG-7A Instance
                              ↓
                         RPG-4 PostProcess
                              ↓
                         RPG-5 Legacy Adapter
                              ↓
                         RPG-6 Prepare
                              ↓
                         RPG-7B Advanced State
                              ↓
                         RPG-8 -> RPG-9 -> RPG-10
```

並行可能:

- Asset Schema / Compiler純粋CPU TestとShadow実機検証
- Output Naming / Observability契約とRPG-2
- Editor Node UIとRPG-3以降のCompiler Contract

先行禁止:

- Device Ownership未確定状態でのLegacy Native Geometry撤去
- Geometry Revision未導入状態でのRuntime Cache恒久化
- Shadow Baseline未取得状態でのLegacy描画経路削除
- RPG-7A未完了状態でのPostProcess Runtime移行
- Operation Lowering未確定状態でのPrepare Event自由化
- 基本Pipeline未成立状態でのDLL Reload Runtime接続

---

# 7. 更新後の直近Action

```text
[x] D3D11 Static Batch Interop SmokeのVS 2026 ComPtr互換修正
[x] RenderWorld Extraction SmokeのComponentType counter定義
[x] RenderWorld Foundation Smokeの空Worker Span修正
[x] Model PacketへFrame-owned ModelData Snapshot追加
[x] Model Geometry Runtime StorageのModelRendererComponent依存撤去
[x] Model Geometry Synchronize TaskのComponent Hazard撤去
[x] Model Geometry Runtime SmokeをSnapshot経路へ更新
[ ] 最新CIで必須Smokeを再確認
[ ] RHI Device Ownership / Reset / Abandon契約を実装
[ ] Model Geometry Revisionを実装
[ ] Shadow Baselineを取得
[ ] Legacy Build / Submit / Facadeを削除
```

---

# 8. Runtime着手ゲート

RenderPipeline Graph Runtime実装は次をすべて満たすまで開始しない。

- [ ] PR #45の必須Smokeが緑
- [ ] Windows Debug / Release x64 Build成功
- [ ] RHI Device Ownership契約がSmokeで固定済み
- [ ] Model Geometry RevisionがSmokeで固定済み
- [ ] RenderPacket Model Resource Snapshot経路が安定
- [ ] Player / Editor主要Scene描画確認
- [ ] Shadow Baseline取得済み
- [ ] Legacy撤去後Shadow Parity確認済み
- [ ] Resize / Play / Stop / Scene ReloadのResource Lifetime確認
- [ ] RenderWorld -> RHI Command生成境界が定義済み

このゲート通過後、最初のRuntime子PRを`RPG-0 Contract Alignment`とする。
