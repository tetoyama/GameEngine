# Step 18-B Static Batch GPU Instance Buffer

## 目的

Step 18-Aで生成した`StaticBatchInstanceData`をBackend非依存RHI BufferへUploadし、将来の`DrawIndexedInstanced`提出に必要なGPU Resource所有契約を確立する。

この段階では既存GBuffer / Shadow描画へ接続しない。GPU Buffer生成またはUploadに失敗した場合はStatic Batch提出を無効化し、通常RenderPacket描画を維持する。

## RHI Draw契約

`IRHICommandList`へ次の任意機能を追加した。

```cpp
bool DrawIndexedInstanced(
    uint32_t indexCountPerInstance,
    uint32_t instanceCount,
    uint32_t firstIndex = 0,
    int32_t vertexOffset = 0,
    uint32_t firstInstance = 0
);
```

契約:

- 未対応Backendは`false`を返す
- 暗黙に通常Drawの複数回実行へ変換しない
- Graphics Queue以外では失敗する
- Index CountまたはInstance Countが0の場合は失敗する
- D3D11 Backendは`ID3D11DeviceContext::DrawIndexedInstanced`へ転送する

戻り値を確認してから通常RenderPacket Fallbackを選択する。

## GPU Buffer所有

`StaticBatchGpuInstanceBuffer`が次を所有する。

- `RHI::BufferHandle`
- 現在のInstance Count
- 確保済みInstance Capacity
- 最後にUploadしたSource Revision
- Upload / Reallocation / Failure Telemetry

GPU Resourceの破棄にはDeviceが必要なため、Destructorから暗黙解放せず`Release(IRHIDevice&)`を明示的に呼ぶ。

```text
Owner Finalize
    -> GPU利用完了を保証
    -> StaticBatchGpuInstanceBuffer::Release(device)
    -> Device::DestroyBuffer
```

## Buffer記述

```text
Usage       = Dynamic
Bind        = Vertex
CPU Access  = Write
Initial     = VertexBuffer
Stride      = sizeof(StaticBatchInstanceData)
```

Instance Input Slotは描画側が指定する。`Bind(commandList, slot)`はBufferが有効でInstance Countが1以上の場合だけ成功する。

## Capacity Growth

必要Instance数が現在Capacityを超えた場合、2倍単位で拡張する。

```text
Required <= Capacity
    -> Existing Buffer reuse

Required > Capacity
    -> Next power-of-two capacity
    -> Replacement Buffer create
    -> Old Buffer destroy
    -> Handle swap
```

32bitの`BufferDesc::byteSize`を超える場合は生成しない。

Replacement生成後に旧Buffer破棄へ失敗した場合はReplacementを破棄し、旧Bufferを維持する。

## Upload抑止

Frame Generationではなく`StaticBatchInstanceDataBuffer::SourceRevision()`を使用する。

```text
Source Revision同一
かつ Instance Count同一
かつ GPU Buffer有効
    -> Upload省略
```

Static Dataが変化していないFrameでは`UpdateBuffer`を繰り返さない。

Transform、Entity Generation、Resource Keyなどが変化してSource Revisionが更新された場合だけ再Uploadする。

## Failure Policy

次の場合はGPU提出状態を無効化する。

- CPU Instance DataがInvalidまたはOverflow
- Buffer生成失敗
- 旧Buffer解放失敗
- Command List UpdateBuffer失敗
- Byte Size上限超過

GPU Buffer Handle自体が再利用可能でも、失敗後は`CanSubmit=false`として古いInstance内容を提出しない。

通常RenderPacketは変更しない。

## Telemetry

```cpp
struct StaticBatchGpuInstanceBufferTelemetry {
    size_t currentInstanceCount;
    size_t instanceCapacity;
    size_t uploadCount;
    size_t reallocationCount;
    size_t failedUploadCount;
    uint64_t uploadedSourceRevision;
    bool valid;
};
```

## 検証

`StaticBatchGpuInstanceBufferSmokeTest.cpp`で次を確認する。

- Dynamic Vertex Buffer生成
- Stride / Byte Size / Bind / CPU Access契約
- Instance SlotへのBind
- 同一Source RevisionのUpload省略
- Instance増加時のCapacity拡張
- Revision変更時の再Upload
- 明示Release後のHandle / Capacity / Valid状態Reset

Static Batch Foundation Workflowへ追加する。

RHI Smoke Testでは次を確認する。

- Null BackendはInstanced Drawに`false`
- D3D11 Command Listが正しいOverrideシグネチャを持つ

## 完了済み

- [x] Backend非依存Instanced Draw契約
- [x] 未対応Backendの安全な`false` Fallback
- [x] D3D11 Native Instanced Draw実装
- [x] Dynamic GPU Instance Buffer所有クラス
- [x] Capacity Growth
- [x] Source RevisionによるUpload抑止
- [x] Explicit Release
- [x] GPU Buffer Telemetry
- [x] Null RHI Smoke Test
- [x] Static Batch Workflow接続

## 未完了

- [ ] RenderSystem / RenderWorldによるGPU Buffer所有
- [ ] MainThread Upload Task登録
- [ ] Static Group単位Offset / Count提出
- [ ] GBuffer / Shadow Pass接続
- [ ] Instance Input Layout / Shader入力
- [ ] Object ID / Picking出力
- [ ] Upload失敗時のEditor診断表示
- [ ] CPU Build / GPU Upload / Instanced Draw計測

## 次工程

1. `StaticBatchGpuInstanceBuffer`をRenderWorld相当の長寿命所有先へ配置する
2. CPU Instance Build完了後のMainThread Upload Taskを追加する
3. Instance Input LayoutとVertex Shader契約を追加する
4. GBufferでStatic Groupを提出する
5. Shadow / Picking / Cullingへ展開する
