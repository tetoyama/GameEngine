# Step 16-F Subresource State Completion

## 状態

**完了**

## 目的

RenderGraphのResource State追跡をTexture全体単位からSubresource単位へ拡張し、Mip LevelやArray Layerが異なるアクセスを不要に直列化せず、それぞれに必要なBarrierだけを生成できるようにする。

## 公開契約

`RenderGraphSubresourceRange`を追加した。

```cpp
struct RenderGraphSubresourceRange {
    uint32_t first;
    uint32_t count;

    static RenderGraphSubresourceRange All();
    static RenderGraphSubresourceRange Single(uint32_t index);
    static RenderGraphSubresourceRange Make(
        uint32_t firstIndex,
        uint32_t subresourceCount
    );
};
```

Pass Builderの`Read` / `Write` / `ReadWrite`は任意のSubresource Rangeを受け取る。

```cpp
builder.Read(
    texture,
    RHI::ResourceState::ShaderResource,
    RHI::RenderGraphSubresourceRange::Single(0)
);
```

Rangeを省略した場合は従来どおりResource全体を対象とする。

## Texture Import契約

既存のTexture Importは互換性のため1 Subresourceとして扱う。

Texture Descriptorを渡すOverloadでは、次の数を追跡する。

```text
Subresource Count = Mip Levels × Array Size
```

```cpp
const auto resource = graph.ImportTexture(
    textureHandle,
    textureDesc.initialState,
    textureDesc,
    "MipChain"
);
```

## Hazard依存契約

依存はLogical Resourceが同じだけでは生成しない。

次の両方を満たす場合のみHazard依存とする。

1. Subresource Rangeが重複する
2. どちらかがWriteまたはReadWriteである

そのため、Mip 0へのWriteとMip 1へのWriteは並列化可能な独立アクセスとして扱う。

Mip 0へのWriteと、その後のMip 0からのReadには依存を生成する。

## Pass内宣言規則

同一Passでは次の規則を適用する。

- 同一Rangeの宣言は1 Accessへ統合する
- 同一RangeでReadとWriteが混在する場合はReadWriteとする
- 同一Rangeで異なるStateを要求した場合はCompile失敗
- 重複する非同一Rangeを宣言した場合はCompile失敗
- 重複しないRangeは同一Pass内で共存可能
- 空Rangeは拒否する

重複する非同一RangeはState統合規則が曖昧になるため、暗黙分割せず明示的に拒否する。

## Barrier生成契約

RenderGraphはSubresourceごとに現在Stateを保持する。

- 指定RangeだけをTransitionする
- 同一Subresourceへの連続UAV WriteではSubresource単位のUAV Barrierを生成する
- 全Subresourceが同一Stateの場合、全体Accessは`AllSubresources` Barrierへまとめる
- Subresource Stateが混在する場合、全体Accessでも必要なSubresourceだけ個別Barrierを生成する
- 既に要求StateのSubresourceには不要なTransitionを生成しない

D3D11 BackendはNative Transitionを持たないため論理Stateだけを更新する。
D3D12 / Vulkan Backendでは同じDescriptorをNative Barrierへ変換する。

## Final State契約

```cpp
ResourceState FinalState(RenderGraphResource resource);
ResourceState FinalState(
    RenderGraphResource resource,
    uint32_t subresource
);
```

Resource全体版は、全Subresourceが同一Stateの場合のみそのStateを返す。
Stateが混在している場合は`ResourceState::Undefined`を返す。

Subresource指定版は個別の最終Stateを返す。

## Buffer契約

Bufferは現時点では全体Resourceとして追跡する。

Bufferへ`Single`や部分Rangeを指定した場合はCompile失敗とする。
将来Byte Range単位のHazard追跡が必要になった場合はTexture Subresourceとは別契約で追加する。

## 検証

`Tests/RHIRenderGraphBarrierSmokeTest.cpp`へ次を追加した。

- Mip 0とMip 1へのWriteが依存しない
- Mip 0のWriteからMip 0のReadへだけ依存する
- Subresource Index付きTransition Barrierを生成する
- 混在StateでResource全体のFinal StateがUndefinedになる
- SubresourceごとのFinal Stateを取得できる
- 混在Stateから全MipをShader Resourceへ移行する際、必要なMipだけBarrierを生成する
- 範囲外Rangeを拒否する
- Bufferの部分Rangeを拒否する
- 同一Pass内の重複非同一Rangeを拒否する
- 同一Pass内の非重複Rangeを許可する

検証Commit:

- `3e5f39857a596fdf0d443026e4bc865a09aa738e`

検証結果:

- Migration Plan Validation run #202: success
- RHI Smoke Test run #256: success
- Windows Build run #776: success

## 完了条件

- Texture Subresource RangeをPass Accessへ宣言できる
- 非重複Rangeへ不要なHazard依存を生成しない
- Subresource別Stateを追跡できる
- 必要なSubresourceだけBarrierを生成できる
- 全Resource Accessとの整合が取れる
- 不正Rangeと曖昧な重複宣言を安全に拒否できる
- Debug / Release x64回帰がない
