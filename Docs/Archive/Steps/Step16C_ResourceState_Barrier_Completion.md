# Step 16-C: Resource State / Barrier Contract

## 状態

コード実装完了。Null / Direct3D11 RHI Smoke TestとGameEngine全体ビルドで回帰確認する。

## 目的

Direct3D 12のResource StateとVulkanのLayout / Access同期を、Backend固有型を上位層へ漏らさず扱える共通契約を定義する。

Direct3D 11には明示Resource Barrierが存在しないが、同じRenderGraphを複数Backendで実行するため、論理状態の検証と追跡はD3D11でも行う。

## 公開契約

`ResourceState`に次の論理状態を追加した。

- Undefined
- Common
- VertexBuffer
- IndexBuffer
- ConstantBuffer
- ShaderResource
- UnorderedAccess
- RenderTarget
- DepthWrite
- DepthRead
- CopySource
- CopyDestination
- Present
- IndirectArgument

複数のRead用途はbitwise ORできる。Write用途同士の複合は原則として使用しない。

`ResourceBarrierDesc`は次を保持する。

- Barrier種別
- BufferまたはTexture Handle
- 遷移前状態
- 遷移後状態
- Subresource番号

BufferとTextureの両方、またはどちらも指定されていないBarrierは無効とする。

## Initial State

`BufferDesc`と`TextureDesc`へ`initialState`を追加した。

Resource生成時にBackend内部の論理状態を初期化する。TextureはMip単位のSubresource Stateを保持する。

## Command List

`IRHICommandList`へ次を追加した。

```cpp
bool ResourceBarrier(std::span<const ResourceBarrierDesc> barriers);
```

RenderPass実行中のBarrierは拒否する。RenderPass境界の外側で状態遷移を確定させる。

## Direct3D 11 Backend

D3D11 BackendはNative Barrierを発行しない。

代わりに次を行う。

1. Barrier配列全体を事前検証
2. HandleとSubresource範囲を検証
3. `before`と現在状態の一致を検証
4. 全Barrierが有効な場合だけ状態を一括更新

途中のBarrierが失敗した場合に、それ以前の状態だけが更新される部分適用を防ぐ。

`before = Undefined`は既知状態を問わない明示的なワイルドカードとして扱う。

UAV Barrierは現在状態に`UnorderedAccess`が含まれている場合だけ受理する。D3D11ではNative UAV Barrierが存在しないため、契約検証のみ行う。

## Null Backend

Null Backendも同じBarrier形状を検証する。

これによりNative APIなしで次をテストできる。

- Resource指定の排他性
- Buffer Subresource制約
- Undefinedへの遷移拒否
- RenderPass中のBarrier拒否
- Command List API互換性

## 検証項目

### Null RHI Smoke Test

- Buffer / Texture Transition
- BufferとTextureの同時指定拒否
- Bufferへの個別Subresource指定拒否
- Transition先Undefined拒否
- RenderPass中のBarrier拒否

### Direct3D11 RHI Smoke Test

- Common → CopyDestination
- `before`不一致拒否
- Undefined wildcard → UnorderedAccess
- UAV Barrier
- 既存Logical Queue / GPU Fence回帰

## 設計上の制約

現段階ではResource State追跡はCommand Listが所有するBackend Resourceへ直接反映される。

D3D12 / Vulkanで並列Command List記録へ進む際は、記録中のローカル状態とSubmit時のGlobal Stateを分離する必要がある。現在の契約はその前提となる状態表現を固定する段階であり、並列記録時のGlobal State競合解決までは実装しない。

## 次工程

1. Buffer / Texture View Descriptor
2. Sampler Handle / Descriptor
3. RenderGraph Resourceへ期待Stateを宣言
4. Pass間Barrierの自動生成
5. D3D11最小三角形統合テスト
