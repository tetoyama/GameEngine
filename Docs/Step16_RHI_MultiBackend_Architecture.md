# Step 16: Multi-Backend RHI Architecture

## 目的

RHIは既存Direct3D 11描画コードを置き換えるための薄いWrapperではない。

RendererからGraphics API固有概念を隔離し、同一のRenderWorld / RenderGraph / Material / Shader管理層を次のBackendで利用可能にする。

- Direct3D 11
- Direct3D 12
- Vulkan
- Null / Test Backend

## 層構造

```text
Renderer / RenderWorld / RenderGraph
                |
                v
       Backend-independent RHI
  Handle / Descriptor / Device / Queue
  CommandList / Pipeline / RenderPass
                |
       +--------+--------+
       |        |        |
     D3D11    D3D12    Vulkan
     Backend  Backend   Backend
```

上位層は`ID3D11Device`、`ID3D12Device`、`VkDevice`などを参照しない。
Native Objectは各Backendディレクトリ内だけで保持する。

## Backend選択

`IRHIBackend`がAPI単位のFactoryを表す。

```cpp
std::unique_ptr<IRHIBackend> backend =
    BackendRegistry::Instance().Create(config.backend);

std::unique_ptr<IRHIDevice> device =
    backend->CreateDevice(createDesc);
```

起動設定からBackendを選び、Renderer本体は選択されたAPIを意識しない。

## Device Capability

API名だけで分岐しない。

Rendererは`DeviceCapabilities`を参照し、利用可能な機能に応じて経路を選択する。

- Compute Shader
- Indirect Draw
- Async Compute
- Multiple Command Queues
- Ray Tracing
- Variable Rate Shading
- Timeline Synchronization

例えばD3D11では直列提出、D3D12 / VulkanではCommand List並列構築を許可する。

## Command実行モデル

`GetImmediateCommandList()`をRHIの中心契約にはしない。
これはD3D11 Immediate Contextに依存した概念だからである。

正式な構造は次とする。

```text
IRHIDevice
  |- Graphics Queue
  |- Compute Queue
  |- Copy Queue
  |- CreateCommandList(queueType)
  |- CreateFence()
```

D3D11 Backendは内部でImmediate Contextへ変換する。
D3D12 / Vulkan BackendはNative Command Queue / Command Bufferへ変換する。

Queue間依存はTimeline FenceのWait / Signalとして表現する。複数Queueから参照するResourceは`ResourceQueueSharingMode::Concurrent`を明示し、Vulkan Backendは必要なQueue FamilyのConcurrent Sharingへ変換する。`Exclusive` ResourceのCross-Queue利用はRenderGraph Compile時に拒否する。

`IRHICommandQueue::Submit()`後、呼び出し側はCommand List Wrapperを破棄できる。GPU完了まで必要なNative Command Allocator / Command Listの寿命保持はBackendの責務とする。RenderGraphのQueue FenceはExecute間で再利用し、明示的なExecution State解放時だけWaitIdle後に破棄する。

## Resource Handle

上位層にはNative Pointerを渡さない。

- `BufferHandle`
- `TextureHandle`
- `ShaderHandle`
- `PipelineStateHandle`
- `FenceHandle`

Handleはindex + generationで構成し、破棄後に古いHandleを拒否する。

## Pipeline State

`PipelineStateDesc`はAPI非依存値だけで構成する。

- Shader Handle
- Input Layout
- Primitive Topology
- Rasterizer State
- Depth / Stencil State
- Blend State
- Render Target Format
- Depth Format

各BackendがDescriptorをNative Pipeline Objectへ変換する。
D3D11では複数State Objectの集合、D3D12ではPSO、VulkanではVkPipelineとなる。

## RenderPass / RenderGraph

RenderPassは色・深度AttachmentとLoad / Store Operationを記述する。

D3D11 Backendでは`OMSetRenderTargets`とClearへ変換する。
Vulkanでは`VkRenderingInfo`またはRenderPassへ変換する。
D3D12ではResource BarrierとRTV / DSV Bindingへ変換する。

RenderGraphは論理Resource Accessから依存順序とBarrierを構築する。
BarrierのNative変換はBackend側の責務とする。

Pass Cullingは`RenderGraphPassFlags::Cullable`を明示したPassだけを対象にする。既存Passは既定でRootとして保持し、Imported ResourceへのWrite、`MarkOutput()`されたResourceへのWrite、非Cullable PassをRootとして依存元を逆向きに生存判定する。Culling後のPassだけで依存、Queue Sharing検証、Barrier、Resource Lifetime、Queue同期を再構築する。

## Native Interop

ImGui、Effekseer、Direct2Dなど既存のAPI依存ライブラリ用にNative Interopは許可する。
ただし通常Rendererからは利用しない。

```cpp
void* IRHIDevice::QueryNativeObject(NativeObjectType type);
```

この経路はBackend連携専用であり、通常描画APIにはしない。

## Step 16完了条件

- Backend RegistryによるAPI選択
- Capability Query
- Device / Queue / CommandList契約
- Resource Handle
- Buffer / Texture / Shader
- Pipeline State
- SwapChain
- RenderPass
- RenderGraph
- D3D11 Backend実装
- Null BackendによるAPI非依存Smoke Test

既存Rendererの移行はStep 17のRenderWorld導入と合わせて段階的に行うが、RHI自体は最初から複数Backendを実装可能な契約として完成させる。
