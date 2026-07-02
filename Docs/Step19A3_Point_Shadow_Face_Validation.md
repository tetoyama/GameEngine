# Step 19-A.3 Point Shadow Face Validation

## 状態

**最優先Lighting調査へ挿入・生成側 / Atlas Tile境界安定化実装済み・実機確認待ち**

この工程はPoint Shadowの方向依存アーティファクトを確認し、Lighting性能測定の前提となるShadow Correctnessを確定する。

Far / NearとBiasの詳細検証は次へ分離した。

```text
Docs/Step19A4_Local_Shadow_Far_Validation.md
Docs/Step19A5_Shadow_Bias_Contract.md
```

---

## 1. 観測された可能性

Point LightのShadow Mapが、受光面または遮蔽物の方向によって不自然になる可能性がある。

Point Shadowは1 Logical Lightを次の6 Perspective Faceへ展開している。

```text
Face 0: +X
Face 1: -X
Face 2: +Y
Face 3: -Y
Face 4: +Z
Face 5: -Z
```

Shader側も同じ順序で、LightからWorld Positionへの方向の最大絶対成分からFaceを選択する。

---

## 2. Face基底の確認結果

生成側のDirection / Upは標準的なLeft-Handed Cube Face構成と一致していた。

| Face | Direction | Up |
|---:|---|---|
| 0 | +X | +Y |
| 1 | -X | +Y |
| 2 | +Y | -Z |
| 3 | -Y | +Z |
| 4 | +Z | +Y |
| 5 | -Z | +Y |

Shader側の選択順も一致している。

```text
abs(X)が最大: +X / -X
abs(Y)が最大: +Y / -Y
その他:       +Z / -Z
```

そのため、現時点でFace番号またはUpベクトルの単純な入れ替わりは確認されていない。

---

## 3. 確認した不安定要因

### 3.1 Directional用Depth Clampの共用

従来は全Shadow Typeで次のRasterizer Stateを使用していた。

```text
DepthClipEnable = FALSE
```

これはDirectional / CSMでLight Near Planeより手前のCasterを残すために利用していたが、Point / SpotはPerspective Projectionである。

Perspective ShadowでNear / Far外のGeometryを深度端へClampすると、Light位置とFace方向によって偽の遮蔽が発生する可能性がある。

### 3.2 90度ちょうどのFace FOV

従来のPoint Face FOV:

```text
90.0 degrees
```

主軸Face選択の境界とPerspective Projectionの端が完全に一致していた。

```text
abs(X) == abs(Y)
abs(Y) == abs(Z)
abs(Z) == abs(X)
```

この境界では浮動小数誤差によって、選択したFaceのUVが僅かに0..1外へ出てShadowなしとして扱われる可能性がある。

### 3.3 Atlas Tileを跨ぐPCF Tap

Point Shadowの6 FaceはTexture2D Atlas上の別Tileへ格納される。

PCF TapをAtlas全体だけでClampすると、Face端で隣接Tileの深度を比較し、方向依存の線や偽Shadowを作る。

---

## 4. 実装した修正

### 4.1 Face Layoutの一元化

追加:

```text
PointShadowFaceLayout.h
```

保持する契約:

- Face Count = 6
- Direction / Up
- Shaderと同じFace選択順
- Point Face Projection FOV

### 4.2 Rasterizer State分離

```text
Directional / CSM
    DepthClipEnable = FALSE

Point / Spot
    DepthClipEnable = TRUE
```

Static Batch Submission後も、現在のLight Typeに対応するRasterizer Stateへ復元する。

### 4.3 Face境界Overlap

Point Face FOVを次へ変更した。

```text
90.0 degrees -> 90.5 degrees
```

目的:

- 主軸切替境界をFace投影端より内側へ入れる
- 浮動小数誤差による面抜けを防ぐ
- Face間へ小さなOverlapを持たせる

この変更はFace順またはFace選択規則を変更しない。

### 4.4 Atlas Tile内PCF Clamp

Deferred / Forwardの`SampleShadowAtlasPCF`で、基準UVと各PCF Tapを現在TileのHalf-Texel内へClampする。

```text
sampleMin = tileMin + atlasTexelSize * 0.5
sampleMax = tileMax - atlasTexelSize * 0.5
sampleUV  = clamp(sampleUV, sampleMin, sampleMax)
```

これにより、PCF Kernelが隣接Faceまたは別LightのTileへ到達することを防ぐ。

Atlas Texel StepはFull Atlas基準で1 Texelずつ移動するため、Tile Grid数を追加で掛けない。

### 4.5 回帰テスト

追加:

```text
PointShadowFaceLayoutSmokeTest.cpp
```

検証内容:

- 6 FaceのDirection / Upが単位ベクトル
- DirectionとUpが直交
- Axis Centerが正しいFaceへ対応
- X -> Y -> ZのTie-BreakがShaderと一致
- Edge / Corner方向が90.5度Projection内に収まる
- FOVが90度より大きく91度以下

WorkflowではDeferred / Forward ShaderのFace番号順と、共通Shadow Bias関数の使用も検査する。

---

## 5. 実機確認シーン

Point Lightを原点付近へ配置し、6方向に同一形状のCasterとReceiverを配置する。

```text
+X / -X
+Y / -Y
+Z / -Z
```

追加で次の境界方向を確認する。

```text
XY Edge
XZ Edge
YZ Edge
XYZ Corner
```

### 確認操作

1. Point LightのCastShadowをON
2. PCF 1x1で各Face中心を確認
3. PCF 3x3 / 5x5でFace境界を確認
4. Point LightをCasterへ近づけ、Near Plane付近を確認
5. CasterをLight Range外へ移動し、偽Shadowが残らないことを確認
6. カメラを移動してもShadow形状が変化しないことを確認
7. Far境界はStep 19-A.4のRange境界テストを実行
8. Bias距離依存はStep 19-A.5のReference Depthテストを実行

---

## 6. 合格条件

- 6方向すべてで同じ形状のShadowになる
- +Y / -Yだけが回転または反転して見えない
- Face境界でShadowが瞬間的に消えない
- Lightを移動したとき境界に固定された線が出ない
- Near Plane付近のGeometryがFace全体を遮蔽しない
- Range外GeometryがFar PlaneへClampされて偽Shadowを作らない
- PCF Tapが隣接Atlas Tileの深度を参照しない
- Static Batchと通常Drawで結果が一致する
- DeferredとForwardで結果が一致する

---

## 7. 残る場合の次の切り分け

Tile内Clamp後もFace境界に線が残る場合、原因は隣接Tile混入ではなく、Face選択の不連続または各Face Projection間の差である可能性が高い。

次候補:

1. PCF 1x1で線が残るか確認し、Filtering問題とProjection問題を分離
2. 選択Face Index / UV / Projected DepthのDebug可視化
3. Face境界で両Faceを評価して結果を比較
4. 境界帯だけ隣接Face SamplingまたはBlend
5. Casterが複数Faceへ同じ条件で描画されているか確認
6. Static Batchと通常DrawのWorld / View / Projection一致確認
7. 将来的にTextureCubeまたはTexture2DArrayへ移行

隣接Face SamplingやBlendはShadow Sample数を増やすため、PCF 1x1でもFace境界問題が残ることを確認してから導入する。

---

## 8. 実装状況

- [x] C++ Face Direction / Up確認
- [x] Deferred Face選択順確認
- [x] Forward Face選択順確認
- [x] Point Face Layout一元化
- [x] Perspective Shadow Rasterizer分離
- [x] Point Face FOV Overlap
- [x] Atlas Tile Half-Texel PCF Clamp
- [x] Face Layout Smoke Test
- [x] Shader Face順Workflow Guard
- [x] Far / Near検証をStep 19-A.4へ分離
- [x] Bias検証をStep 19-A.5へ分離
- [ ] Windows Build確認
- [ ] Lighting Diagnostic Contract確認
- [ ] 6 Axis実機確認
- [ ] Edge / Corner実機確認
- [ ] Near / Far Clip実機確認
- [ ] PCF 1x1 / 3x3 / 5x5比較
- [ ] Static / Dynamic Caster比較
- [ ] Deferred / Forward比較
- [ ] 必要時のみ隣接Face Sampling検討
