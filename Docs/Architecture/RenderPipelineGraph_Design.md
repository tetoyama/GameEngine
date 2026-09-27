# RenderPipeline Graph Design

Status: **Canonical design summary — not fully implemented runtime**

この文書は、旧RenderPipeline Graph Architecture / Integration Plan / Execution Order Amendment / Resource & DLL Hot Reload Contractを統合した将来設計。

## Position

RenderPipeline Graphは、Step 16で導入した低水準RHI / RenderGraphを置き換えるものではない。

役割はその上に、

- Pipeline Asset
- Compiler
- Runtime Instance
- Resource Slot Contract
- Render Operation
- Editor-facing graph

を提供すること。

## Main objects

### RenderPipelineGraphAsset

保存可能なNode / Edge / parameter定義。

### CompiledRenderPipeline

Assetを検証・loweringした実行用immutable representation。

### RenderPipelineInstance

View / frameごとのruntime state、history、temporal resourceを持つ。

## Node / Resource

Nodeは名前付きinput/output slotを持つ。

Resource contractでは少なくとも次を区別する。

- Texture
- Buffer
- View / attachment
- external resource
- temporal/history resource

接続時にformat、usage、size policyなどを検証する。

## Operation

想定Operation分類:

- Raster
- Fullscreen
- Compute
- External

Extension側がnative callbackを長期間保持するより、
Host所有のBuiltin Operationへloweringする方式を優先する。

## Execution

Pipeline CompilerがNode DAGを検証し、必要なresource transition / pass orderingを低水準RenderGraphへ落とす。

ECS WorldをGraph Nodeから直接読むことは避け、RenderWorld / DrawListなど明示された入力を利用する。

## Hot reload

DLL extensionを使う場合、compiled pipelineへ裸のDLL function pointerを恒久保持しない。

推奨方式:

1. Extension moduleをload
2. Node definition / operation factoryをregister
3. Compile時にHost-owned operationへlowering
4. old generationのCPU callback完了を待つ
5. extension-owned stateをdestroy
6. DLL unload
7. new generationをcompile

GPU完了待ちは、そのmodule generation固有のGPU resourceを破棄する場合に限定する。

## Implementation order

現在の方針では、先に以下を安定させる。

1. RenderWorld / resource ownership
2. Shadow correctness
3. RHI device lifetime
4. existing PostEffect compatibility
5. minimal Pipeline Instance
6. Asset / Compiler
7. temporal/history
8. editor graph / preview

## Current status

本書は将来設計であり、現行Rendererの説明は [Rendering](Rendering.md) を正とする。

旧4文書は `Docs/Archive/Rendering/RenderPipelineGraph/` に保存する。
