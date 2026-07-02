# Step 19-A.2 Lighting Bottleneck Diagnosis

## 状態

**最優先・Packed走査改善確認済み・Light/Shadow参加分離の実機確認待ち**

診断UI:

```text
Project Settings
    -> Lighting
```

診断設定はRuntime専用であり、Scene YAMLまたはProject YAMLへ保存しない。

---

## 1. Material Shader方針

`switch(materialID)`でMaterial実装を一つのDeferred Shaderへ統合しない。

過去の実機検証では同一Wave内のMaterial分岐が直列化され、統合前より重くなった。Material別Deferred Pixel Shaderを維持する。

Material別Full Screen Drawが支配的になった場合も、候補はStencil TestによるPixel Shader起動前制限とし、Material ID動的分岐へ戻さない。

診断分岐はDraw全体で均一な値だけを使用する。

許可する診断項目:

- 全Shadow評価無効
- CSM Shadow評価無効
- Point Shadow評価無効
- PCF Kernel Radius Override
- Environment Reflection無効
- 最大Logical Light数

---

## 2. 初期Baseline

2026-06-30の初期Pass計測:

| Scope | GPU Time | GPU Frame比 |
|---|---:|---:|
| GPU Frame | 61.6301ms | 100% |
| Player Lighting | 39.6063ms | 約64.3% |
| Player Post Effect | 17.5012ms | 約28.4% |
| Player Shadow Map生成 | 2.1187ms | 約3.4% |
| Player GBuffer | 0.8786ms | 約1.4% |
| ImGui | 0.0512ms | 約0.1% |

LightingとPost EffectでGPU Frameの約92.7%を占めたため、Lightingを先に処理する。

---

## 3. 第1回A/B測定

Packed Entry走査修正前の参考値:

| 条件 | GPU Frame | Player Lighting | Player Shadow | Player Post | Lighting差 |
|---|---:|---:|---:|---:|---:|
| Baseline | 72.6944ms | 38.7113ms | 3.0341ms | 17.5688ms | - |
| No Shadow | 52.6815ms | 16.9738ms | 3.1539ms | 17.1909ms | -21.7375ms / -56.2% |
| Old Lights 1 | 36.7918ms | 4.2906ms | 2.9788ms | 17.0148ms | -34.4207ms / -88.9% |
| No Environment | 71.1808ms | 38.9612ms | 3.0536ms | 16.9544ms | +0.2499ms / +0.6% |

確定事項:

- Lighting Shader内Shadow評価が主要因
- Environment Reflectionは主要因ではない
- Old Lights 1はLogical Light比較として無効

Old Lights 1は`ActiveLightCount`をPacked Entry数として1へ切り詰め、CSMを1 Cascadeへ途中切断していた。

---

## 4. Packed Entry構造問題

測定時は次の状態だった。

```text
Packed GPU entries: 10
Shadow-casting entries: 10
```

構成:

```text
Logical 0: Directional CSM / 4 Cascade Entries
Logical 1: Point           / 6 Face Entries
Total: 10 Packed Entries
```

旧Shaderは各Pixelで全Packed Entryを順番に読み、後続Cascade / Faceを`LIGHT`構造体読込後にskipしていた。

`LIGHT`はView / Projection Matrixを含むため、使用しないEntryの大きなBuffer読込が全画面で発生していた。

### 実装した修正

- `PackedLightEntryTraversal.h`
- CSM / Point Entry Span契約
- Packed Entry数からLogical Light数を算出
- Logical Light上限から保持するPacked Entry境界を算出
- Shader LoopをPacked Entry単位の`for`からLogical Light単位の`while`へ変更
- `NdotL <= 0`前にShadow Atlas Offsetを進める
- 通常ShadowもPacked IndexではなくAtlas OffsetでTileを選択
- 診断上限を`Maximum Logical Lights`へ変更

新しいShader Loop:

```text
entryIndex = 0
while entryIndex < ActiveLightCount
    load first entry of logical light
    resolve CSM / Point span
    evaluate logical light once
    entryIndex += span
```

---

## 5. 第2回A/B測定

Packed Entry走査修正後の単一Resolved Frame参考値。

この計測ではPost Effectが0.0000msだったため、旧GPU Frame全体とは直接比較しない。Player Lighting同士を比較する。

| 条件 | GPU Frame | Player Lighting | Player Shadow | Baseline比Lighting差 |
|---|---:|---:|---:|---:|
| New Baseline | 44.4117ms | 29.1246ms | 4.5804ms | - |
| New No Shadow | 27.3395ms | 13.0765ms | 4.0950ms | -16.0481ms / -55.1% |
| New Logical Lights 1 | 28.5939ms | 13.2291ms | 4.3223ms | -15.8955ms / -54.6% |
| PCF 1x1 | 45.1429ms | 29.8199ms | 4.4810ms | +0.6953ms / +2.4% |
| No Environment | 44.7320ms | 29.1615ms | 4.0346ms | +0.0369ms / +0.1% |

### 5.1 Packed走査修正の効果

旧Baseline 38.7113msからNew Baseline 29.1246msへ低下した。

```text
改善量  = 9.5867ms
改善率  = 24.8%
```

単一Frame比較のため最終確定値ではないが、Packed Entry反復走査が主要因の一つだったことを確認した。

### 5.2 Shadow評価

New No Shadowで29.1246msから13.0765msへ低下した。

```text
Shadow-side cost ~= 16.0481ms
```

Packed走査修正後もLighting時間の約55%をShadow評価が占める。

### 5.3 Logical Lights 1

Logical Lights 1は13.2291msで、No Shadow 13.0765msとほぼ同じだった。

```text
Baseline - Logical Lights 1 = 15.8955ms
```

後続のLayout表示により、Logical 1はPoint Light、Spanは6 Faceであることを確認した。

### 5.4 PCF 1x1

PCF 1x1は29.8199msで、Baseline 29.1246msより改善しなかった。

現時点ではPCF Sample数よりも、Point Face処理、Cascade選択、Matrix計算、Buffer Accessが支配的な可能性が高い。既定PCFを3x3へ変更しない。

### 5.5 Environment

No Environmentは29.1615msでBaselineとの差は0.04msだった。Environment最適化は後順位のままとする。

---

## 6. CastShadow既定値とLight参加の結合問題

### 6.1 観測

2026-06-30のLogical Light Layout:

```text
Logical 0: Directional CSM / Entry 0 / Span 4 / Shadow ON
Logical 1: Point           / Entry 4 / Span 6 / Shadow ON
```

Scene内の2 Logical Lightが両方ともShadow ONになっていた。

### 6.2 原因

2つの問題が結合していた。

1. `LightComponent`の新規作成既定値が`CastShadow = true`
2. `ShadowMapPass`が`CastShadow == false`のLightをGPU Light Bufferへ登録する前に除外

旧契約:

```text
CastShadow OFF
    -> Shadowを生成しない
    -> Light Bufferにも入らない
    -> 照明自体が消える
```

このためLightを通常利用するだけでもCastShadowをONにする必要があり、Point Lightが自動的に6 Faceへ展開されやすい状態だった。

### 6.3 根本修正

追加:

- `LightComponentDefaults.h`
- `LightGpuSubmissionPolicy.h`
- `LightShadowParticipationSmokeTest.cpp`

新契約:

```text
Enable OFF
    -> Lightingへ送らない
    -> Shadowも生成しない

Enable ON / CastShadow OFF
    -> 1 Logical LightとしてLightingへ送る
    -> Shadow Atlas Entryは生成しない

Enable ON / CastShadow ON
    -> Lightingへ送る
    -> Light Typeに応じてShadow Entryへ展開する
```

`ShadowMapPass`は全Enabled LightをLighting Submission対象とし、CastShadow ONのLightだけを展開する。

### 6.4 新規Lightの既定値

```text
Enable:      ON
LightType:   Point
CastShadow:  OFF
```

Point Shadowは必要なLightだけInspectorで明示的にONにする。

### 6.5 既存Sceneの互換性

Scene YAMLに明示的に保存済みの`CastShadow: true`は変更しない。

旧既定値によって保存されたのか、ユーザーが意図的にONにしたのかを自動判別できないため、一括でOFFへ変換しない。

Point LightごとにInspectorで確認する。

### 6.6 Inspector警告

- CastShadow OFFでもLightは有効なままであることをTooltip表示
- Point Shadowは6 Atlas Faceへ展開されることを表示
- CSM ShadowはCascade数分へ展開されることを表示

---

## 7. Local Light範囲外Pixelの早期終了

Point / Spot Lightは`attenuation == 0`でもShadow評価とBRDF計算を続けていた。

DiffuseとSpecularは最終的に0になるため、既存のAmbient加算だけを保持して早期終了する。

```text
if attenuation <= 0
    accumulate ambient
    skip shadow sampling
    skip BRDF
```

Point Lightが画面の一部だけを照らすSceneで効果が期待できる。

---

## 8. 添付された第3回スクリーンショットの扱い

Layout情報は有効:

```text
Logical Lights: 2
Shadow Logical Lights: 2
Shadow Entries: 10
CSM Span: 4
Point Span: 6
```

一方、多くの画像で`GPU status: Pending`となっている。

`GPU latest resolved`は現在表示中のPresetより前のFrameである可能性があるため、Baseline / No Shadow / No CSM Shadow等の単一Frame数値は直接比較しない。

性能判定には診断UIの次を使用する。

```text
60 Frame Warm-up
120 Resolved Samples
Average / P95
```

---

## 9. 次の実機確認

最新ブランチをビルドし、Material Shaderを再コンパイルする。

### 9.1 Correctness確認

1. Point Lightを選択
2. CastShadowをOFF
3. Point Lightの明るさが残ることを確認
4. Point Shadowだけ消えることを確認
5. Scene保存・再読込後もOFFが維持されることを確認

### 9.2 期待Telemetry

CSM Shadow ON、Point Shadow OFF:

```text
Packed GPU entries:      5
Logical lights:          2
Shadow logical lights:   1
Shadow entries:          4

Logical 0: Directional CSM / Entry 0 / Span 4 / Shadow ON
Logical 1: Point           / Entry 4 / Span 1 / Shadow OFF
```

CSM / PointともにShadow OFF:

```text
Packed GPU entries:      2
Logical lights:          2
Shadow logical lights:   0
Shadow entries:          0
```

### 9.3 性能測定順

1. CSM ON / Point OFF Baseline
2. CSM ON / Point ON Baseline
3. No CSM Shadow
4. No Point Shadow
5. No Shadow
6. PCF 1x1 / 3x3 / 5x5

各条件で60 Frame Warm-up後、120 Resolved SampleのAverage / P95を記録する。

---

## 10. 中期設計

現在の手動Span走査は互換修正であり最終形ではない。

```text
LogicalLightBuffer
    one entry per Directional / Point / Spot light
    color / direction / attenuation / shadow metadata index

ShadowEntryBuffer
    one entry per CSM cascade / point face / spot map
    view / projection / atlas tile / bias
```

Lighting Loopは`LogicalLightBuffer`だけを走査し、Shadow評価時だけ`ShadowEntryBuffer`を参照する。

---

## 11. 実装状況

- [x] GPU Pass単位Timestamp
- [x] 初期Baseline / No Shadow / No Environment
- [x] Packed Entry混在問題を特定
- [x] Logical Light単位Shader走査
- [x] Shadow Atlas Offset Correctness修正
- [x] Packed走査で約24.8%改善を確認
- [x] CSM / Point Shadow個別Toggle
- [x] Logical Light Layout Telemetry
- [x] CSM 4 Entry / Point 6 Entryを実機確認
- [x] LightとShadow参加の結合問題を特定
- [x] 非Shadow Lightを単一Logical Entryとして保持
- [x] 新規LightのCastShadow既定値をOFFへ変更
- [x] Point / Spot範囲外PixelのShadow / BRDF早期終了
- [x] InspectorへShadow展開コストを表示
- [x] Light Shadow Participation Smoke追加
- [x] ShadowMapPass旧除外処理のソース契約追加
- [ ] Windows Build確認
- [ ] Lighting Diagnostic Contract確認
- [ ] Point CastShadow OFFでLightが残る実機確認
- [ ] Packed 5 / Shadow 4 Telemetry確認
- [ ] Scene保存・再読込確認
- [ ] 120 Sample Average / P95取得
- [ ] 恒久Shadow最適化
- [ ] LogicalLightBuffer / ShadowEntryBuffer分離
- [ ] Post Effect内訳計測

---

## 12. 完了条件

- `switch(materialID)`統合を使用していない
- CSM CascadeとPoint FaceをPacked Entry Loopで重複走査しない
- Logical Light制限でPacked Entryを途中切断しない
- CastShadow OFFでもLightがLightingへ参加する
- Point Shadow OFF時に6 Face Entryが生成されない
- 新規LightのCastShadowが既定OFF
- Scene保存・再読込でCastShadow設定が維持される
- Default設定で意図した見た目になる
- 次の恒久Shadow最適化をAverage / P95に基づいて選択する
