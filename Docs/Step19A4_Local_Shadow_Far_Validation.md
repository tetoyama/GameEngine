# Step 19-A.4 Local Shadow Far Validation

## 状態

**Range / Far・GPU Clip・保守的CPU Culling・Bias契約を整理済み／CI・実機確認待ち**

Point / Spot ShadowのFarをLight Rangeと一致させ、Far境界での影消失・偽Shadow・Bias変動を検証する。

関連契約:

```text
Docs/Step18D_Shadow_Depth_Clip_Culling_Contract.md
Docs/Step19A3_Point_Shadow_Face_Validation.md
Docs/Step19A5_Shadow_Bias_Contract.md
Docs/Shadow_Atlas_PCF_Contract.md
```

---

## 1. データ契約

```text
LightComponent.Param.x = Local Light Range / Shadow Far
LightComponent.Param.w = Projected-depth Bias at Reference Depth
```

Point / Spotでは同じ`Param.x`を次へ使用する。

```text
Lighting attenuation end
Perspective Shadow Far
```

---

## 2. 確認した問題

### 2.1 SpotだけFarを1000倍していた

旧実装:

```text
Point Shadow Far = Param.x
Spot Shadow Far  = Param.x * 1000
Lighting Range   = Param.x
```

Spotでは光が届かない範囲までProjectionが延び、PointとSpotでRangeの意味が異なっていた。

### 2.2 Perspective Shadowへ固定NDC Biasを使用していた

旧Sampling:

```text
depth = projectedDepth - Param.w
```

Perspective Depthは非線形であるため、固定NDC Biasは遠距離ほど大きなWorld-space Offsetへ相当する。ShadowがCasterから離れる、または遠距離で消える原因になり得る。

### 2.3 既存Bias値をWorld Unitへ再解釈した

一時実装では`Param.w`をWorld Biasとして扱ったが、既存Sceneの`0.005`を`0.005 world units`へ読み替えるため保存互換がなかった。この方式は撤回した。

### 2.4 GPU ClipとCPU Cullingを同じ強さにすると影が欠落する

Point / SpotのGPU RasterizerはPerspective Near / Far Clipが必要である。

ただしCPU AABB CullingでもNear / Far Planeを有効化すると、大きいCasterまたはFar境界を跨ぐCasterをTriangle Clip前に全体除外できる。

症状:

```text
ReceiverにはLight Range内なので光が届く
CasterはCPU Far Planeで除外される
結果として影だけ消える
```

GPUとCPUを完全一致させるのではなく、CPU Shadow Cullingは保守的にする必要がある。

---

## 3. 実装した契約

### 3.1 Range / Far統一

```text
Point Lighting Range = Param.x
Point Shadow Far     = Param.x
Spot Lighting Range  = Param.x
Spot Shadow Far      = Param.x
```

共通Policy:

```text
Source/GameApplication/Engine/Scene/System/Render/Lighting/
    LocalLightShadowProjection.h
```

不正値:

```text
NaN / Infinity / Near以下
    -> Near + MinimumDepthSpan
```

### 3.2 Shared Clip / Bias定数

```text
Near Plane           = 0.1
Minimum Depth Span   = 0.1
Maximum NDC Bias     = 0.01
Bias Reference Depth = 1.0
Maximum Slope Scale  = 9.0
```

定義:

```text
Source/Shader/commonDefine.h
```

### 3.3 GPU Rasterizer

```text
Directional / CSM
    DepthClipEnable = false

Point / Spot
    DepthClipEnable = true
```

Point / SpotではPerspective Near / Far外TriangleをGPUでClipし、Depth端へ押し込まれる偽Shadowを防ぐ。

### 3.4 CPU Shadow Caster Culling

全Shadow ViewでNear / Far Planeを無効化する。

```text
Directional / CSM / Point / Spot
    CPU Left / Right / Bottom / Top = enabled
    CPU Near / Far                  = disabled
```

CPU CullingはGPUより保守的である。

- Light-space XY外Casterは除外する
- Near / Far境界を跨ぐCaster Boundsは保持する
- Point / Spotの正確なPerspective ClipはGPUへ任せる

実装:

```text
Source/GameApplication/Engine/Scene/System/Render/Culling/
    ShadowRenderPacketCullingView.h
```

### 3.5 Reference-depth Perspective Bias

Point / Spotでは`Param.w`をReference DepthでのProjected-depth Biasとして保存する。

```text
projectionScale = near * far / (far - near)
referenceDerivative = projectionScale / referenceDepth^2
currentDerivative   = projectionScale / receiverDepth^2

worldEquivalentBias = Param.w / referenceDerivative
adjustedNdcBias = worldEquivalentBias
                * slopeScale
                * currentDerivative

finalBias = min(Param.w, adjustedNdcBias)
```

性質:

- Reference Depthでは既存`Param.w`と一致
- 遠距離では`1 / depth^2`で小さくなる
- 同一受光点深度ならFar変更に依存しない
- Reference Depthより近い場合は既存値を上限として維持
- NaN / Infinity / 負値は0
- 最大値は`LOCAL_LIGHT_SHADOW_MAX_NDC_BIAS`

Directional / CSMは共通Orthographic NDC Biasを使用する。

### 3.6 既存Scene互換

```text
Existing Param.w = 0.005
    -> Reference Depthで0.005 NDC
    -> 保存・再読込後も0.005
```

`0.005 world units`へ再解釈しない。

---

## 4. Inspector

Point / Spot:

```text
Range / Shadow Far
Inner Angle       // Spotのみ
Outer Angle       // Spotのみ
Shadow Bias
Shadow Clip: Near / Far / Ratio
```

`Far / Near > 10000`ではPerspective Depth精度警告を表示する。

Shadow Bias入力範囲:

```text
0.0 .. 0.01
```

---

## 5. 回帰テスト

### LocalLightShadowProjectionSmokeTest

- Range正常値保持
- NaN / Infinity / 負値補正
- Near / Far Ratio
- Reference Depthで既存Projected Biasを維持
- 深度が遠いほどNDC Biasが小さくなる
- 逆算したWorld Equivalentが一定
- 同一受光点深度ではFar変更に依存しない
- Grazing SurfaceでSlope Scaleが増加
- Maximum NDC Bias Clamp

### RenderPacketViewCullingSmokeTest

- Player / EditorではNear / Far Plane有効
- 全Shadow ViewではNear / Far Plane無効
- Shadow ViewではNear / Far外Casterを保持
- 左右上下Plane外CasterはShadow Viewでも除外
- TileごとのView Identityを分離

### Lighting Diagnostic Contract

- Spot Rangeの`* 1000`再導入を禁止
- Point / Spot共通Far Policyを確認
- Point / Spot GPU RasterizerがDepth Clipを使用
- CPU Shadow Cullingが保守的Near / Far無効契約を維持
- Deferred / Forwardで共通Bias関数を使用
- Deferred / Forward Shaderを`fxc`でコンパイル

---

## 6. 実機確認

### 6.1 Range境界

```text
Range = 5 / 10 / 25 / 50 / 100 / 500
Caster distance = Range - 0.1 / Range / Range + 0.1
```

期待:

- Light減衰境界とShadow Farが一致
- Range外TriangleがDepth端へClampされない
- Far境界を跨ぐ大きいCasterの影が突然消えない
- Range変更だけでShadowが大きく浮かない

### 6.2 6方向

```text
+X / -X
+Y / -Y
+Z / -Z
```

全方向でFar境界と影消失位置が一致すること。

### 6.3 Bias

```text
Bias = 0
Bias = 0.0005
Bias = 0.001
Bias = 0.005
Bias = 0.01
```

期待:

- Reference Depth付近の既存Scene結果を維持
- Range変更だけでPeter Panning量が急増しない
- 遠距離でShadowが突然消えない
- Point Face境界でBias量が不連続にならない

### 6.4 CPU Conservative Culling

- Caster AABBをFar Planeへ跨がせる
- Caster中心だけをFar外へ置く
- ReceiverはRange内へ残す
- Static Batch / Ordinary Drawを比較

期待:

- AABB中心または一部がFar外でも、交差Triangleの影を維持
- GPU Clip後にRange外部分だけが除外される

---

## 7. 残る場合の次候補

1. Far / Near Ratio過大によるD32 Perspective精度
2. Point Face Projection境界
3. Static Batchと通常DrawのProjection不一致
4. Range外ReceiverでAmbientだけが残る見え方
5. Texture2D AtlasからTextureCube / Texture2DArrayへの移行

Atlas Tile混入は次で別管理する。

```text
Docs/Shadow_Atlas_PCF_Contract.md
```

---

## 8. 実装状況

- [x] Point / Spot Far契約確認
- [x] Spot `Range * 1000`削除
- [x] 不正Range補正
- [x] Point / Spot GPU Perspective Depth Clip
- [x] 全Shadow CPU Near / Far Culling無効化
- [x] CPU / GPU責務を文書化
- [x] InspectorへNear / Far / Ratio表示
- [x] Reference-depth Bias契約
- [x] Deferred / Forward Bias統一
- [x] C++数式Smoke Test更新
- [x] Deferred / Forward Shader Compile Smoke
- [ ] Windows Build確認
- [ ] Lighting Diagnostic Contract確認
- [ ] Render Packet View Culling Smoke確認
- [ ] 6方向Far境界実機確認
- [ ] Range 5..500実機確認
- [ ] Bias段階実機確認
- [ ] Far境界Caster AABB確認
- [ ] Static / Dynamic Caster比較
