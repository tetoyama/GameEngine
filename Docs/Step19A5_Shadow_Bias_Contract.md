# Step 19-A.5 Shadow Bias Contract

## 状態

**CPU / HLSL契約統一済み・CI / 実機確認待ち**

Point / Spot Shadowの距離依存アーティファクトを抑えつつ、既存Sceneの`LightComponent.Param.w`を再解釈せず維持する。

---

## 1. 解決する問題

Perspective Shadow Depthは非線形である。

```text
z_ndc = far / (far - near)
      - near * far / ((far - near) * viewZ)
```

微分は次になる。

```text
d(z_ndc) / d(viewZ)
    = near * far / ((far - near) * viewZ^2)
```

固定NDC Biasを全距離へそのまま適用すると、遠距離ほどワールド空間換算量が増え、次が起こりやすい。

- ShadowがCasterから離れる
- 遠距離のShadowが消える
- Point Face境界で見え方が変化する
- Range / Far変更時に必要なBias量が変化する

一方、既存`Param.w`をWorld Unitへ再解釈すると、保存済みSceneの見た目が互換でなくなる。

---

## 2. 保存契約

`LIGHT`へ新しいBiasフィールドは追加しない。

```text
Param.x = Point / Spot Range and Shadow Far
Param.y = Spot Inner Angle
Param.z = Spot Outer Angle
Param.w = Projected-depth Bias at Reference Depth
```

共有定数:

```text
Near Plane      = 0.1
Reference Depth = 1.0
Maximum NDC Bias = 0.01
Maximum Slope Scale = 9.0
```

`Param.w`の既存値は、基準深度でのProjected-depth Biasとして維持する。

```text
Existing Param.w = 0.005
    -> Reference Depthで0.005 NDC
    -> 保存・再読込後も同じ値
```

既存値を`0.005 world units`へ読み替えない。

---

## 3. Point / Spot変換

入力値を基準深度でのNDC BiasとしてWorld Equivalentへ変換し、現在の受光点深度で再投影する。

```text
projectionScale = near * far / (far - near)
referenceDerivative = projectionScale / referenceDepth^2
currentDerivative   = projectionScale / receiverDepth^2

worldEquivalentBias = baseNdcBias / referenceDerivative
adjustedNdcBias = worldEquivalentBias
                * slopeScale
                * currentDerivative

finalBias = min(baseNdcBias, adjustedNdcBias)
```

性質:

- 基準深度では既存`Param.w`と一致する
- 基準深度より遠いほど`1 / depth^2`で小さくなる
- Farだけを変更しても、同じ受光点深度なら原理上同じ値になる
- 基準深度より近い場合は既存値を上限として維持する
- 最大値は`LOCAL_LIGHT_SHADOW_MAX_NDC_BIAS`で制限する

---

## 4. Slope契約

受光面がShadow Cameraへ対してGrazingになるほどBiasを増やす。

```text
grazing = 1 - saturate(receiverAlignment)
slopeScale = min(1 + grazing^2 * 8, 9)
```

Point LightではRadial `NdotL`ではなく、選択したCubemap Face方向と受光面法線のAlignmentを使う。

```text
receiverAlignment = abs(dot(receiverNormal, selectedFaceDirection))
```

理由:

水平面でも、横向きCubemap Faceから見ればGrazing Surfaceになるため、Radial `NdotL`だけではShadow Cameraに対する傾斜を表せない。

---

## 5. Directional / CSM契約

Directional / CSMはOrthographic Projectionのため、Perspective距離補正を行わない。

```text
baseNdcBias = max(Param.w, 0)
finalBias = min(baseNdcBias * slopeScale, MaximumNdcBias)
```

Point / Spotと同じ`Param.w`を使うが、Projection種別に応じて解釈処理を分ける。

---

## 6. CPU / HLSL同期

CPU参照実装:

```text
Source/GameApplication/Engine/Scene/System/Render/Lighting/
    LocalLightShadowProjection.h
```

HLSL実装:

```text
Source/Shader/Material/ShadowDepthBias.hlsli
```

Deferred / Forward使用箇所:

```text
Source/Shader/Material/DeferredFunc.hlsli
Source/Shader/Material/FowardFunc.hlsli
```

両実装は次を共有する。

- Near Plane
- Minimum Depth Span
- Maximum NDC Bias
- Reference Depth
- Maximum Slope Scale
- Reference DerivativeからWorld Equivalentを求める式
- Current Derivativeによる再投影
- 既存値を上限とするClamp

---

## 7. Inspector契約

Point / Spotでは次を表示する。

```text
Range / Shadow Far
Inner Angle       // Spotのみ
Outer Angle       // Spotのみ
Shadow Bias
Shadow Clip: Near / Far / Ratio
```

Tooltip:

```text
Projected-depth bias at the reference depth.
Point and Spot shadows reduce it with distance so the equivalent
world-space offset remains stable.
```

`Shadow Bias (World)`とは表示しない。

---

## 8. 自動テスト

`LocalLightShadowProjectionSmokeTest`で次を確認する。

- 不正Rangeの補正
- Reference Depthで入力Biasを維持
- 遠距離ほどNDC Biasが小さくなる
- 再構築したWorld Equivalentが一定
- 同一受光点深度ではFar変更に依存しない
- 近距離では既存Projected Biasを上限として維持
- Grazing SurfaceでSlope Scaleが増加
- Maximum NDC Bias Clamp
- 負値 / NaN Biasを0へ補正

`Lighting Diagnostic Contract`で次を確認する。

- Deferred / Forwardが共通関数を使用
- Point Face選択順が一致
- Point / Spot FarがRangeと一致
- Shaderが`fxc`でコンパイル可能

---

## 9. 実機確認

### 9.1 Existing Scene

- 既存Sceneを変更せず開く
- `Param.w = 0.005`のReference Depth付近が変更前と同等であること
- 保存・再読込で値と見た目が変わらないこと

### 9.2 Distance

```text
Receiver Depth = 0.2 / 1 / 5 / 10 / 25 / 50
Range = 5 / 10 / 25 / 50 / 100 / 500
```

確認:

- 遠距離でShadowが突然消えない
- Peter Panningが距離に比例して拡大しない
- Rangeだけの変更で同じ受光点のBias量が急変しない

### 9.3 Slope

```text
Shadow Bias = 0 / 0.0005 / 0.001 / 0.005 / 0.01
Surface = Front Facing / 45 degrees / Grazing
```

確認:

- Acne低減
- Peter Panning量
- Point Face境界
- Deferred / Forward一致
- Static Batch / Ordinary Draw一致

---

## 10. 完了条件

- [x] `Param.w`保存互換を維持
- [x] Point / Spot距離補正を共通HLSLへ実装
- [x] Point Face基準のSlope Alignmentを実装
- [x] C++参照実装をHLSLと同じ契約へ統一
- [x] Smoke TestをReference Depth契約へ更新
- [x] Inspector表記をProjected-depth契約へ修正
- [ ] Lighting Diagnostic Contract成功
- [ ] Windows Build成功
- [ ] Existing Scene保存・再読込確認
- [ ] Range 5..500実機確認
- [ ] Point 6 Face境界確認
- [ ] Deferred / Forward確認
- [ ] Static / Dynamic Caster比較
