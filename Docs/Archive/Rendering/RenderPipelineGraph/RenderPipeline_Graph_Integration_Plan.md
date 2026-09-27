# RenderPipeline Graph Integration Plan

Status: **Planned — 2026-08-02**

対象PR: **#45 `refactor/ecs-scheduler-foundation`**

設計本文:

- `Docs/RenderPipeline_Graph_Architecture.md`

関連計画:

- `Docs/ECS_Scheduler_Migration_Plan.md`
- `Docs/Step16_RHI_MultiBackend_Architecture.md`
- `Docs/Step18A_RenderWorld_Runtime_Ownership_Progress.md`
- `Docs/Step18_Static_Entity_Batching_Plan.md`
- `Docs/Step19A_GPU_Pixel_Cost_Optimization.md`
- `Docs/Render_Observability_Headless_AgentOS_Integration_Plan.md`（PR #55）

---

# 1. 計画上の位置付け

RenderPipeline Graphは、既存Step 16-Fの低水準`RenderGraph`を置き換えるものではない。

```text
ECS World
    ↓ Step 18-A Extraction
RenderWorld / SceneRenderSnapshot
    ↓
RenderPipelineGraphAsset
    ↓ Pipeline Compiler
CompiledRenderPipeline
    ↓ Camera / Viewport Instance
RenderPipelineInstance
    ↓ Frame Prepare
RenderOperation[]
    ↓ Lowering
既存 Step 16-F RenderGraph
    ↓
RHI Command List / Queue
    ↓
D3D11 Backend
```

責務境界:

| 層 | 責務 |
|---|---|
| RenderPipeline Graph | 高水準Node、Asset、接続、Semantic、Preview、Legacy Adapter |
| Compiled Pipeline | Node展開、Slot解決、型検証、安定順序、エラー |
| Pipeline Instance | Camera固有History、動的Parameter、実行Revision |
| RenderOperation | Raster / Fullscreen / Compute / Copy / Externalの論理命令 |
| Step 16-F RenderGraph | Resource Version、Hazard、Barrier、Lifetime、Pass Culling、Queue同期 |
| RHI | Backend非依存Resource / View / Pipeline / Command契約 |
| D3D11 Backend | Native Resource、Immediate Context、実BindとDraw/Dispatch |

Migration Plan上では、Step 18配下へ次を追加する。

```text
Step 18-G: RenderPipeline Graph Foundation
    RPG-0 Contract Alignment
    RPG-1 Asset / ID / Registry
    RPG-2 Compiler / Validation
    RPG-3 Operation Lowering
    RPG-4 PostProcess Compatibility
    RPG-5 Legacy Pass Adapter
    RPG-6 Prepare Event / DrawList / RenderView
    RPG-7 Pipeline Instance / History
    RPG-8 Builtin Pipeline / Editor Integration
    RPG-9 Preview / Observability
    RPG-10 Native Operation Migration / Optimization
```

Step 19の描画並列化再検討は、`RenderOperation`生成とRHI Command生成境界が安定した後に行う。

---

# 2. 現在のPR #45へ入れる範囲

本更新でPR #45へ組み込むもの:

- RenderPipeline Graphの正式設計文書
- 現在のRenderWorld / RHI / RenderGraphとの責務境界
- 実装タスク分解
- 依存関係と完了ゲート
- Legacy移行順序
- CI / 実機検証方針
- PR分割方針

この更新だけではRuntimeコードを変更しない。

理由:

- PR #45の最新HEADではRenderWorld / Static Batch / Model Geometryの一部Smokeが未修復
- Step 19-A Shadow Correctnessの実機固定が未完了
- Legacy RendererとRHI Rendererが混在している
- この状態で高水準Graph実行を接続すると、不具合原因がGraph、Legacy Pass、Resource Lifetimeのどこにあるか分離できなくなる

Runtime着手ゲート:

- [ ] PR #45の必須Smokeを緑化
- [ ] Windows Debug / Release x64 Build成功
- [ ] Player / Editor Viewの主要Scene描画確認
- [ ] Point / Spot / CSM Shadow Correctness確認
- [ ] Resize / Play / Stop / Scene ReloadのResource Lifetime確認
- [ ] RenderWorld Extractionと通常Renderable RHI Geometry経路の安定化

---

# 3. 現行設計との整合修正

設計の思想は維持する。ただし現在のRHI契約へ組み込む際、次を実装上の必須補正とする。

## 3.1 高水準層で`DXGI_FORMAT`を保持しない

Architecture本文の例では`DXGI_FORMAT`を使用しているが、実装ではRHIのBackend非依存Formatを使用する。

```cpp
struct TextureContract
{
    RHITextureDimension dimension;
    RHITextureUsage usage;
    RHITextureFormat format;
    ColorSpace colorSpace;
    TextureSemantic semantic;
    ResolutionConstraint resolution;
    SampleConstraint samples;
    bool requiresMipmaps = false;
};
```

`DXGI_FORMAT`への変換はD3D11 Backend内だけで行う。

## 3.2 高水準Graphと低水準RenderGraphを別型にする

名前衝突を避けるため、実装上は次を明確に分ける。

```text
RenderPipelineGraphAsset / CompiledRenderPipeline
    高水準

rhi::RenderGraph または既存RenderGraph
    低水準
```

高水準Nodeの接続を、そのままGPU Passとして扱わない。

## 3.3 `RenderPipelineInstance`はTransient物理Resourceを所有しない

Instanceが所有するもの:

- History Resource Handle
- ExposureなどのPersistent State
- Camera / Viewport固有Parameter
- Node Runtime State
- Compile / Asset Revision

低水準RenderGraphが所有するもの:

- Frame内Transient Logical Resource
- Physical Resource Allocation
- Lifetime / Alias候補
- Barrier / State Tracking

## 3.4 Node Assetは実行可能Objectを直接保存しない

Assetには型名、Version、Parameter、Slot、接続を保存する。

複雑なOperation展開はRegistry上のCompilerへ委譲する。

```cpp
class IRenderPipelineNodeCompiler
{
public:
    virtual ~IRenderPipelineNodeCompiler() = default;

    virtual void Validate(
        const RenderPipelineNodeAsset& node,
        RenderPipelineValidationContext& context) const = 0;

    virtual void Compile(
        const RenderPipelineNodeAsset& node,
        RenderPipelineCompileContext& context) const = 0;
};
```

AssetへC++関数ポインタ、`shared_ptr<IRenderPass>`、Native Resourceを保存しない。

## 3.5 Stable IDの完全識別子

永続Node IDはGraphローカル単調増加`uint64_t`を維持する。

Runtimeでの完全識別は文字列連結ではなく構造体を使用する。

```cpp
struct RenderNodeInstanceId
{
    RenderPipelineAssetHandle asset;
    SubgraphInstancePath path;
    RenderNodeId nodeId;
};
```

表示時だけ次の形式へ変換する。

```text
MainGraph/PostProcess:8/Node:3
```

## 3.6 ReadWriteの扱い

高水準Pipeline Graphでは、同一Resource VersionへのReadWriteを禁止する。

```text
Color v0 -> Fog -> Color v1
```

低水準RenderGraphでは、Backend実装上必要なUAV ReadWriteを引き続き表現可能とする。ただしNode Compilerが明示的にLoweringした内部Operationだけに限定する。

## 3.7 ExternalOperationの隔離

`ExternalOperation`はNative Contextを任意操作できる抜け道にしない。

最低限宣言するもの:

- Read Resource
- Write Resource
- Required State
- Final State
- Viewport / Scissor影響
- Pipeline / IA / OM State影響
- Preview Policy
- MainThread制約
- Resource Lifetime制約

Opaque Legacy Passの前後では保守的に次を行う。

- Pass Culling禁止
- Physical Resource再利用禁止
- State Cache無効化または再同期
- 宣言対象Resourceの競合解除
- Debug時の未宣言Bind検出

## 3.8 Editor Overlayは単純な最終PostProcessではない

Grid / Gizmo / OutlineはFinalColor後段へ注入できる。

一方、Object ID、Selection Mask、SceneDepth依存OverlayはGeometryまたは専用Raster Operationを必要とする。

したがってOverlay Decoratorは、Pipeline終端だけでなく標準SemanticへOperationを注入できるCompiler Decoratorとして扱う。

## 3.9 BuiltinPipelineは再帰不能なFallbackとする

BuiltinPipelineは通常Assetと同じCompilerを通すが、次を保証する。

- Engine内蔵定義から生成
- 最小Node型だけを使用
- User Asset参照なし
- Subgraph外部参照なし
- Compile失敗時はHard Error
- Builtin自身からBuiltinへFallbackしない

## 3.10 Determinism

Prepare EventのCustom PredicateやOperation生成は、保存不能なラムダを直接保持しない。

- Registry Key
- Type Version
- Serializable Parameter
- Stable Scene Snapshot Input

から決定的に生成する。

Frame Prepare結果の安定順序は次で固定する。

```text
Pipeline Domain
Node Instance Path
RenderNodeId
Operation Index
Stable Draw Sequence
```

---

# 4. 依存グラフ

```text
PR #45 CI / Smoke修復
    ↓
Step 19-A Shadow Correctness固定
    ↓
Step 18-A RenderWorld / Runtime Ownership完了
    ├───────────────┐
    ↓               ↓
RPG-0 Contract      Render Observability Output Naming
    ↓               │
RPG-1 Asset         │
    ↓               │
RPG-2 Compiler      │
    ↓               │
RPG-3 Operation Lowering -> Step 16-F RenderGraph
    ↓
RPG-4 PostProcess Compatibility
    ↓
RPG-5 Legacy Pass Adapter
    ↓
RPG-6 Prepare Event / DrawList / RenderView
    ↓
RPG-7 Instance / History
    ↓
RPG-8 Builtin / Editor
    ↓
RPG-9 Preview / Capture
    ↓
RPG-10 Legacy撤去 / Resource Pool / Optimization
    ↓
Step 19 Command List並列構築再検討
```

Parallel可能な作業:

- Asset SchemaとCompiler純粋CPU TestはShadow実機確認と並行可能
- Editor Node UIはCompiler Contract確定後に並行可能
- Render ObservabilityのStable Output Name設計はRPG-2と並行可能

禁止する先行:

- Legacy Pass Adapter完成前のPlayerPass全面置換
- Shadow Correctness固定前のVisual Baseline確定
- Output Semantic確定前のNode Preview専用Texture API
- Operation Lowering確定前のPrepare Event自由化

---

# 5. タスク分解

## RPG-0: Contract Alignment

目的:

既存RHI、RenderGraph、RenderWorld、PostEffect、RenderPassの境界を固定する。

タスク:

- [ ] 現行`PostProcessNode`、`CameraPostEffect`、`PostEffectPass`の保存形式とRuntime所有権を一覧化
- [ ] 現行`IRenderPass`、`RenderPassContext`、PlayerPass、EditorPassのResource Read / Writeを一覧化
- [ ] Step 16-F RenderGraphの公開APIと不足機能をGap Analysis
- [ ] `RHITextureFormat`、ColorSpace、Semantic、Resolution Contractを定義
- [ ] `RenderResourceType`をRHI Resource Typeへ対応付け
- [ ] `RenderOperation`から既存RenderGraph PassへのLowering契約を定義
- [ ] `SceneRenderSnapshot`と`RenderWorld`の入力契約を定義
- [ ] Node Registry / Event RegistryのThread Safety契約を定義
- [ ] Architecture Contract Smoke Testを追加

完了条件:

- 高水準層に`ID3D11*`、`DXGI_FORMAT`、Immediate Contextが現れない
- RenderPipeline GraphとStep 16-F RenderGraphの責務が重複しない
- 既存Rendererの全主要PassについてDeclared / Partial / Opaque分類が存在する

## RPG-1: Asset / ID / Registry Foundation

タスク:

- [ ] `RenderPipelineGraphAsset`
- [ ] Graphローカル単調増加`RenderNodeId`
- [ ] `nextNodeId`のYAML保存
- [ ] Node / Edge / Slot / Parameter Schema
- [ ] Node Type Version
- [ ] Unknown Node Placeholder
- [ ] Unknown Prepare Event Placeholder
- [ ] Subgraph Asset Reference
- [ ] Graph複製時のID Remap
- [ ] Undo / Redo用Command境界
- [ ] Node Type Registry
- [ ] Asset Schema Migration Registry

検証:

- [ ] 削除IDを再利用しない
- [ ] Save / Load後にIDと接続が一致
- [ ] Unknown型のRaw YAMLが失われない
- [ ] Subgraph複製時に外部接続だけ正しくRemap
- [ ] 不正`nextNodeId`を最大ID+1へ明示Migration

## RPG-2: Compiler / Validation

タスク:

- [ ] `CompiledRenderPipeline`
- [ ] `RenderNodeHandle { index, generation }`
- [ ] Slot Index解決
- [ ] Subgraph Instance Path展開
- [ ] Resource Producer / Consumer Table
- [ ] 必須入力検証
- [ ] Type / Semantic / ColorSpace / Format / Resolution / Sample検証
- [ ] Current Frame Cycle検出
- [ ] History Edge分離
- [ ] Stable Topological Sort
- [ ] Required Output Semantic検証
- [ ] Structured Compile Diagnostic
- [ ] Asset RevisionとCompiled Cache Key
- [ ] Node Compiler Registry Snapshot

Compiler Error形式:

```cpp
struct RenderPipelineDiagnostic
{
    DiagnosticSeverity severity;
    RenderPipelineAssetHandle asset;
    SubgraphInstancePath path;
    RenderNodeId nodeId;
    std::optional<ResourceSlotId> slotId;
    std::string code;
    std::string message;
};
```

完了条件:

- 不正Graphは実行経路へ入らない
- 同じAsset Revisionから同じCompiled順序を生成する
- EditorがNode / Slot単位で具体的なErrorを表示できる

## RPG-3: RenderOperation / RenderGraph Lowering

タスク:

- [ ] `ClearOperation`
- [ ] `RasterOperation`
- [ ] `FullscreenOperation`
- [ ] `ComputeOperation`
- [ ] `CopyOperation`
- [ ] `ResolveOperation`
- [ ] `GenerateMipsOperation`
- [ ] `ExternalOperation`
- [ ] Logical Resource Version割当
- [ ] Output Resource Descriptor解決
- [ ] Operation Read / WriteをStep 16-F RenderGraphへ登録
- [ ] Required State変換
- [ ] RHI Pipeline / View / Sampler Binding Descriptor
- [ ] Stable Operation Name
- [ ] Pass Culling Root指定
- [ ] Standard Pipeline Outputsの公開

検証:

- [ ] `Color v0 -> Fog -> Color v1`でSRV / RTV同時Bindが発生しない
- [ ] 未使用OperationがCullingされる
- [ ] External Output / Present RootはCullingされない
- [ ] D3D11 Debug LayerでResource Hazardが出ない
- [ ] Null BackendでOperation Lowering Testが通る

## RPG-4: Existing PostProcess Compatibility

タスク:

- [ ] 既存PostProcess Node Asset Adapter
- [ ] Shader配列からFullscreenOperation列への変換
- [ ] 既存Node Parameterと新Parameter Sourceの対応
- [ ] `resolutionScale`をResolution Policyへ移行
- [ ] 既存YAMLをSchema Migration
- [ ] 既存PostProcess GraphをSubgraphとして公開
- [ ] CameraPostEffect Runtime StorageをPipeline Instanceへ接続
- [ ] Preview Handle互換層

完了条件:

- 既存Scene YAMLを変更なしまたは自動Migrationでロード可能
- Bloom、SSAO、SSR、DOF、Outlineなど主要Nodeの描画差分が許容範囲内
- Player / Editor Camera切替でHistoryやRuntimeが混線しない

## RPG-5: Legacy RenderPass Adapter

タスク:

- [ ] `LegacyRenderPassNodeCompiler`
- [ ] `ExternalOperation` Adapter
- [ ] Shadow / GBuffer / Lighting / Forward / PostEffect / Overlay UIのResource宣言
- [ ] Declared / Partially Declared / Opaque分類
- [ ] Opaque Barrier / State Cache再同期
- [ ] PlayerPass固定順序をBuiltin Graphとして再現
- [ ] EditorPass固定順序をDecorator込みで再現
- [ ] Legacy Pass単位Telemetry

初期Graph:

```text
Shadow Legacy Node
    ↓ ShadowDepth
GBuffer Legacy Node
    ↓ GBuffer + SceneDepth
Lighting Legacy Node
    ↓ HDRScene
Transparent Legacy Node
    ↓ HDRScene v1
PostProcess Subgraph
    ↓ DisplayColor
UI Legacy Node
    ↓ FinalColor
Present Node
```

完了条件:

- Node化前後でPass順序と描画結果を変更しない
- Legacy Passを一つずつ無効化して依存Errorを確認できる
- Opaque Passが存在する間は危険なResource Aliasを行わない

## RPG-6: Prepare Event / DrawList / RenderView

タスク:

- [ ] `IRenderNodePrepareEvent`
- [ ] Prepare Event Registry
- [ ] Capability Bitset
- [ ] `RenderNodePrepareContext`
- [ ] `DrawListBuilder`
- [ ] `NodeParameterWriter`
- [ ] `RenderViewBuilder`
- [ ] `RenderOperationBuilder`
- [ ] Node専用DrawList Storage
- [ ] DrawList Slot入出力
- [ ] RenderView Slot入出力
- [ ] Engine Semantic Parameter Resolver
- [ ] MainThread / AnyWorker Schedule分類
- [ ] Capability違反のDebug検出
- [ ] Deterministic Operation Build順

初期Prepare Event:

1. `BuildOpaqueDrawList`
2. `BuildTransparentDrawList`
3. `BuildShadowDrawList`
4. `BuildShadowViews`
5. `BuildCameraView`
6. `ResolveEngineParameters`

完了条件:

- EventからScene Componentを変更できない
- EventからNative Graphics Contextへ到達できない
- DrawList、View、Parameter以外の副作用がない
- 同入力から同じDrawList / Operation順を生成する

## RPG-7: Pipeline Instance / Temporal History

タスク:

- [ ] `RenderPipelineInstance`
- [ ] Instance Key: Scene Context + Camera Entity + Viewport Scope
- [ ] History Resource Registry
- [ ] Previous / Current Frame Edge
- [ ] Camera Cut検出
- [ ] Resolution変更Invalidation
- [ ] Scene Context失効Invalidation
- [ ] Asset Revision変更時のState Migration / Reset
- [ ] Exposure / TAAなどのNode Runtime State
- [ ] Instance Lifetime Telemetry

完了条件:

- Game View、Editor View、Node Preview、複数CameraでHistoryを共有しない
- Scene Reload / Camera削除時にHistory Resourceが解放される
- Resize直後に古い解像度のHistoryを読まない

## RPG-8: BuiltinPipeline / Editor Integration

タスク:

- [ ] Engine内蔵BuiltinPipeline定義
- [ ] Camera Pipeline Asset参照
- [ ] `EditorViewSource`
- [ ] `EditorPipelineSource`
- [ ] `EditorViewportRenderSettings`
- [ ] Pipeline Compile失敗時Fallback
- [ ] Fallback理由のEditor表示
- [ ] Standard Output Semantic
- [ ] Editor Overlay Compiler Decorator
- [ ] Grid / Gizmo / Selection Mask / Outline / ImGui注入
- [ ] Depth / ObjectId欠落時の機能縮退
- [ ] Pipeline切替時のInstance / History処理

完了条件:

- Editor Camera + Builtin
- Editor Camera + Scene Camera Pipeline
- Scene Camera + Builtin
- Scene Camera + Scene Camera Pipeline

の4組合せが動作する。

## RPG-9: Node Preview / Render Observability

依存:

- RPG-2 Compiler
- RPG-3 Lowering
- RPG-7 Instance
- Render Observability Output Registry

タスク:

- [ ] Output Preview
- [ ] Isolated Preview
- [ ] Pipeline Preview
- [ ] Debug View
- [ ] Upstream Dependency Slice Compile
- [ ] Live Frame Input
- [ ] Captured Frame Input
- [ ] Default / Custom Texture Input
- [ ] Preview Policy
- [ ] Side Effect NodeのCache / Fallback / Skip
- [ ] Named Render Output公開
- [ ] Preview TextureのLifetime / Pin契約

完了条件:

- Preview専用描画実装を持たない
- 本番Node Compilerと同じOperation定義を使用する
- Preview終了後にTransient Resourceを保持しない

## RPG-10: Legacy Operation Migration / Optimization

移行順:

1. PostProcess
2. Deferred Lighting
3. GBuffer
4. Shadow
5. Transparent
6. UI / Effekseer / External Renderer

各Pass共通タスク:

- [ ] Legacy Resource Access列挙
- [ ] Native API型をRHI Handleへ移行
- [ ] Prepare / Operation / Execute責務分離
- [ ] ExternalOperationから正式Operationへ移行
- [ ] Legacy Adapter削除
- [ ] Visual Regression
- [ ] GPU Timing比較
- [ ] Resource Lifetime確認

最適化:

- [ ] Transient Resource Pool
- [ ] Physical Texture再利用
- [ ] Unused Pass Culling拡張
- [ ] Compiled Pipeline Cache
- [ ] Shader Reflection Cache
- [ ] Parameter Upload Batch
- [ ] DrawList Cache / Revision
- [ ] GPU時間Node表示
- [ ] Resource Lifetime Debug View

---

# 6. PR分割方針

PR #45へ全Runtime実装を直接積まない。

推奨子PR:

| PR | 内容 | Base |
|---|---|---|
| RPG-0 | Contract / Type / Architecture Tests | `refactor/ecs-scheduler-foundation` |
| RPG-1 | Asset / ID / YAML / Registry | RPG-0 |
| RPG-2 | Compiler / Validation | RPG-1 |
| RPG-3 | Operation / RenderGraph Lowering | RPG-2 |
| RPG-4 | PostProcess Adapter | RPG-3 |
| RPG-5 | Legacy RenderPass Adapter / Builtin parity | RPG-4 |
| RPG-6 | Prepare Event / DrawList / RenderView | RPG-5 |
| RPG-7 | Instance / History | RPG-6 |
| RPG-8 | Editor Pipeline Selection / Overlay | RPG-7 |
| RPG-9 | Preview / Observability | RPG-8 + Observability branch |
| RPG-10 | Pass別Native移行 | RPG-9 |

各PRは次を満たす。

- 単一責務
- 既存Player / Editor描画を壊さない
- Null Backendまたは純粋CPU Smoke Testを持つ
- Windows Debug / Release x64を通す
- Runtime変更時は実機確認項目をPR本文へ記録

---

# 7. CI計画

追加するWorkflow群:

```text
RenderPipeline Asset Contract
RenderPipeline Compiler Contract
RenderPipeline Operation Lowering Contract
RenderPipeline PostProcess Compatibility
RenderPipeline Legacy Pass Boundary
RenderPipeline Prepare Capability
RenderPipeline Instance History
RenderPipeline Builtin Fallback
RenderPipeline Editor Selection
RenderPipeline Preview Slice
```

最低検証:

- Stable Node ID
- YAML Round Trip
- Unknown Type Preservation
- Slot Type Validation
- Cycle Detection
- History Edge Validation
- Stable Topological Sort
- Resource Versioning
- ExternalOperation Isolation
- Capability Violation
- Camera / Viewport Instance Isolation
- Builtin Fallback
- Legacy Pass順序Parity

既存必須Workflow:

- Windows Build
- RHI Smoke Test
- RenderWorld Foundation
- Model Renderer GPU Runtime
- Static Batch Foundation
- Lighting Diagnostic Contract
- GPU Pass Timing Contract

---

# 8. 実機検証マトリクス

## View / Pipeline

| View | Pipeline | 必須確認 |
|---|---|---|
| Player | Builtin | 現行Game View parity |
| Player | Custom | Node接続とFallback |
| Editor Camera | Builtin | 通常編集安定性 |
| Editor Camera | Scene Pipeline | 自由視点で同じ画作り |
| Scene Camera | Builtin | 構図だけ標準描画 |
| Scene Camera | Scene Pipeline | Game View parity |
| Node Preview | Dependency Slice | 本番定義再利用 |

## Resource Lifetime

- Resize
- Minimize / Restore
- Play / Stop
- Scene Reload
- Additive Scene Load / Unload
- Camera追加 / 削除
- Pipeline Asset Reload
- Shader Recompile
- Device Lost / Recovery

## Visual

- Static Model GBuffer / Shadow
- GPU Skinning
- CPU Skinning fallback
- Transparent
- Terrain
- Wave
- Particle
- Effekseer
- UI
- Environment Map
- Bloom / SSAO / SSR / DOF / Fog / Outline

---

# 9. 最初に着手する具体的タスク

PR #45の次の優先順位を次で固定する。

```text
1. 現在赤いSmoke Testを修復
2. Shadow Correctness実機確認
3. Step 18-A Legacy Build / Submit / Facade削除
4. 通常Renderable Legacy Native Geometry fallback撤去
5. RenderWorld -> RHI Command生成境界
6. RPG-0 Contract Alignment
7. RPG-1 Asset / ID / Registry
8. RPG-2 Compiler / Validation
9. RPG-3 Operation Lowering
10. RPG-4 PostProcess Adapter
```

最初の実装PRは`RPG-0 Contract Alignment`とする。

`RPG-0`では描画順やShader結果を変更せず、型、境界、Validation Testだけを追加する。

---

# 10. 完了定義

RenderPipeline Graph全体の完了条件:

- [ ] 既存PostProcess Node Assetが移行または互換ロードできる
- [ ] Shadow、GBuffer、Lighting、Transparent、PostProcess、UIがNodeとして表現される
- [ ] Node接続から実行順を導出する
- [ ] 同一Resource更新がVersion化される
- [ ] Step 16-F RenderGraphがBarrier / Lifetime / Cullingを一元管理する
- [ ] Prepare Eventが専用Context外へ副作用を出さない
- [ ] Camera / ViewportごとのHistoryが分離される
- [ ] Node Previewが本番Node定義を再利用する
- [ ] BuiltinPipelineが常に利用可能
- [ ] Editor Viewの視点とPipelineを独立選択できる
- [ ] Editor OverlayがPipeline Semanticへ安全に注入される
- [ ] Legacy Passが段階的に正式Operationへ移行される
- [ ] 上位層からD3D11 Native API型が消える
- [ ] Debug Layer Resource Hazardがない
- [ ] Windows Debug / Release x64と主要実機Sceneが回帰しない
