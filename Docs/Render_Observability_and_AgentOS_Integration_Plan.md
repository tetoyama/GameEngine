# Render Observability / Headless Validation / AgentOS Integration Plan

Status: **Design v1 — 2026-07-26**

対象ブランチ:

- Renderer / ECS / Scheduler / RHI基盤: `refactor/ecs-scheduler-foundation`
- AgentOS信頼性改善: `llm-agent-2` 系
- 本計画更新: `agent/update-render-agentos-roadmap`

関連計画:

- `Docs/ECS_Scheduler_Migration_Plan.md`
- `Docs/Step19A_GPU_Pixel_Cost_Optimization.md`
- `Docs/StepH2_Device_Lost_Recovery_Design.md`
- AgentOS側 `Docs/AgentOS/00_Architecture.md`
- AgentOS側 `Docs/AgentOS/01_Phase_Plan.md`

---

# 1. 目的

Renderer刷新、AgentOS開発、GitHub Actionsによる遠隔検証を、相互依存を壊さず一つの開発順序へ統合する。

最終的には、AIまたは人間が次の閉ループを安全に実行できる状態を目指す。

```text
Inspect source / scene / runtime state
    -> Plan
    -> Modify
    -> Build
    -> Launch deterministic engine run
    -> Capture named render outputs
    -> Collect logs / metrics / images
    -> Evaluate
    -> Repair or report
```

今回の主眼は、最終段の自律変更そのものではなく、その前提となる**描画結果を決定的かつ機械可読に観測できるエンジン基盤**を確立することにある。

---

# 2. 現状認識

現在の開発は三つの独立トラックに分かれている。

```text
A. Renderer Foundation
   refactor/ecs-scheduler-foundation
   ECS / Scheduler / RenderPacket / RHI / RenderGraph / Shadow / GPU Cost

B. AgentOS Reliability
   llm-agent-2 系
   Planning / Tool Routing / Evidence / Critic / Repair / Conversation Context

C. Editor Experience
   agent/modern-imgui-wrapper
   Editor UI / Interaction / B.R.A.I.N. presentation
```

Rendererトラックでは、RHI契約とRenderGraphは先行している一方、RenderWorld基盤、Native Resource所有権分離、Device Lost完全復帰、Pixel Cost最適化が未完了である。

AgentOSトラックでは、実行制御と調査経路を改善中であり、RendererのNative APIへ直接依存させる段階ではない。

したがって、`CaptureFrame`をAgentOSへ直接実装するのではなく、次の順序を強制する。

```text
Renderer correctness / RHI boundary
    -> Render observability API
    -> Headless deterministic runner
    -> GitHub Actions capture
    -> AgentOS adapter
    -> Visual / performance regression
    -> Autonomous modification loop
```

---

# 3. 基本判断

## 3.1 Render CaptureはAgentOS機能ではない

画像取得、GPU Readback、PNG保存、Render Target列挙はエンジンの汎用機能として実装する。

AgentOSは完成済みのAPIを`EngineTools`から呼ぶだけとする。

禁止:

- AgentOSから`ID3D11Device` / `ID3D11Texture2D`を直接参照する
- AgentOS内へWIC / DirectXTex保存処理を置く
- `GraphicsContext`の内部Texture名をTool側が知る
- SwapChain BackBufferをWindow Screenshotとして取得する

許可:

```text
AgentOS CaptureFrame Tool
    -> IRenderCaptureService
        -> IRenderOutputRegistry
        -> IRHIReadbackService
        -> Image Encoder
```

## 3.2 Capture対象はSwapChainではなく名前付きRender Output

Window、ImGui Dock、Editor Viewサイズへ依存した画面キャプチャを基盤にしない。

最低限、次を名前付きOutputとして公開する。

```text
FinalColor
SceneColorBeforePost
GBuffer.Albedo
GBuffer.Normal
GBuffer.Depth
GBuffer.Material
GBuffer.Emissive
GBuffer.Parameter
Lighting.Diffuse
Lighting.Specular
Shadow.Atlas
PostProcess.Input
PostProcess.Output
EditorView.FinalColor
PlayerView.FinalColor
```

実際に存在しないOutputは空画像ではなく`Unavailable`を返す。

## 3.3 Backend差はCapabilityで処理する

上位層で`if (D3D11)`や`if (WARP)`を行わない。

必要なCapability例:

```cpp
struct RenderObservabilityCapabilities {
    bool supportsOffscreenRendering;
    bool supportsTextureReadback;
    bool supportsHeadlessDevice;
    bool supportsSoftwareRasterizer;
    bool supportsGpuTimestamp;
    bool supportsHdrReadback;
};
```

D3D11 BackendはHardware AdapterまたはWARPを選択できるようにする。

## 3.4 Visual TestよりCorrectnessを優先する

Step 19-AのShadow Correctness、Device Lost、Render Target Lifetime、Pass順序の暗黙依存を先に解消する。

壊れたRendererを自動撮影しても、誤った基準画像を固定するだけになる。

---

# 4. 既存Migration Planへの位置付け

本工程は、既存Step番号を全面変更せず、概念上の**Step 18.5: Render Observability / Headless Validation**として扱う。

```text
Step 16   Multi-Backend RHI
Step 17   Task分割 / Render Packet
Step 18-A RenderWorld基盤
Step 18.5 Render Observability / Headless Validation  <- 本計画
Step 18-B〜F Static Batching完成
Step 19-A Pixel Cost / Shadow Correctness
Step 19   描画並列化再検討
```

ただし、実作業は完全な直列ではない。

- Output命名契約とReadback抽象はStep 16完了後に設計可能
- RenderWorld由来のOutput公開はStep 18-Aに依存
- WARP / Offscreen起動はSwapChain分離とDevice選択に依存
- Visual Regressionの基準画像確定はStep 19-AのCorrectness安定後
- AgentOS統合はAgentOS Tool Routing / Evidence契約安定後

---

# 5. 改訂ロードマップ

## Phase R0: 現行RendererのCorrectness固定

基点: `refactor/ecs-scheduler-foundation`

優先対象:

- Step 19-AのShadow Correctness
- Point / Spot / CSM境界の実機検証
- GBuffer / Lighting / PostEffectの回帰確認
- Device Lost Phase 2aの設計適合
- Resize時Resource Lifetime
- GPU Timestamp QueryのPending破棄
- RenderPass末尾Stateの明示化

完了ゲート:

- Debug / Release x64ビルド成功
- Player View / Editor Viewの主要Sceneが描画可能
- Shadow設定変更で既知の破綻がない
- Resize / Play / Stop / Scene切替でDevice Resourceが不整合にならない
- GPU Pass計測がCPU Stallを発生させない

このPhaseではAgentOS画像取得を実装しない。

## Phase R1: RenderWorld / Output境界確立

依存: Step 16 RHI、Step 17 Render Packet、Step 18-A

### R1-A: Render Output Registry

追加候補:

```cpp
using RenderOutputId = uint64_t;

struct RenderOutputDescriptor {
    RenderOutputId id;
    std::string stableName;
    TextureHandle texture;
    TextureFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t mipLevels;
    uint32_t arrayLayers;
    bool transient;
    bool captureAllowed;
};

class IRenderOutputRegistry {
public:
    virtual std::vector<RenderOutputDescriptor> ListOutputs() const = 0;
    virtual std::optional<RenderOutputDescriptor> FindOutput(
        std::string_view stableName) const = 0;
};
```

契約:

- Stable NameはBackend Native名ではなくRenderer上位概念
- Frameを越えてTexture Handleを無条件保持しない
- Transient ResourceはCapture要求時にPass CullingされないようPinする
- Outputの生成元Passと最終Write Frameを記録する
- 同名Outputの複数Viewは`PlayerView` / `EditorView`などScopeで区別する

### R1-B: Present Target分離

```text
Render Scene
    -> FinalColor Texture
    -> Optional Upscale / Composite
    -> Present Target or Offscreen Sink
```

完了条件:

- `Present()`を行わずFinalColorまで描画できる
- SwapChain ImageはOutputの一種であり、唯一の最終描画先ではない
- Editor UIなしでもPlayer FinalColorが生成される
- Resize対象をDisplay SizeとInternal Render Sizeで分離する

## Phase R2: Render Readback / Image Artifact

依存: R1

### R2-A: RHI Readback契約

```cpp
struct TextureReadbackRequest {
    TextureHandle source;
    uint32_t mipLevel = 0;
    uint32_t arrayLayer = 0;
    PixelConversion conversion;
};

struct TextureReadbackResult {
    std::vector<std::byte> pixels;
    uint32_t width;
    uint32_t height;
    uint32_t rowPitch;
    ImagePixelFormat format;
};
```

D3D11 BackendではStaging Textureへ`CopyResource`または`CopySubresourceRegion`し、GPU完了後に`Map(D3D11_MAP_READ)`する。

禁止:

- GPU完了待ちを無制限Busy Waitする
- 毎Frame自動Readbackする
- Render Thread外からImmediate Contextを無同期利用する
- MSAA / Typeless / Depth Textureを暗黙変換する

### R2-B: Image Encoder

初期対応:

- PNG: LDR Color / Normal可視化 / ID可視化
- EXRまたはHDR保存: 将来対応
- JSON Manifest: 必須

Capture結果:

```text
Capture/<capture-id>/
  manifest.json
  FinalColor.png
  GBuffer.Albedo.png
  GBuffer.Normal.png
  GBuffer.Depth.png
  Shadow.Atlas.png
  diagnostics.json
  engine.log
```

Manifest最低項目:

```json
{
  "schemaVersion": 1,
  "captureId": "...",
  "commit": "...",
  "branch": "...",
  "backend": "D3D11",
  "adapter": "WARP",
  "scene": "Asset/...",
  "camera": "MainCamera",
  "frame": 30,
  "fixedDeltaTime": 0.0166666667,
  "seed": 12345,
  "internalSize": [1280, 720],
  "outputs": [],
  "warnings": [],
  "errors": []
}
```

## Phase R3: Headless / WARP Execution

依存: R1、R2

### R3-A: 起動モード

CLI例:

```text
GameEngine.exe
  --mode capture
  --backend d3d11
  --adapter warp
  --scene Asset/Scene/VisualTest.scene
  --camera MainCamera
  --width 1280
  --height 720
  --warmup-frames 30
  --capture-frame 31
  --capture-output FinalColor
  --capture-output GBuffer.Normal
  --output-dir Capture/result
  --fixed-delta 0.0166666667
  --random-seed 12345
  --no-present
  --no-editor-ui
```

### R3-B: Device選択

```cpp
enum class AdapterPreference {
    Default,
    HighPerformance,
    MinimumPower,
    Warp,
    SpecificLuid
};
```

契約:

- GitHub-hosted Windows RunnerではWARPを明示選択する
- ローカルではHardwareを既定とする
- Hardware Device作成失敗時の暗黙WARP fallbackは設定で制御する
- CIではD3D Debug Layerが存在することを前提にしない
- WARP非対応機能はCapability Errorとして報告する

### R3-C: Window依存の扱い

初期実装は完全なOS非依存プロセスを必須としない。

許容:

- Message-onlyまたは非表示WindowをDevice初期化補助に使用
- SwapChainを作らずOffscreen Textureへ描画

禁止:

- Window ScreenshotをCapture結果とする
- Desktop解像度やDPIへCapture解像度を依存させる
- ImGui Main Viewportの存在をPlayer描画前提にする

## Phase R4: Deterministic Runtime Validation

依存: R3

追加:

- `TimeService` Fixed Delta Mode
- Engine全体Random Seed注入
- Input Recording / Replay
- Scene Load Complete判定
- Asset Streaming / Shader Compile完了判定
- Warm-up Frame
- Capture Frame指定
- 最初の差異Frame報告

決定論の扱い:

- PhysXを含む完全Bit Determinismは前提にしない
- 同一環境での再現率を計測する
- 差異が出た場合は最初のFrame、Entity、Component、Outputを報告する
- Temporal EffectはWarm-upと許容誤差を個別設定する

完了ゲート:

- 同一Commit / Scene / Seedで連続5回Captureし、FinalColor差分が閾値内
- 固定DeltaがRuntime Logへ記録される
- Scene Load前のCaptureを拒否する
- Shader Compile失敗を黒画像成功として扱わない

## Phase R5: GitHub Actions Capture

依存: R3、最低限のR4

Workflow責務:

```text
Checkout
  -> Build Debug or Release x64
  -> Launch WARP offscreen capture
  -> Validate manifest and expected outputs
  -> Upload images / JSON / log as Artifact
```

初期Workflow:

- `workflow_dispatch`
- PR Labelまたは手動実行のみ
- Capture Sceneを1つに限定
- 失敗時もArtifactをUpload
- Artifact retentionを明示
- Secretやローカル絶対PathをManifestへ出さない

成功条件:

- Process Exit Code 0
- `manifest.json`が存在
- requested outputがすべて`Captured`
- 画像Width / Heightが要求値と一致
- Engine Error件数が許容値以下
- WARP DeviceであることをManifestから確認

このPhaseでは画像内容の自動合否はまだ必須にしない。

## Phase A0: AgentOS Reliability完成

基点: `llm-agent-2` 系

Rendererトラックと並行して以下を完了させる。

- Tool CatalogをPlannerへ正確に供給
- Critic追加調査へTool Catalogを供給
- Code Search / Reference Search / Runtime Toolの決定的ルーティング
- Unsatisfied Evidence処理
- Request Revision / Repair再計画
- Conversation Context Isolation
- Main Thread Tool Dispatch
- Artifact Provenance

完了ゲート:

- 存在しないToolを生成しない
- Runtime観測とCode観測を混同しない
- Tool失敗を成功Evidenceとして扱わない
- 追加調査要求が実Commandへ変換される
- Artifact URI / Commit / FrameをEvidenceへ保持できる

このPhaseではD3D11 APIをAgentOSへ追加しない。

## Phase A1: AgentOS Render Tool Adapter

依存: R2、R3、A0

統合ブランチ候補:

```text
integration/agentos-render-observability
```

Tool候補:

### `ListRenderOutputs`

入力:

```json
{}
```

出力:

- Stable Name
- Format
- Size
- Availability
- Last Writer Pass
- Capture Capability

### `CaptureRenderOutput`

入力:

```json
{
  "output": "PlayerView.FinalColor",
  "width": 1280,
  "height": 720,
  "frame": 31,
  "format": "png"
}
```

出力:

- Artifact URI
- Manifest URI
- Capture ID
- Provenance
- Warning / Error

### `GetRenderDiagnostics`

出力:

- Active Backend / Adapter
- Device Lost State
- Pass Timing
- Render Size
- Executed / Culled Pass
- Shader Compile Error
- Resource Validation Error

### `RequestRemoteRenderCapture`

GitHub Actionsを起動する外部Toolとして扱い、Engine内部Toolとは分離する。

AgentOSはWorkflowの開始、状態取得、Artifact取得を別Commandとして扱う。

## Phase A2: Visual / Performance Regression

依存: R4、R5、A1

### Visual Regression

段階導入:

1. Artifact生成のみ
2. Baselineとの画像差分生成
3. Pass別差分
4. 閾値判定
5. Critic Evidence化

比較指標:

- Exact pixel一致は既定にしない
- Mean Absolute Error
- Max Error
- Changed Pixel Ratio
- SSIM等は補助指標
- Depth / Normal / IDはOutput別閾値を持つ

Baseline契約:

- Baseline更新はHuman Approval必須
- Commit / Scene / Renderer設定 / Adapter種別を固定
- Hardware BaselineとWARP Baselineを混在させない
- Shadow Correctness未確定時にBaselineを更新しない

### Performance Regression

- WARPはGPU性能比較に使用しない
- WARPで検出するのはCrash、描画欠落、Shader失敗、極端なCPU時間増加
- GPU Pass性能Baselineは固定実機またはSelf-hosted GPU Runnerで取得
- GitHub-hosted WARP結果をRTX実機性能と比較しない

## Phase A3: Autonomous Development Loop

依存: A2

```text
Task
  -> Inspect
  -> Plan
  -> Human-approved modification scope
  -> Apply patch
  -> Compile
  -> Unit / smoke tests
  -> Headless run
  -> Capture artifacts
  -> Critic
  -> Repair or final report
```

安全条件:

- 変更前Git Snapshot
- 変更ファイル数上限
- Build / Test timeout
- Capture timeout
- Baseline更新禁止
- Human Approval Gate
- Rollback可能
- 同じ失敗を無限再試行しない

---

# 6. ブランチ戦略

## 6.1 計画更新

```text
base: refactor/ecs-scheduler-foundation
head: agent/update-render-agentos-roadmap
```

本ブランチは文書のみとする。

## 6.2 Renderer Observability実装

```text
base: refactor/ecs-scheduler-foundation
head: feature/render-observability
```

PRを次の単位へ分割する。

1. Render Output Registry契約
2. Offscreen FinalColor / Present分離
3. RHI Texture Readback
4. PNG + Manifest
5. WARP / Capture CLI
6. Deterministic warm-up / fixed delta / seed
7. GitHub Actions Artifact

PR #45本体へすべて直接追加しない。

## 6.3 AgentOS実装

```text
base: llm-agent-2
head: agent/<reliability-fix>
```

Renderer API完成まではMock Tool Contractだけを許可する。

## 6.4 統合

```text
base: 更新済みAgentOS基盤
head: integration/agentos-render-observability
```

Renderer側Commitを先に統合し、その公開APIへAgentOS Adapterを接続する。

AgentOS実装をRenderer Foundationへ逆流させない。

---

# 7. PR境界と禁止事項

## 一つのPRへ混ぜない組み合わせ

- Shadow Correctness修正 + PNG Encoder
- RenderWorld移行 + AgentOS Tool追加
- WARP Device選択 + Visual Baseline大量追加
- Fixed Delta導入 + PhysX挙動修正
- Render Target Format最適化 + Baseline更新
- Editor UI刷新 + Headless起動

## 一つのPRにまとめてよい組み合わせ

- Readback Interface + Null Backend Contract Test
- D3D11 Staging Readback +専用Smoke Test
- Capture Manifest + Schema Validation
- CLI Parser + Capture Request Validation
- GitHub Actions Workflow + Artifact Contract Test

---

# 8. 観測データ契約

Captureは画像だけで完了としない。

必須Artifact:

- Manifest JSON
- Engine Log
- Requested Render Output
- Backend / Adapter情報
- Scene / Camera / Frame
- Fixed Delta / Seed
- Pass Execution一覧
- Shader Compile結果
- Device Lost状態

推奨Artifact:

- GPU / CPU Timing
- RenderGraph Pass一覧
- Resource Format / Size
- Warning / Error集計
- Entity / Component Snapshot参照

Evidence Provenance例:

```json
{
  "sourceType": "render_capture",
  "sourceUri": "github-actions://run/.../artifact/...",
  "commit": "...",
  "scene": "...",
  "camera": "...",
  "frame": 31,
  "output": "PlayerView.FinalColor",
  "backend": "D3D11",
  "adapter": "WARP"
}
```

---

# 9. テスト計画

## Contract Tests

- Output Stable Name重複拒否
- stale Texture Handle拒否
- capture不可Transient Output拒否
- 不明Outputで`Unavailable`
- Null BackendでAPI契約のみ検証

## D3D11 Readback Smoke

- 既知色TextureをReadback
- Row Pitch差を処理
- RGBA / BGRA変換
- Depth可視化
- UINT ID可視化
- Device Lost時中断

## Headless Smoke

- WARP Device作成
- SwapChainなし起動
- Scene Load
- 1 Frame描画
- PNG出力
- 正常終了

## Determinism Smoke

- 同一Seedで5回Capture
- 同一Output Size
- Manifest差分が許可項目だけ
- 画像差分が閾値内

## Workflow Smoke

- 手動Dispatch
- Build失敗Artifact
- Runtime失敗Artifact
- Capture成功Artifact
- Artifact内Schema検証

## AgentOS Integration Smoke

- `ListRenderOutputs`
- `CaptureRenderOutput`
- ArtifactをEvidenceへ登録
- Tool失敗時にCritic Hard Fail
- 未取得画像を取得済みと報告しない

---

# 10. 完了定義

## Render Observability v1

- D3D11 HardwareとWARPの双方でOffscreen描画可能
- `PlayerView.FinalColor`をPNG保存可能
- ManifestとEngine Logを同時出力
- SwapChain / ImGuiなしでCapture可能
- 不明OutputやShader失敗を成功扱いしない
- GitHub Actions Artifactとして取得可能

## AgentOS Integration v1

- AgentOSがNative Graphics APIを参照しない
- Stable Output NameからCapture要求可能
- ArtifactをProvenance付きEvidenceとして利用可能
- Criticが画像取得失敗を検出可能
- Capture結果に基づく報告が再現可能

## Autonomous Validation v1

- 固定Scene / Camera / Seed / DeltaでCapture可能
- Baselineとの差分Artifactを生成可能
- Visual RegressionとPerformance Regressionを区別
- Human ApprovalなしにBaselineを更新しない
- 失敗時に変更前状態へRollback可能

---

# 11. 直近の実施順

現在の最優先順は次とする。

1. PR #45のShadow Correctness / Step 19-A実機検証を継続
2. Step 18-A RenderWorldの公開境界を確定
3. `IRenderOutputRegistry`設計とNull Backend Contract Test
4. FinalColorとPresentの分離
5. D3D11 Readback + PNG + Manifest
6. WARP Offscreen CLI
7. GitHub Actions手動Capture
8. AgentOS Reliability完了
9. AgentOS Adapter統合
10. Fixed Delta / Seed / Replay
11. Visual Regression
12. Autonomous Modify-Build-Observe-Repair

次の実装着手点は、AgentOS Toolではなく**Renderer側の`IRenderOutputRegistry`とPresent Target分離**である。
