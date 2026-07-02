# Shadow Atlas PCF Contract

## 状態

**Deferred / Forward統一済み・CI / 実機確認待ち**

Point / Spot / Directional / CSMのShadow Atlas Samplingを同一契約へ統一する。

---

## 1. 解決する問題

Shadow Mapは1枚のTexture2D Atlasへ複数Shadow EntryをTile配置している。

```text
Atlas
    Tile 0: CSM Cascade 0
    Tile 1: CSM Cascade 1
    Tile 2: CSM Cascade 2
    Tile 3: CSM Cascade 3
    Tile 4..9: Point Face 0..5
```

PCF Samplingでは次の二つを区別する必要がある。

```text
Atlas Texel Size = 1 / Full Atlas Dimensions
Tile UV Size     = 1 / Atlas Grid Count
```

旧CSM経路は次の計算を行っていた。

```text
texelSize = (1 / Atlas Dimensions) * Tile UV Size
```

これはTile比率を二重に掛けており、PCF Offsetが実際の1 Atlas Texelより小さくなる。

また、旧CSM経路はPCF Tapを現在Tile内へClampしていなかったため、Cascade端で隣接TileのDepthを参照できた。

---

## 2. 正式な座標契約

### 2.1 Atlas Grid

```text
atlasCount = max(ShadowAtlasCount, 1)
grid       = ceil(sqrt(atlasCount))
tileSize   = 1 / grid
```

### 2.2 Tile Index

CPU `ShadowMapPass`は次のEntryだけで`shadowNum`を進める。

```text
Enable != 0 && CastShadow != 0
```

Shader側`ResolveShadowAtlasTileForEntry`も同じ規則で先行Entryを数える。

### 2.3 Full Atlas Texel

```text
atlasTexelSize = float2(
    1 / atlasTextureWidth,
    1 / atlasTextureHeight)
```

PCFの`StepTexel = 1`はFull Atlas上の実Texture Texelを1個移動する。

`atlasTexelSize`へ`tileSize`を掛けない。

### 2.4 Tile Safe Range

```text
tileMin  = tileCoordinate * tileSize
tileMax  = tileMin + tileSize
sampleMin = tileMin + atlasTexelSize * 0.5
sampleMax = tileMax - atlasTexelSize * 0.5
```

基準UV:

```text
suvBase = clamp(
    tileMin + saturate(localUV) * tileSize,
    sampleMin,
    sampleMax)
```

各PCF Tap:

```text
sampleUV = clamp(
    suvBase + int2Offset * atlasTexelSize * StepTexel,
    sampleMin,
    sampleMax)
```

これによりFilter Kernelが隣接Cascade、Point Face、別LightのTileへ侵入しない。

---

## 3. 共通Shader責務

Deferred / Forwardは同じ関数構造を持つ。

```text
ResolveShadowAtlasSampleBase
    -> Atlas Count / Grid / Tile
    -> Full Atlas Texel Size
    -> Half-Texel Safe Range
    -> Safe Base UV

SampleShadowAtlasPCFResolved
    -> Safe Base UV
    -> PCF Kernel
    -> TapごとのTile Clamp

SampleShadowAtlasPCF
    -> 通常Shadow用Adapter
```

対象:

```text
Source/Shader/Material/DeferredFunc.hlsli
Source/Shader/Material/FowardFunc.hlsli
```

Point / Spotは`SampleShadowAtlasPCF`を使用する。

CSMはCascade判定後に`ResolveShadowAtlasSampleBase`を一度実行し、`SampleShadowAtlasPCFResolved`とRaw Depth参照で同じSafe Base UVを共有する。

---

## 4. CSM Raw Depth参照

CSMは現在CascadeのDepthからOccluder位置を推定し、前Cascadeとの整合を確認する補助経路を持つ。

Raw DepthのLoad座標もAtlas範囲内へClampする。

```text
safeLoadCoord = clamp(
    int2(suvBase * atlasDimensions),
    int2(0, 0),
    int2(atlasDimensions) - 1)
```

Projection Z Scaleが0へ近い場合の除算も下限値で保護する。

---

## 5. 適用範囲

| Light Type | Projection | Sampling |
|---|---|---|
| Directional | Orthographic | Atlas Tile Clamp |
| Directional CSM | Orthographic / Cascade | Atlas Tile Clamp + Raw Depth Clamp |
| Spot | Perspective | Atlas Tile Clamp |
| Point | 6 Perspective Face | Atlas Tile Clamp |

Bias計算は別契約であり、この文書では変更しない。

```text
Docs/Step19A5_Shadow_Bias_Contract.md
```

Point Face選択とFOVは次を参照する。

```text
Docs/Step19A3_Point_Shadow_Face_Validation.md
```

---

## 6. CI Guard

`Lighting Diagnostic Contract`でDeferred / Forwardの双方について次を確認する。

- `ResolveShadowAtlasSampleBase`が存在
- `SampleShadowAtlasPCFResolved`が存在
- Half-Texel Safe Rangeを計算
- CSM専用`texelSize * tile`計算を禁止
- Deferred / Forward Shaderを`fxc ps_5_0`でコンパイル

---

## 7. 実機確認

### 7.1 PCF Kernel

```text
PCF 1x1
PCF 3x3
PCF 5x5
```

期待:

- Kernelを広げると実際のAtlas Texel単位でShadow Edgeが変化する
- 1x1と3x3が完全に同じ形状にならない
- CascadeまたはFace境界で別Tileの形状が混入しない

### 7.2 CSM Cascade境界

- Cascade 0 / 1境界
- Cascade 1 / 2境界
- Cascade 2 / 3境界
- Camera移動
- CasterがCascade境界を横切る状態

期待:

- 正方形の偽Shadowが出ない
- 隣接CascadeのDepthが混入しない
- Deferred / Forwardで境界位置が一致

### 7.3 Point Face境界

- XY / XZ / YZ Edge
- XYZ Corner
- LightをCasterへ近づける
- Rangeを5..500で変更

期待:

- 隣接Atlas TileのDepthが混入しない
- Face境界に固定された線が出ない
- 残る線はFace Projection / Face選択問題として切り分け可能

---

## 8. 完了条件

- [x] Full Atlas Texel Sizeへ統一
- [x] Point / Spot Tile Half-Texel Clamp
- [x] CSM Tile Half-Texel Clamp
- [x] CSM Raw Depth Load Clamp
- [x] Deferred / Forward共通関数構造
- [x] CI Regression Guard追加
- [ ] Lighting Diagnostic Contract成功
- [ ] Windows Build成功
- [ ] PCF 1x1 / 3x3 / 5x5実機比較
- [ ] CSM Cascade境界実機確認
- [ ] Point Face境界実機確認
- [ ] Static Batch / Ordinary Draw比較
