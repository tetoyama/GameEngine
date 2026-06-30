# Step 19-A.4 Local Shadow Far Validation

## 状態

**最優先Lighting Correctnessへ挿入・構造修正済み・実機確認待ち**

Point / Spot ShadowのFar範囲をLight Rangeと一致させ、Far変更時の方向依存・影消失・偽Shadowを検証する。

---

## 1. 観測

Point LightのShadowが方向だけでなく、Far相当のRange変更時にも不自然になる可能性がある。

対象設定:

```text
LightComponent.Param.x = Local Light Range
LightComponent.Param.w = Local Shadow World Bias
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

### 2.2 CPU Shadow Cullingが全LightでNear/Far Planeを無効化

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

Perspective Depthは非線形であるため、同じNDC Biasでもワールド空間換算量は受光点深度とFar Planeで変化する。

Farを大きくすると、Far側でShadowが大きく離れる、消える、境界が変化する原因になり得る。

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
LocalLightShadowProjection.h
```

不正値:

```text
NaN / Infinity / Near以下
    -> Near + MinimumDepthSpan
```

### 3.2 Shared Clip定数

C++とHLSLで次を共有する。

```text
Near Plane       = 0.1
Minimum Span     = 0.1
Maximum NDC Bias = 0.01
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

### 3.4 Depth-aware Perspective Bias

Point / Spotでは`Param.w`をワールド距離Biasとして扱い、受光点のView深度でNDC Biasへ変換する。

DirectX LH Perspective Depth:

```text
z_ndc = far/(far-near)
      - near*far / ((far-near)*viewZ)
```

微分:

```text
d(z_ndc)/d(viewZ)
    = near*far / ((far-near)*viewZ^2)
```

変換:

```text
ndcBias = worldBias
        * near*far
        / ((far-near)*viewZ^2)
```

最大値は`LOCAL_LIGHT_SHADOW_MAX_NDC_BIAS`で制限する。

Directional / CSMの既存NDC Bias契約は変更しない。

共有HLSL:

```text
ShadowDepthBias.hlsli
```

Deferred / Forwardの両方で同じ関数を使用する。

### 3.5 既存SceneのBias値

既存YAMLの`Param.w`は保存形式を変更せず読み込むが、Point / Spotでは意味が固定NDC BiasからWorld Biasへ変わる。

旧値`0.005`は新契約では`0.005 world units`として扱う。Scene ScaleやCaster形状によっては再調整が必要となる。

Inspectorでは次の名前で明示する。

```text
Shadow Bias (World)
```

---

## 4. Inspector

Point / Spotでは汎用`Param`表示を次へ分解した。

```text
Range / Shadow Far
Inner Angle       // Spotのみ
Outer Angle       // Spotのみ
Shadow Bias (World)
```

併記:

```text
Shadow Clip: Near / Far / Ratio
```

`Far / Near > 10000`ではPerspective Depth精度警告を表示する。

Range TooltipにはLighting減衰の終端とShadow Farが同じ値であることを表示する。

World Bias Tooltipには受光点ごとにNDC Biasへ変換され、Far変更に追従することを表示する。

---

## 5. 回帰テスト

### LocalLightShadowProjectionSmokeTest

- Range正常値保持
- NaN / Infinity / 負値補正
- Near / Far Ratio
- World BiasからNDC Biasへの変換
- 深度が遠いほどNDC Biasが小さくなる
- 逆算したWorld Biasが一定
- 長距離Rangeで固定NDC Biasにならない

### RenderPacketViewCullingSmokeTest

- Depth Clamp ShadowではNear / Far Planeを無効化
- Perspective ShadowではNear / Far Planeを保持
- Perspective ShadowでNear手前Casterを除外
- Perspective ShadowでFar外Casterを除外

### Lighting Diagnostic Contract

- Spot Rangeの`* 1000`再導入を禁止
- Point / Spot共通Far Policyを確認
- Perspective Depth ClipをCulling Viewへ渡すことを確認
- Deferred / ForwardでDepth-aware Bias関数を使用
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
Bias = 0.005
Bias = 0.05
```

期待:

- Range変更だけでピーターパン量が急増しない
- Near側とFar側で極端な差が出ない
- 必要なBias値がRangeごとに桁違いにならない

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
- [x] World Bias契約をInspectorへ明示
- [x] Depth-aware Perspective Bias追加
- [x] Deferred / Forward Bias統一
- [x] C++数式Smoke Test
- [x] Deferred / Forward Shader Compile Smoke
- [ ] Windows Build確認
- [ ] Lighting Diagnostic Contract確認
- [ ] Render Packet View Culling Smoke確認
- [ ] 6方向Far境界実機確認
- [ ] Range 5..500実機確認
- [ ] Bias 3段階実機確認
- [ ] Static / Dynamic Caster比較
