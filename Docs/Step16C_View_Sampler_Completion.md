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

---

# Step 16-F: RenderGraph Resource State / Barrier

## 完了内容

- 外部Buffer / TextureのImport
- Resource初期状態の登録
- Pass Accessごとの要求State宣言
- 実行順に基づくTransition Barrier生成
- 同一UAV Stateへの連続Write時のUAV Barrier生成
- Pass実行直前のBackend Barrier発行
- 最終Resource Stateの取得
- 同一Pass内の競合State拒否
- 未Binding論理ResourceへのState指定拒否

状態を指定しない従来のAddResource / Read / Writeは依存構築専用として維持する。
これにより既存RenderGraph利用コードを壊さず、状態遷移を段階的に導入できる。

## 制約

- 現在はResource全体を遷移し、個別Subresource宣言は未対応
- Queue間所有権移譲とQueue間Fenceは未対応
- Transient Resource生成と寿命解析は未対応
- D3D11は論理状態を検証し、Native Barrierは発行しない

## 検証

専用Smoke Testで次を検証する。

- CommonからCopyDestination
- CopyDestinationからShaderResource
- ShaderResourceからUnorderedAccess
- 連続UAV Write時のUAV Barrier
- 実行順とBarrier適用順
- 最終State
- 競合Stateと未Binding ResourceのCompile失敗
