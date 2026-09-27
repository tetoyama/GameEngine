# Step 18-D Static Shadow Vertex Input State Fix

## 実機症状

`Asset/Scene/_scene.scene`を開いた直後、D3D11 Debug Layerが通常の`Draw` / `DrawIndexed`に対して次を繰り返し報告した。

```text
DEVICE_SHADER_LINKAGE_SEMANTICNAME_NOT_FOUND
INSTANCEWORLD0-3 / INSTANCEOBJECT0
```

Scene自体は開始・追加まで完了しており、Scene Load失敗ではない。

## 原因

Static Shadowの`DrawIndexedInstanced`はD3D11 Immediate Contextへ次をBindする。

- Static Batch Vertex Shader
- `INSTANCEWORLD0-3` / `INSTANCEOBJECT0`を含むInput Layout
- Triangle List Topology

Static提出後、ShadowMap PassはPixel Shader、Sampler、Rasterizer Stateだけを通常経路へ戻していた。

そのためStatic用Input Layoutが残留し、通常`commonVS`と組み合わされた次のDrawでVertex Shader入力署名が不一致になった。

## 修正

`StaticBatchD3D11VertexInputState`を追加し、Static Shadow提出前に次を捕捉する。

- Vertex Shader
- Input Layout
- Primitive Topology

RAII Destructorで全return経路から元の状態へ復元する。

復元は`StaticBatchShadowSubmission::Submit`内部で行うため、次を含む全経路で保証される。

- Static Draw成功
- Group単位Fallback
- Upload失敗
- Queue取得・Submit失敗
- Source / Visibility / Mapping検証失敗

Pixel Texture / Sampler復元は既存`StaticBatchShadowPixelState`が引き続き担当する。

## 回帰契約

`StaticBatchShadowVertexInputRestoreSmokeTest.cpp`で次を検証する。

- State Scopeがcopy / move不可
- `VSGetShader` / `IAGetInputLayout` / `IAGetPrimitiveTopology`による捕捉
- `VSSetShader` / `IASetInputLayout` / `IASetPrimitiveTopology`による復元
- Static Shadow Command開始前にScopeが構築される

## 実機再確認

同じ`_scene.scene`を開き、D3D11 Debug Outputに次が出ないことを確認する。

```text
DEVICE_SHADER_LINKAGE_SEMANTICNAME_NOT_FOUND
INSTANCEWORLD
INSTANCEOBJECT
```

併せて通常Model、Terrain、BillboardのShadowとStatic Shadow Draw Call削減を確認する。
