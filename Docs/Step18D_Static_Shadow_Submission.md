# Step 18-D Static Shadow Submission

## 目的

Static Instance BatchをShadowMap Passへ提出し、同一Geometry / Material条件を持つStatic ModelのShadow Draw Callを`DrawIndexedInstanced`へ統合する。

最適化が失敗しても影を欠落させず、既存の通常`RenderPacket`経路へ完全にFallbackすることを最優先とする。

## 実装済み

- Static GBuffer Vertex Shaderを共有するShadow専用Depth-only Pipeline
- `shadowPS.cso`、Color Attachment 0、D32、Back Cull、Depth Clip無効、LessEqual契約
- `StaticBatchUploadSystem`によるPipeline所有とShadow→共有Vertex Shaderの逆順Release
- CPU Instance / Packet Cache / GPU InstanceのRevision一致検証
- CPU Instance Group / Packet Cache Entryの完全Mapping検証
- Group内全PacketのScene / Kind / Layer / Material / Shadow Pass検証
- Opaque Model、2 Instance以上の選別
- Geometry Binding CacheとSource GPU Instance Bufferの再利用
- Directional / CSM / Spot / Pointの各Shadow Atlas Tileで`DrawIndexedInstanced`提出
- Queue Submit成功Groupだけ通常Shadow Packetから除外
- Static提出後のShader / Sampler / Rasterizer / Diffuse Texture状態復元
- Player / Editorと全Light Tileを合算する累積Shadow Telemetry
- Diffuse Texture / Alpha Cutout対応
- D3D11 WARPによるDepth-only Pipeline / Instanced Draw / Texture State実機契約

## Eligibility

Static Shadow対象は次をすべて満たすGroupに限定する。

```text
Packet Kind       = Model
Render Layer      = Opaque3D
Instance Count    >= 2
Pass Mask         includes Shadow
Geometry Key      = Geometry Binding Cacheと一致
Source Revision   = Packet Cache / CPU Instance / GPU Instanceで一致
Group Mapping     = CPU Instance GroupとPacket Cache Entryが完全一致
Resource Keys     = Pipeline / Geometry / Texture / Materialが一致
Diffuse Texture   = 未使用、または有効なSRVとSamplerを取得可能
```

Groupの代表Packetだけではなく、対応する全PacketについてScene、Kind、Layer、Material Key、Shadow Passを検証する。

Normal Mapを持つModelは、現在の共通GBuffer Material Resolverが明示的に拒否するため通常Shadow経路へ残る。ShadowではNormal Mapを使わないが、Resource同等性の拡張は別工程とする。

## Material / Alpha同等性

通常`RenderableModel`とStatic Shadowは、同じMaterial Resolver結果をShadow PSへ渡す。

```text
Component Material
    + Assimp Diffuse Color
    + Model Diffuse Texture または TextureComponent Override
    + UV Animation
    -> shadowPS.hlsl
    -> BaseColor Alpha × Texture Alpha
    -> ALPHA_CLIP_THRESHOLDでdiscard
```

通常`RenderableModel`では、Model内Diffuse TextureをBindした後の`materialData`をShadow時にもConstant Bufferへ反映するよう修正した。これにより通常DrawとStatic DrawでMaterialFlagsおよびBaseColor Alphaが一致する。

Static Shadowでは`t0`へDiffuse SRV、`s0`へ既存Material Samplerを使用する。Samplerが取得できない場合はStatic提出せず、通常Shadow Packetを維持する。

## D3D11 Pixel State契約

`StaticBatchShadowPixelState`がStatic提出前の次の状態を保存する。

- Diffuse Texture SRV Slot `t0`
- Material Sampler Slot `s0`

GroupごとにDiffuse TextureをBindし、スコープ終了時に元の`t0/s0`を復元する。非D3D11 Backendではこのクラスを生成しない。

WARP Interop Testで次を検証する。

- 元SRV / Samplerの取得
- Alpha Texture SRVへの一時差し替え
- Bind中のSRV / Sampler一致
- スコープ終了後の完全復元
- Sampler未Bind時の`CanSampleDiffuseTexture=false`

## 原子的Fallback

```text
Pipeline未初期化
Source / Cache / GPU Revision不一致
CPU Instance Group / Packet Cache Entry不一致
Group Packet不整合
Geometry Binding未解決
Material / Resource同等性未証明
Diffuse SRV欠落
Material Sampler欠落
Command生成失敗
Draw生成失敗
Queue Submit失敗
    ↓
Replacement Setへ登録しない
    ↓
既存RenderPacketを通常Shadow経路で描画
```

Replacement SetはGraphics QueueへのSubmit成功後にだけ更新する。Submit後のReplacement登録が失敗した場合も通常Packetを残すため、最悪でもDepthの重複描画となり、Shadow欠落にはしない。

## D3D11実行順

D3D11 RHI Command ListはImmediate Contextへ順番に発行する。

```text
Bind Diffuse Texture
    -> SetMaterial / SetUV
    -> SetPipelineState / Buffer Bind
    -> DrawIndexedInstanced
```

GroupごとのMaterial / Texture更新とDraw順序は維持される。

Static提出後、通常Renderable描画へ戻る前に次を復元する。

- `shadowPS.cso`
- Shadow Sampler Slot 1
- `DepthClipEnable=false` Rasterizer State
- Diffuse Texture Slot `t0`
- Material Sampler Slot `s0`

## Telemetry

Shadow Submission Telemetryは`StaticBatchUploadSystem`が所有し、Player / Editor Viewと全Shadow Atlas Tileを合算する。

値はProject SettingsのTelemetry Reset以降の累積値とする。

- Light Tile Attempts
- Candidate Group Visits
- Submitted Groups / Instances
- Ordinary Draws Replaced
- Static Draw Calls
- Estimated Draw Reduction
- Mapping / Eligibility / Texture Binding / Layer / Geometry Fallback
- Draw / Queue Failures

全return経路をRAII Recorderで記録する。既存Telemetry ResetはShadow集計もResetする。

## Compile / Test契約

`Static Batch Foundation Smoke Test`:

- Shadow Pipeline Descriptor / Ownership / Release
- CPU Instance Group / Packet Cache Entry Mapping
- Group Packet Eligibility
- Instance Range検証
- Telemetry集計 / Draw削減計算 / Reset
- Shadow / GBuffer Shader Compile

`D3D11 Static Batch Interop Smoke`:

- 10要素Static Input Layout
- Depth-only Pipeline / D32 DSV
- `DrawIndexedInstanced(3, 2)`
- Queue Submit / WaitIdle
- Diffuse SRV / Samplerの一時Bindと復元
- Sampler欠落時の拒否

WorkflowのPath FilterにはStatic Batch、ShadowMapPass、RenderableModel、Telemetry UIを含める。

## 未完了

- [ ] Windows Engine Debug x64 Compile
- [ ] Windows Engine Release x64 Compile
- [ ] Static Batch Foundation Workflow完了
- [ ] D3D11 Static Batch Interop Workflow完了
- [ ] Directional Shadow実機回帰
- [ ] CSM全Cascade実機回帰
- [ ] Spot Shadow実機回帰
- [ ] Point Shadow 6 Face実機回帰
- [ ] RenderDocで通常Shadow DrawとStatic Shadow Drawを比較
- [ ] Normal Map付きModelをShadow Eligibilityへ拡張
- [ ] Light View単位Batch Culling

## 完了条件

- Static対象ModelのShadowが通常経路と一致する
- Alpha Cutout輪郭が通常DrawとStatic Drawで一致する
- Static提出失敗時にShadow欠落が発生しない
- Queue成功前に通常Packetを除外しない
- Directional / CSM / Spot / Pointの全Light種別でAtlas Tileを破壊しない
- Debug / Release x64と専用Smoke Testが成功する
- RenderDocでShadow Draw Call削減を確認できる
