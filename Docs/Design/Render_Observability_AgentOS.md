# Render Observability / Headless Validation / AgentOS

Status: **Active design direction — develop / 2026-09-27**

旧2026-07-26計画を、ブランチ統合後の現在地に合わせて要約したDesign文書。

原文は `Docs/Archive/Design/Render_Observability_and_AgentOS_Integration_Plan.md` に保存する。

## Goal

人間またはAgentOSが、Rendererの結果を推測ではなく機械可読なEvidenceとして取得できるようにする。

目標ループ:

```
Inspect
  -> Plan
  -> Modify
  -> Build
  -> Launch deterministic run
  -> Capture named render output
  -> Collect logs / metrics / image
  -> Evaluate
  -> Repair or report
```

## Boundary

Render CaptureはAgentOS固有機能にしない。

Engine側が汎用Observability APIを提供し、AgentOSはTool Adapterとして利用する。

```
Renderer / RHI
  -> Render Output Registry
  -> Readback / Diagnostics API
  -> AgentOS Tool Adapter
```

## Required engine-side capabilities

### Named render outputs

最低限、描画結果を名前で取得できるようにする。

例:

- Player Final
- Editor Final
- GBuffer channels
- Shadow atlas
- Lighting output
- Post-effect intermediate

SwapChainだけをcapture対象にしない。

### Readback

RHIへreadback契約を追加し、Backend固有処理を上位から隠す。

D3D11ではstaging resource + GPU完了確認 + Mapを利用する。

### Deterministic validation mode

自動検証では可能な範囲で次を固定できるようにする。

- scene
- viewport size
- timestep
- random seed
- camera
- quality / render settings

## Headless / WARP

完全なwindowless rendererを最初から要求しない。

段階的に:

1. deterministic normal run
2. hidden/minimal window
3. D3D11 WARP validation
4. backend-independent headless path

と進める。

## AgentOS tools

Engine APIが成立した後に、AgentOSへ次のようなToolを公開する。

- `ListRenderOutputs`
- `CaptureRenderOutput`
- `GetRenderDiagnostics`
- `RequestRenderValidation`

AgentOSは画像が取得できていない場合に「取得済み」と扱わない。

## Validation priorities

Visual diffより先にcorrectnessを固定する。

優先順位:

1. resource / device lifetime
2. pass execution
3. output existence / size / format
4. deterministic scene result
5. image comparison
6. performance regression

## Relationship to current architecture

- Current renderer: [Rendering](../Architecture/Rendering.md)
- Future high-level pipeline: [RenderPipeline Graph Design](../Architecture/RenderPipelineGraph_Design.md)
- AgentOS: [AgentOS / B.R.A.I.N.](../AgentOS/README.md)
- Historical GPU optimization records: `Docs/Archive/Steps/`

## Next implementation boundary

最初にAgentOS Toolを増やすのではなく、Renderer側へ小さいOutput Registry / Readback契約を追加する。

そのAPIをEditorの手動captureでも利用できる状態になってからAgentOS Adapterを接続する。
