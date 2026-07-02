# Step 18-B Runtime GPU Upload Integration

## 位置づけ

`Step18B_Static_Batch_GPU_Instance_Buffer.md`で定義したGPU Buffer契約を、Engine Runtimeへ接続する追補計画書。

## 実装済みPipeline

```text
RenderSystem.RenderPacket.Build        AnyWorker / Early
    -> Static CPU Candidate / Cache / Instance Data
    -> StaticBatchUploadSystem.Instance.Upload  MainThread / Default
    -> RHI Dynamic Vertex Buffer
    -> RenderSystem.RenderPacket.Submit MainThread / Late
```

Upload Taskは`RenderPacketFrameBuffer`をRead、`StaticBatchGpuInstanceBuffer`をWriteとして宣言する。

## Active RHI Device

`RenderHardwareInterfaceService`が稼働中の`IRHIDevice`を所有する。

- `AdoptDevice`
- `GetDevice`
- `ReleaseDevice`
- `ResetDevice`
- DeviceをBackendより先にShutdown

GraphicsContextが生成済みのD3D11 Device / Contextを再利用し、別Deviceを生成しない。

`EnsureGraphicsContextRHIDevice`はNative Device / ContextをRHI Wrapperへ採用する。SwapChainはGraphicsContextだけが所有し、Interop Wrapperへは`nullptr`を渡す。RHI側がBackBuffer参照を保持して`ResizeBuffers`を妨げることを防ぐ。

EngineContextではRHIServiceをGraphicsContextより後に登録する。逆順Shutdownにより、Scene / RendererのGPU Resource、RHI Device、GraphicsContext Native Deviceの順で解放する。

## StaticBatchUploadSystem

SceneManagerのSystemRegistryへRenderSystem直後に登録する。

責務:

- Static CPU Instance Dataの有効性確認
- Graphics Command List生成
- Dynamic Instance Buffer作成・再確保
- Source Revisionが変化した時だけUpload
- Graphics Queue Submit
- Finalize時の明示Release
- Upload状態とTelemetryの公開

失敗時はGPU Bufferを提出せず、通常RenderPacket経路を維持する。

## Telemetry

Scene Storage Runtime Telemetryへ次を表示する。

- GPU Instance Current / Capacity
- Uploaded Source Revision
- Upload Count
- Reallocation Count
- Failed Upload Count
- Valid
- CPU Instanceが存在するのに直近Uploadが失敗した場合の警告

## 検証

- `StaticBatchGpuInstanceBufferSmokeTest.cpp`
- `StaticBatchUploadSystemSmokeTest.cpp`
- `RHISmokeTest.cpp`
- `D3D11RHIBackendSmokeTest.cpp`
- Static Batch Foundation Workflow
- RHI Smoke Workflow
- Windows Build

## 完了済み

- [x] Active RHI Device所有
- [x] GraphicsContext既存Device採用
- [x] SwapChain非共有
- [x] Shutdown順序
- [x] Dynamic GPU Instance Buffer
- [x] MainThread Upload Task
- [x] SystemRegistry登録
- [x] Queue Submit
- [x] GPU Upload Telemetry
- [x] Editor警告
- [x] Smoke Test登録

## 未完了

- [ ] Instance Input Layout / Shader入力
- [ ] Static Group単位Offset / Count提出
- [ ] GBuffer / Shadow Pass接続
- [ ] Object ID / Picking
- [ ] Batch Culling
- [ ] GPU Upload / Instanced Draw時間計測

## 次工程

1. Instance Vertex Input契約
2. Static GroupからGeometry / Material / Instance Range解決
3. GBuffer `DrawIndexedInstanced`
4. Shadow / Pickingへの展開
5. Spatial Cell Batch Culling
