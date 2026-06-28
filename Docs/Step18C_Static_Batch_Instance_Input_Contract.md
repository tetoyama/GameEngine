# Step 18-C Static Batch Instance Input Contract

## 目的

Step 18-BでGPUへUploadした`StaticBatchInstanceData`を、Backend非依存Pipeline Input Layoutと専用Shaderへ接続する。

この段階では既存GBuffer描画を置き換えない。Vertex / Instance Input契約、Shader Compile、代表Packet解決までを先に固定する。

## Vertex Buffer Layout

既存`VERTEX_3D`はSlot 0へ入力する。

| Semantic | Format | Offset | Slot | Classification |
|---|---:|---:|---:|---|
| POSITION0 | RGB32_Float | 0 | 0 | Per Vertex |
| NORMAL0 | RGB32_Float | 12 | 0 | Per Vertex |
| TANGENT0 | RGB32_Float | 24 | 0 | Per Vertex |
| COLOR0 | RGBA32_Float | 36 | 0 | Per Vertex |
| TEXCOORD0 | RG32_Float | 52 | 0 | Per Vertex |

`sizeof(VERTEX_3D) == 60`をCompile Timeで固定する。

RHIに`Format::RGB32_Float`を追加し、D3D11では`DXGI_FORMAT_R32G32B32_FLOAT`へ変換する。

## Instance Buffer Layout

`StaticBatchInstanceData`はSlot 1へ入力する。

| Semantic | Format | Offset | Slot | Step Rate |
|---|---:|---:|---:|---:|
| INSTANCEWORLD0 | RGBA32_Float | 0 | 1 | 1 |
| INSTANCEWORLD1 | RGBA32_Float | 16 | 1 | 1 |
| INSTANCEWORLD2 | RGBA32_Float | 32 | 1 | 1 |
| INSTANCEWORLD3 | RGBA32_Float | 48 | 1 | 1 |
| INSTANCEOBJECT0 | RGBA32_UInt | 64 | 1 | 1 |

`INSTANCEOBJECT0`の内容:

```text
x = Entity Index
y = Entity Generation
z = Scene Context ID
w = Reserved
```

`sizeof(StaticBatchInstanceData) == 80`をCompile Timeで固定する。

## C++ Layout Builder

`StaticBatchInstanceInputLayout.h`が次を提供する。

- `AppendVertexElements`
- `AppendInstanceElements`
- `BuildInstanceOnly`
- `BuildFull`

`BuildFull`はSlot 0 / Slot 1を合わせた10要素の`RHI::InputElementDesc`配列を返す。

## Shader Contract

専用Shader:

- `StaticBatchVS.hlsl`
- `StaticBatchGBufferPS.hlsl`

Vertex ShaderはInstance World Matrixを使用し、通常のObject Constant BufferにあるWorld Matrixへ依存しない。

Pixel Shaderは`INSTANCEOBJECT0`からScene ID / Object IDを取得し、GBuffer Parameterへ次を出力する。

```text
x = Scene Context ID
y = Entity Index
z = Shader ID
w = Material Flags
```

Entity GenerationはGPU Picking結果をCPU側で検証するためInstance Dataへ保持するが、現在のGBuffer Parameterには直接出力しない。

## Static Group Binding

`StaticBatchPacketCacheEntry`と`StaticBatchInstanceGroup`へ`representativePacketIndex`を追加した。

代表Packetから将来次を解決する。

- Geometry Binding
- Material Binding
- Texture Binding
- Pipeline / Render Layer
- Index Count

代表Packet Indexは同じFrameのPublished Packet配列を参照する。範囲外の場合はStatic Drawを行わず、通常Packet描画へFallbackする。

## Correctness Policy

現段階でStatic Groupを実描画へ移行する条件:

- Kindが直接Geometryを持つ`Mesh`
- Groupの全Packetが同じViewで可視
- GBuffer Pass対象
- 不透明Material
- Representative Packetが有効
- CPU Instance DataがValid
- GPU Instance Buffer Upload成功
- Pipeline / Geometry Binding成功

一つでも満たさない場合、Group全体を通常RenderPacket経路へ残す。

複数SubMeshを内包する`Model`は、SubMesh単位Resource KeyとPacket分割が完了するまでInstanced Draw対象外とする。

## 検証

- `StaticBatchInstanceInputLayoutSmokeTest.cpp`
  - Vertex / Instance Offset
  - Format
  - Slot
  - Per-Vertex / Per-Instance分類
  - Step Rate
  - Struct Size
- Static Batch WorkflowのShader Contract Job
  - `StaticBatchVS.hlsl`を`vs_5_0`でCompile
  - `StaticBatchGBufferPS.hlsl`を`ps_5_0`でCompile
- FrameBuffer Integration Test
  - Representative Packet Index
  - Geometry Binding Pointer
  - Instance Range

## 完了済み

- [x] RHI `RGB32_Float`
- [x] D3D11 RGB32変換
- [x] 完全なSlot 0 / Slot 1 Input Layout
- [x] Static Batch専用Vertex Shader
- [x] Static Batch専用GBuffer Pixel Shader
- [x] Instance Object情報のNo-Interpolation転送
- [x] Representative Packet Index
- [x] C++ Layout Smoke Test
- [x] FXC Shader Compile Job

## 未完了

- [ ] RHI Shader / Pipeline State Resource所有
- [ ] Native Mesh BufferのRHI Geometry Binding
- [ ] Group内Packet可視性検証
- [ ] GBuffer Static Draw提出
- [ ] 成功Groupだけ通常Packet提出から除外
- [ ] Model SubMesh単位Packet / Resource Key
- [ ] Shadow Shader / Pass
- [ ] Picking Generation検証

## 次工程

1. Static Batch専用RHI Pipeline Resource所有クラス
2. D3D11既存Mesh BufferのInterop
3. Mesh Group可視性検証
4. GBuffer `DrawIndexedInstanced`
5. 成功Groupの通常Packet除外
