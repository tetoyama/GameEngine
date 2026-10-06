# Rendering

Status: **Canonical current architecture — 2026-10-06**

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

既存Editor UIはD3D11 Backendを利用する。Scene Viewは設定されたD3D11、
またはD3D12 / Vulkanの共通描画経路を利用する。後者はEditor UIへ画像を転送する。
移植用の共通描画経路はD3D12 / Vulkan / Metal Backendを利用できる。


ServiceはDevice generation / lifetime tokenを持ち、
GPU resource ownerが古いDeviceを誤って解放しないためのOwnership Epochを提供する。

Backend抽象化は存在するが、Renderer全体からD3D11依存が完全に消えた状態ではない。

##### Portable rendering

既存RHIにSDL GPU Backendを追加し、同じ描画処理をDirect3D 12 / Vulkan / Metalで使う。
起動元は `Source/main.cpp`、起動・終了は既存 `GameApplication` にまとめる。
Visual Studioは従来のWindows Editorを、CMakeは各OSの描画プレビューをビルドする。
プレビューはScene・ゲーム・Editor全体の移植ではない。Macで本体が動く段階は未達。

#### 既存の責務

- Editorのサービス登録・終了順は従来の `EngineContext`。
- ファイル読み込み・キャッシュは従来の `ResourceService` / Loader。
- モデルGPU Bufferは従来の `ModelGeometryRuntimeStorage`。
- テクスチャGPU資源は `TextureData`。既存DX11 Textureを必要時に選択APIへ転送する。
- `RenderSystem` が既存RenderPacketから描画データを作る。別のScene・Asset管理は追加しない。
- `FrameRenderer` はモデルBufferと材質TextureのHandleを参照する。
  所有するのは自身の描画ターゲット・Shader・Pipeline・更新Bufferだけ。
- Backendは既存Handle / ResourcePool / Queue / Fenceの契約をSDL GPUへ接続する。Computeは未対応。
  Pass依存と論理状態遷移には既存 `RenderGraph` を使う。

#### Editorでの選択

Project Settings → Application → Rendering APIで選択し、保存して再起動する。
Direct3D 11経路は従来通り。Direct3D 12 / Vulkanは不透明な静的モデル、BaseColor Texture、
UV変換、PBR / Unlit、発光、Environment Map、単一Directional Lightに対応する。
Animation・Toon / Custom Shader・Terrain・透過・CSM・Local Light・Post Effect等は未移行。
未対応・未解決の件数をEditor Viewに表示する。

Editor UIはDX11のまま、描画結果を同期Readbackで表示する。性能改善は未達。
旧Object-IDによるViewクリック選択は無効、Hierarchy / Gizmoは既存経路を使う。
既存のMaximum Frame Latency設定はDXGIの表示待ちを制限する。新しい固定Frame数設定は追加しない。
Buffer更新の同期・cyclingはBackendに任せ、Rendererに別のFrame Slotを持たせない。

#### ビルド

Windows / macOSはProject Settings → Buildで選択する。各OSのネイティブ環境、またはCIでビルドする。
WindowsからMacへ直接クロスコンパイルする仕組みではない。

```sh
cmake -DTARGET_PLATFORM=Windows -P cmake/BuildPortable.cmake
cmake -DTARGET_PLATFORM=macOS -P cmake/BuildPortable.cmake
```

CMakeは実行ファイル1つを作る。デモ専用のService登録・Asset Loader・テストターゲットは持たない。
描画プレビューは最小限の起動と三角形の表示だけを行う。
GPU描画の手動確認では `--backend d3d12|vulkan|metal --frames 2 --offscreen --capture image.ppm` を使える。

共通GLSLからSPIR-V / DXIL / MSLを生成する。3形式は各APIで実行するために必要。
生成途中のHLSLは配布しない。Shader再生成はSDKのglslc / spirv-cross / dxcで
CMakeの `PortableShaders` ターゲットを使う。
BRDF計算は既存HLSLとGLSLで `Source/Shader/Material/BRDF.hlsli` を共有する。
SDL3 3.4.18を固定Hashで取得する。ライセンスは `ThirdParty/SDL3-LICENSE.txt`。

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
