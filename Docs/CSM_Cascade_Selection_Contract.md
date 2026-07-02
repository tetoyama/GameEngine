# CSM Cascade Selection Contract

## 状態

**Deferred / Forward実装済み・CI / 実機確認待ち**

Directional CSMで受光点ごとに評価するCascadeを1つへ限定し、低解像度Cascadeによる上書きと重複PCF Costを除去する。

---

## 1. 確認した問題

旧`ShadowFactorCascades`は、受光点のXYが入るすべてのCascadeを順番に評価していた。

```text
for each cascade
    if receiver XY is inside
        run PCF
        finalShadow = min(finalShadow, shadow)
```

問題:

- CascadeのZ範囲を判定していない
- 近Cascadeと遠Cascadeが重なるPixelで複数回PCFを実行する
- 遠い低解像度Cascadeの結果が近い高解像度Cascadeを上書きできる
- 4 Cascade / PCF 5x5では、1 Logical CSM Lightあたり最大100 Comparison Sampleになる
- Cascade外の受光点Depthを`0..1`へSaturateして誤ったShadow比較を行える
- Raw DepthからOccluder位置を推定する補助分岐が必要になり、結果がCascade順とDepth誤差へ依存する

---

## 2. Cascade生成順

`ShadowMapPass`はCamera NearからFarへ、次の順でCascade Entryを格納する。

```text
Cascade 0 = 最も近い / 最も高い空間解像度
Cascade 1
Cascade 2
Cascade 3 = 最も遠い / 最も低い空間解像度
```

各CascadeはCamera Frustum Sliceの8 CornerからBounding Sphereを求め、Light View / Orthographic Projectionを生成する。

```text
splitNear .. splitFar
    -> 8 world-space corners
    -> center / radius
    -> Orthographic width = radius * 2
    -> Orthographic depth = 0 .. radius * 2
```

そのため、受光点を完全なLight NDC Volumeで判定し、最初に入るCascadeを選択できる。

---

## 3. 正式な選択契約

受光点をCascade順に変換する。

```text
clip = worldPosition * LightView * LightProjection
ndc  = clip.xyz / clip.w
uv   = ndc.xy * 0.5 + 0.5
uv.y = 1 - uv.y
```

有効条件:

```text
clip.w > 0
uv.x / uv.y in [-epsilon, 1 + epsilon]
ndc.z in [-epsilon, 1 + epsilon]
```

現在のepsilon:

```text
0.0001
```

有効な最初のCascadeを選ぶ。

```text
for cascade from near to far
    if complete XYZ volume contains receiver
        sample this cascade once
        return shadow

return fully lit
```

境界epsilonを通過したUV / DepthはSampling前に`0..1`へSaturateする。

---

## 4. なぜXYだけでは不十分か

各CascadeはCamera SliceごとにLight ViewのCenterが異なる。

受光点があるCascadeのXY範囲へ偶然入っていても、Light DepthではそのCascadeの前後に外れている場合がある。

旧経路は次を行っていた。

```text
outside XY -> skip
outside Z  -> 判定なし
cdepth     -> saturate
```

この場合、CascadeのNear / Far端へDepthが押し込まれ、正方形の偽Shadowまたは遠Cascade由来のShadowを生成できる。

受光点選択ではXYZすべてを判定する。

---

## 5. Caster Cullingとの関係

GPU Rasterizer:

```text
Directional / CSM: DepthClipEnable = false
Point / Spot:       DepthClipEnable = true
```

CPU Shadow Caster Culling:

```text
All Shadow Views:
    Left / Right / Bottom / Top = enabled
    Near / Far                  = disabled
```

CPU CullingはGPUより保守的にする。

Directional / CSMでは、受光点と同じLight-space XYへ投影されるNear / Far外Casterを保持する必要がある。

Point / Spotでも、大きいAABBまたはFar境界を跨ぐCasterをCPU側だけで先に除外すると、光は届くのに影だけ消える可能性がある。そのためGPUでPerspective Depth Clipを行いつつ、CPU CullingはNear / Far Planeを無効化したままにする。

この契約により、CSM Shaderが遠Cascadeを「Caster救済用」に追加評価する必要はない。

---

## 6. Performance上限

旧経路:

```text
CSM comparison samples per pixel
    = valid overlapping cascade count * PCF kernel samples

Worst case 4 cascades / 5x5
    = 4 * 25
    = 100 samples
```

新経路:

```text
CSM comparison samples per pixel
    = 1 selected cascade * PCF kernel samples

5x5
    = 25 samples
```

Matrix変換は最初の有効Cascadeまで必要だが、Shadow Texture Comparisonは最大1 Cascadeだけになる。

---

## 7. 削除した補助処理

旧経路にあった次を削除した。

- `finalShadow = min(finalShadow, shadow)`
- Litなら次Cascadeへ進むShadow結果依存選択
- Raw Shadow Depth Load
- Raw DepthからOccluder World Positionを推定
- Previous / First Cascadeへの再投影

Cascade選択はShadow結果ではなく、受光点のLight-space Volumeだけで決定する。

---

## 8. Deferred / Forward同期

対象:

```text
Source/Shader/Material/DeferredFunc.hlsli
Source/Shader/Material/FowardFunc.hlsli
```

両方で次を一致させる。

- Near -> FarのEntry順
- XYZ Volume判定
- Epsilon
- 最初の有効Cascadeで即時Return
- Atlas Tile内PCF Clamp
- Orthographic Shadow Bias

---

## 9. CI Guard

`Lighting Diagnostic Contract`で次を確認する。

- `outsideDepth`判定が存在
- 選択Cascadeから`return SampleShadowAtlasPCF`する
- `finalShadow = min`を禁止
- `ShadowMap.Load`によるCSM補助判定を禁止
- Deferred / Forwardを`fxc ps_5_0`でコンパイル

---

## 10. 実機確認

### Correctness

- Camera NearからFarまで前後移動
- Cascade 0 / 1境界
- Cascade 1 / 2境界
- Cascade 2 / 3境界
- Casterを受光点よりLight方向の手前へ離す
- 大きいCasterをCascade Near / Far境界へ跨がせる

期待:

- 遠Cascade由来の正方形Shadowが出ない
- 近CascadeのShadowが遠Cascadeで上書きされない
- Cascade外受光点でDepth Saturate由来のShadowが出ない
- Light方向上のNear / Far外Casterの影を維持

### Performance

同一Sceneで次を比較する。

```text
CSM ON / PCF 5x5
CSM ON / PCF 3x3
CSM ON / PCF 1x1
No CSM Shadow
```

記録:

- Lighting Average / P95
- ShadowMap Average / P95
- GPU Frame Average / P95
- Packed GPU Entry Count
- Shadow Entry Count

---

## 11. 完了条件

- [x] XYZ Cascade Volume判定
- [x] NearからFarの最初の有効Cascadeを選択
- [x] 1 Pixelにつき最大1 Cascade PCF
- [x] Shadow結果依存のCascade選択を削除
- [x] Raw Depth Occluder補助処理を削除
- [x] Deferred / Forward同期
- [ ] Lighting Diagnostic Contract成功
- [ ] Windows Build成功
- [ ] Cascade境界実機確認
- [ ] Near / Far外Caster確認
- [ ] PCF 1x1 / 3x3 / 5x5性能再測定
