# Step 16-C: View and Sampler Completion

## 完了内容

- BufferViewHandle / TextureViewHandle / SamplerHandle
- BufferViewDesc / TextureViewDesc / SamplerDesc
- Null Backend実装
- Direct3D11 Backend実装
- RenderPass AttachmentのTexture View化
- Shader Resource / Unordered Access View Binding
- Sampler Binding
- View生存中の親Resource破棄拒否

## Direct3D11

Bufferはtyped / structured / raw Viewを生成する。
TextureはSRV / UAV / RTV / DSVを生成する。
SamplerはFilter、Address、Comparison、Anisotropy、LODをD3D11 Sampler Stateへ変換する。

Graphics / Compute / Copy QueueのCommand Domainは維持する。
UAV BindingはCompute Stageだけで受理する。
Copy QueueではViewとSampler Bindingを拒否する。

## 検証

RHI Smoke Test run #123:

- Null RHI build: success
- Null RHI execution: success
- Direct3D11 RHI build: success
- Direct3D11 WARP execution: success

既存のResource Barrier、Logical Queue、GPU Fenceも回帰確認した。

## 次工程

1. RenderGraph Resource State宣言
2. Pass間Barrier自動生成
3. SwapChain Image / Acquire / Present契約
4. Direct3D11最小三角形統合テスト
