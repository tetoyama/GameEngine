# Step 16-B: D3D11 Logical Queue / Synchronization Completion

## 状態

コード・RHI Smoke Test完了。GameEngine Debug / Release x64の回帰ビルドを継続確認する。

## 目的

Direct3D 11のImmediate ContextをRHI公開契約へ直接漏らさず、次の共通Queueモデルへ適合させる。

- Graphics Queue
- Compute Queue
- Copy Queue
- Command List
- Fence Value
- Queue Submit

Direct3D 11はNative Queueを一つしか持たないため、Compute / Copyを独立Native Queueとして偽装しない。
3種類の論理Queueを公開し、すべてのCommandをImmediate Contextへ直列化する。

## 実装内容

### Logical Queue fallback

`D3D11RHIDevice`が次のQueueを所有する。

- `m_graphicsQueue`
- `m_computeQueue`
- `m_copyQueue`

`GetQueue()`と`CreateCommandList()`は3種類のQueue Typeを受け付ける。
ただしCapabilityは実態に合わせて次のままとする。

- `supportsCompute = true`
- `supportsAsyncCompute = false`
- `supportsMultipleCommandQueues = false`
- `supportsTimelineSynchronization = false`

Compute / Copy Queueの存在はAPI共通化のための論理Fallbackであり、並列GPU実行を意味しない。

### Command domain validation

Command ListはQueue Typeに応じて実行可能なCommandを制限する。

Graphics Queue:

- RenderPass
- Graphics Pipeline
- Compute Pipeline
- Vertex / Index Binding
- Draw / DrawIndexed
- Dispatch
- Resource Update

Compute Queue:

- Compute Pipeline
- Compute Shader Resource Binding
- Dispatch
- Resource Update

Copy Queue:

- Resource Update

異なるQueue TypeのCommand ListをQueueへSubmitした場合は失敗させる。
Secondary Command ListはD3D11 Backendでは未対応として生成を拒否する。

### GPU completion fence

以前の実装は`Flush()`直後にFence Valueを完了扱いしていた。
しかし`Flush()`はCommand送信を促すだけで、GPU完了を保証しない。

修正後は`D3D11_QUERY_EVENT`をSubmit末尾へ挿入し、`GetData()`が完了を返した時だけFenceの`completedValue`を進める。

これにより次を区別できる。

- CPUがCommandを提出した状態
- GPUが提出済みCommandを完了した状態

`WaitIdle()`も同じEVENT Query方式でImmediate ContextのGPU完了を待つ。

## 検証

`Tests/D3D11RHIBackendSmokeTest.cpp`でWARP Deviceを生成し、次を検証する。

- D3D11 Backend Registry
- Capability値
- Graphics / Compute / Copy論理Queue取得
- Secondary Command List拒否
- QueueごとのCommand List生成
- Compute / CopyでRenderPass拒否
- Queue Type不一致Submit拒否
- Compute → Copy → GraphicsのFence Value進行
- GPU EVENT QueryによるFence Wait
- Device WaitIdle

RHI Smoke Test run #64:

- `null-rhi-smoke-x64`: success
- `d3d11-rhi-backend-smoke-x64`: success

## 制約

D3D11 Command Listは現在も記録時にImmediate ContextへCommandを発行する。
そのためD3D12 / VulkanのNative Command Bufferのような並列記録は行わない。

上位層は`supportsMultipleCommandQueues`と`supportsAsyncCompute`を確認し、D3D11でQueue並列実行を選択してはならない。

## 次工程

Step 16の次の対象は、Migration Planに従い次の順で進める。

1. Migration Plan上の実装済みRHI項目を実コードへ同期
2. Buffer / Texture View Descriptor
3. Sampler Handle / Descriptor
4. Resource State / Barrier Descriptor
5. RenderGraph Resource State / Barrier生成
6. D3D11最小三角形統合テスト
