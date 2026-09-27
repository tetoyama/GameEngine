# Rendering

Status: **Canonical current architecture — 2026-09-27**

## Frame path

現在の主要描画フローは概ね次の順。

```
Shadow
  -> GBuffer
  -> Deferred Lighting
  -> Post Effect
  -> Runtime / Editor UI composition
```

Player ViewとEditor Viewは共通Renderer基盤を利用しつつ、最終合成先とEditor overlayを分ける。

## RenderSystem

`RenderSystem` はECSから描画に必要な情報を抽出し、次の処理を管理する。

- animation pose calculation / upload
- model geometry runtime synchronization
- render packet build
- view culling
- render packet submission
- render pass execution

## RenderWorld

`RenderWorld` はRenderer側のFrame-localな中間状態。

主に次を保持する。

- RenderPacket frame buffer
- Culling visibility
- StaticBatch candidate / cache / instance data
- frame generation
- submission state

ECS WorldのComponent storageをRenderPassから任意に読む構造を減らすための境界として扱う。

## RenderPacket

RenderPacketは描画対象をPassへ渡すためのCPU-side representation。

Model / Mesh / Sprite / Terrain / WaveなどのRenderable情報を、View CullingやStaticBatchへ接続する。

## Culling

CullingはBounds更新とView判定を分離する。

Viewごとにvisibilityを構築し、RenderPacket submission時に参照する。

Cullingの現在契約は [ECS and Scheduling](ECS_and_Scheduling.md) と本書を正とする。

## StaticBatch

StaticBatchは次の段階を持つ。

- candidate collection
- resource / geometry key grouping
- instance data
- visible instance selection
- GPU instance buffer
- GBuffer submission
- Shadow submission

細かな実装クラス単位の契約はCanonical仕様にしない。

## RHI

`RenderHardwareInterfaceService` はBackend RegistryとDeviceを所有する。

現在はD3D11 Backendが実用Backend。

ServiceはDevice generation / lifetime tokenを持ち、
GPU resource ownerが古いDeviceを誤って解放しないためのOwnership Epochを提供する。

Backend抽象化は存在するが、Renderer全体からD3D11依存が完全に消えた状態ではない。

## Shadow

主なShadow系:

- Cascaded Shadow Map
- Point light 6-face shadow
- Spot / local light shadow

Atlas samplingではtile境界越えを避けるため、half-texel safe rangeを考慮する。

Point face mapping、local-light projection、shadow participationなどの純粋契約は自動Smoke Testで維持する。

過去のCSM acne / sample cost / bias調査はArchiveに残す。

## Post effect

Post effectはNode化された処理を持ち、現行Renderer上で利用される。

さらに上位のRenderPipeline Graph構想は、現在Rendererを即座に置き換えるものではない。

将来設計は [RenderPipeline Graph Design](RenderPipelineGraph_Design.md)。

## Runtime text / 2D

Runtime UI / textはゲーム向け表示とEditor ImGuiを分離する。

MiniGameCollectionではPlayer-facing UIをDirect2D経路で合成する。

ElemenTactics向けに `RuntimeTextComponent` / `RuntimeTextSystem` も存在する。

## Validation

自動CIは「すべてのRenderer内部クラス」ではなく、壊れた時に発見が難しい小さい契約を優先する。

GPU実機・WARP・StaticBatch詳細・RenderGraph詳細は必要時の手動/診断テストとして保持する。
