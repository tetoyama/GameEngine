# Step 18-D Static Shadow Submission

## 目的

Static Instance BatchをShadowMap Passへ提出し、同一Geometry / Material条件を持つStatic ModelのShadow Draw Callを`DrawIndexedInstanced`へ統合する。

この最適化が失敗しても影を欠落させず、既存の通常`RenderPacket`経路へ完全にFallbackすることを最優先とする。

## 現在の実装範囲

実装済み:

- Static GBuffer PipelineのVertex Shaderを共有するShadow専用Pipeline
- `shadowPS.cso`とDepth-only Pipeline Stateの所有
- D32 Depth Target、Color Attachment 0、Back Cull、Depth Clip無効、LessEqual契約
- `StaticBatchUploadSystem`によるShadow Pipeline Bootstrap / Release
- Shadow Pipelineを先に解放してから共有Vertex Shaderを解放する逆順所有契約
- Source CPU Instance / Packet Cache / Source GPU InstanceのRevision一致検証
- CPU Instance GroupとPacket Cache Entryの完全対応検証
- Shadow対象Group内の全Packet検証
- Opaque Model、2 Instance以上、Shadow Pass有効Groupの選別
- Geometry Binding Cacheの再利用
- 通常`RenderableModel`と同じComponent Material定数の再構築
- Shadow Atlasの各Light Tileで`DrawIndexedInstanced`を提出
- Queue Submit成功Groupだけを通常Shadow Packetから除外
- Static提出後の通常Shadow Pixel Shader / Sampler / Rasterizer復元
- Pipeline / Eligibility / Ownership / Shader Compile Smoke Test

## 初期Eligibility

初期Static Shadow対象は次をすべて満たすGroupに限定する。

```text
Packet Kind       = Model
Render Layer      = Opaque3D
Instance Count    >= 2
Pass Mask         includes Shadow
Geometry Key      = Geometry Binding Cacheと一致
Source Revision   = Packet Cache / CPU Instance / GPU Instanceで一致
Group Mapping     = CPU Instance GroupとPacket Cache Entryが完全一致
Material          = Group全体で同一
Diffuse Texture   = 未使用
```

Groupの代表Packetだけではなく、対応する全PacketについてScene、Kind、Layer、Material Key、Shadow Passを検証する。

Diffuse Textureを使用するGroupは、Alpha Test / Cutoutの同等性をStatic経路で証明できるまで通常Shadow描画へ残す。

## Shadow Material同等性

既存`RenderableModel`のShadow経路は、通常描画のGeometry / Texture情報を解決した後も、Shadow PSへ渡すMaterial定数にはComponent Materialを使う。Assimp MaterialのDiffuse ColorはShadow用定数へ再設定しない。

Static Shadowも同じ契約に合わせる。

```text
Geometry / Resource同等性
    -> StaticBatchModelMaterialResolverで検証

Shadow PSへ渡すMaterial / UV
    -> StaticBatchModelPacketMaterialで代表Packetから再構築
    -> Assimp Diffuse Colorを上書きしない
```

これによりAssimp Diffuse AlphaとComponent Alphaが異なるModelでも、Static経路だけAlpha Clip結果が変わることを防ぐ。

## 原子的Fallback

```text
Pipeline未初期化
Source / Cache / GPU Revision不一致
CPU Instance Group / Packet Cache Entry不一致
Group Packet不整合
Geometry Binding未解決
Material同等性未証明
Diffuse Texture使用
Command生成失敗
Draw生成失敗
Queue Submit失敗
    ↓
Replacement Setへ登録しない
    ↓
既存RenderPacketを通常Shadow経路で描画
```

Replacement SetはGraphics QueueへのSubmit成功後にだけ更新する。

Queue Submit後にReplacement登録が失敗した場合も通常Packetを残すため、最悪でもDepthの重複描画となり、Shadow欠落にはしない。

## D3D11実行順

現在のD3D11 RHI Command ListはDeferred Contextへ記録する方式ではなく、`SetPipelineState`、Buffer Bind、`DrawIndexedInstanced`をImmediate Contextへ順番に発行する。

したがって次の順序がGroupごとに維持される。

```text
SetMaterial / SetUV
    -> SetPipelineState / Buffer Bind
    -> DrawIndexedInstanced
```

複数Groupを提出しても最後のMaterialだけが全Drawへ適用される構造にはならない。

## D3D11状態復元

Static Shadow提出後、通常Renderable描画へ戻る前に次を復元する。

- `shadowPS.cso`
- Shadow Sampler Slot 1
- `DepthClipEnable = false` Rasterizer State

Static PipelineのDepth-StencilはDepth Write有効、LessEqualであり、Shadow Pass契約と一致する。通常Renderableは従来どおり自身のVertex Shader / Input Layout / Geometry BufferをBindする。

## Compile / Test契約

`Static Batch Foundation Smoke Test`で次を検証する。

- Shadow Pipeline Descriptor
- 共有Vertex Shader所有
- Shadow Pipeline逆順Release
- CPU Instance Group / Packet Cache Entry Mapping
- Group Packet Eligibility
- `size_t`から`uint32_t`へ変換する前のInstance Range検証
- Submission HelperのHeader Compile
- `StaticBatchVS.hlsl`
- `StaticBatchGBufferPS.hlsl`
- `shadowPS.hlsl`

ShadowMapPass変更も同WorkflowのPath Filterへ含める。

## 未完了

- [ ] Windows Engine Debug x64 Compile
- [ ] Windows Engine Release x64 Compile
- [ ] Static Batch Foundation Workflow完了
- [ ] Directional Shadow実機回帰
- [ ] CSM全Cascade実機回帰
- [ ] Spot Shadow実機回帰
- [ ] Point Shadow 6 Face実機回帰
- [ ] RenderDocで通常Shadow DrawとStatic Shadow Drawを比較
- [ ] Shadow Draw削減TelemetryのEditor表示
- [ ] Diffuse Texture / Alpha Cutout GroupのStatic対応
- [ ] Light View単位Batch Culling

## 完了条件

- Static対象ModelのShadowが通常経路と一致する
- Static提出失敗時にShadow欠落が発生しない
- Queue成功前に通常Packetを除外しない
- Directional / CSM / Spot / Pointの全Light種別でAtlas Tileを破壊しない
- Debug / Release x64と専用Smoke Testが成功する
- RenderDocでShadow Draw Call削減を確認できる
