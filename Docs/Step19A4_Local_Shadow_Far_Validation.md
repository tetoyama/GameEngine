# Step 19-A.4 Local Shadow Far Validation

## 状態

**最優先Lighting Correctnessへ挿入・構造修正済み・Bias契約再統一済み・実機確認待ち**

Point / Spot ShadowのFar範囲をLight Rangeと一致させ、Far変更時の方向依存・影消失・偽Shadowを検証する。

Biasの詳細契約は次を参照する。

```text
Docs/Step19A5_Shadow_Bias_Contract.md
```

---

## 1. 観測

Point LightのShadowが方向だけでなく、Far相当のRange変更時にも不自然になる可能性がある。

対象設定:

```text
LightComponent.Param.x = Local Light Range / Shadow Far
LightComponent.Param.w = Projected-depth Bias at Reference Depth
```

Point / SpotではRangeをLighting減衰とShadow ProjectionのFar Planeで共有する。

---

## 2. 確認した問題

### 2.1 SpotだけFarを1000倍していた

旧実装:

```text
Point Shadow Far = Param.x
Spot Shadow Far  = Param.x * 1000
Lighting Range   = Param.x
```

Spotでは光が届かない範囲までShadow Projectionが延び、PointとSpotでRangeの意味が異なっていた。

### 2.2 CPU Shadow Cullingが全LightでNear / Far Planeを無効化

旧Culling契約は、ShadowMapPass全体が`DepthClipEnable = FALSE`である前提だった。

```text
Shadow View
    Left / Right / Top / Bottom Plane: ON
    Near / Far Plane: OFF
```

Point / SpotをPerspective用`DepthClipEnable = TRUE`へ分離した後も、CPU側ではNear / Far Planeを無効化したままだった。

### 2.3 Perspective Shadowへ固定NDC Biasを使用

旧Sampling:

```text
depth = projectedDepth - Param.w
```

Perspective Depthは非線形であるため、同じNDC Biasでもワールド空間換算量は受光点深度で変化する。

遠距離では固定NDC Biasが大きなワールド空間オフセットへ変換され、Shadowが離れる・消える原因になり得る。

### 2.4 既存Bias値をWorld Unitへ再解釈した

一時実装では`Param.w`をWorld Biasとして扱ったが、既存Sceneの`0.005`を`0.005 world units`へ読み替えるため、保存互換がなかった。

この方式は撤回した。

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

C++とHLSLで次を共有する。

```text
Near Plane          = 0.1
Minimum Span        = 0.1
Maximum NDC Bias    = 0.01
Bias Reference Depth = 1.0
Maximum Slope Scale = 9.0
```

定義:

```text
Source/Shader/commonDefine.h
```

### 3.3 Light Type別CPU Culling

```text
Directional / CSM
    DepthClipEnable = FALSE
    CPU Near / Far Plane = OFF

Point / Spot
    DepthClipEnable = TRUE
    CPU Near / Far Plane = ON
```

GPU RasterizerとCPU CullingのDepth Clip契約を一致させる。

### 3.4 Reference-depth Perspective Bias

Point / Spotでは`Param.w`を基準深度でのProjected-depth Biasとして保存する。

DirectX LH Perspective Depth:

```text
z_ndc = far / (far - near)
      - near * far / ((far - near) * viewZ)
```

微分:

```text
d(z_ndc) / d(viewZ)
    = near * far / ((far - near) * viewZ^2)
```

変換:

```text
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
- 最大値は`LOCAL_LIGHT_SHADOW_MAX_NDC_BIAS`で制限

Directional / CSMはOrthographic用NDC Bias契約を維持する。

共有HLSL:

```text
Source/Shader/Material/ShadowDepthBias.hlsli
```

Deferred / Forwardの両方で同じ関数を使用する。

### 3.5 既存Scene互換

既存YAMLの`Param.w`は保存形式も数値も変更しない。

```text
Existing Param.w = 0.005
    -> Reference Depthで0.005 NDC
    -> 保存・再読込後も0.005
```

`0.005 world units`へ再解釈しない。

---

## 4. Inspector

Point / Spotでは汎用`Param`表示を次へ分解した。

```text
Range / Shadow Far
Inner Angle       // Spotのみ
Outer Angle       // Spotのみ
Shadow Bias
```

併記:

```text
Shadow Clip: Near / Far / Ratio
```

`Far / Near > 10000`ではPerspective Depth精度警告を表示する。

Range TooltipにはLighting減衰の終端とShadow Farが同じ値であることを表示する。

Shadow Bias Tooltipには基準深度のProjected-depth Biasであり、Point / Spotでは距離に応じて減衰することを表示する。

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
- Reference Depthより近い場合は既存値を上限として維持
- Grazing SurfaceでSlope Scaleが増加
- Maximum NDC Bias Clamp

### RenderPacketViewCullingSmokeTest

- Depth Clamp ShadowではNear / Far Planeを無効化
- Perspective ShadowではNear / Far Planeを保持
- Perspective ShadowでNear手前Casterを除外
- Perspective ShadowでFar外Casterを除外

### Lighting Diagnostic Contract

- Spot Rangeの`* 1000`再導入を禁止
- Point / Spot共通Far Policyを確認
- Perspective Depth ClipをCulling Viewへ渡すことを確認
- Deferred / Forwardで共通Bias関数を使用
- Deferred / Forward Shaderを`fxc`でコンパイル

---

## 6. 実機確認

### 6.1 Range境界

Point Lightを固定し、Caster / Receiverを同じ軸上へ置く。

測定Range:

```text
5
10
25
50
100
500
```

各Rangeで次を確認する。

```text
Caster distance = Range - 0.1
Caster distance = Range
Caster distance = Range + 0.1
```

期待:

- Range内Casterだけが有効
- Range外CasterがFar PlaneへClampされない
- Point Lightの減衰境界とShadow Farが一致
- Range変更でShadowが急に大きく浮かない

### 6.2 6方向

同じ距離条件を次の方向で確認する。

```text
+X / -X
+Y / -Y
+Z / -Z
```

全方向でFar境界が一致すること。

### 6.3 Bias

同じSceneでRangeだけを変更し、Shadow Biasを固定する。

```text
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

---

## 7. 残る場合の次候補

1. Point Face Atlas Tileを跨ぐPCF Tap
2. Far / Near Ratio過大によるD32 Perspective精度
3. Caster BoundsがFar Planeを跨ぐ場合のCPU AABB判定
4. Static Batchと通常DrawのProjection不一致
5. Range外ReceiverでAmbientだけが残る見え方
6. Texture2D AtlasからTextureCube / Texture2DArrayへの移行

---

## 8. 実装状況

- [x] Point / Spot Far契約確認
- [x] Spot `Range * 1000`削除
- [x] Local Shadow Clip Policy追加
- [x] 不正Range補正
- [x] Point / Spot CPU Near / Far Culling有効化
- [x] InspectorへNear / Far / Ratio表示
- [x] Reference-depth Bias契約をInspectorへ明示
- [x] Depth-aware Perspective Bias追加
- [x] Deferred / Forward Bias統一
- [x] C++参照実装をHLSL契約へ再統一
- [x] C++数式Smoke Test更新
- [x] Deferred / Forward Shader Compile Smoke
- [ ] Windows Build確認
- [ ] Lighting Diagnostic Contract確認
- [ ] Render Packet View Culling Smoke確認
- [ ] 6方向Far境界実機確認
- [ ] Range 5..500実機確認
- [ ] Bias段階実機確認
- [ ] Static / Dynamic Caster比較
