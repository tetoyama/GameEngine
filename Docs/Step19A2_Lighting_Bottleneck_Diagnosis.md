# Step 19-A.2 Lighting Bottleneck Diagnosis

## 状態

**最優先・Packed走査改善確認済み・Shadow種別分解中**

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

想定構成:

```text
Logical 0: CSM   / 4 Cascade Entries
Logical 1: Point / 6 Face Entries
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

今回の画像ではPost Effectが0.0000msだったため、旧GPU Frame全体とは直接比較しない。Player Lighting同士を比較する。

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

Shadow Map生成は約4msのままであり、主要コストはLighting Shader内に残っている。

### 5.3 Logical Lights 1

Logical Lights 1は13.2291msで、No Shadow 13.0765msとほぼ同じだった。

```text
Baseline - Logical Lights 1 = 15.8955ms
```

この結果から、除外された2個目のLogical Lightが大きなコストを持つ可能性が高い。

ただし現時点の画像ではLogical 0 / 1のLight Typeを表示していないため、Point Lightであるとはまだ断定しない。

### 5.4 PCF 1x1

PCF 1x1は29.8199msで、Baseline 29.1246msより改善しなかった。

可能性:

- PCF Sample数よりCascade / Face選択、Matrix計算、Buffer読込が支配的
- Point Shadow経路が支配的
- Material Shader BinaryへのPCF Override反映を追加確認する必要がある
- 単一Frameの揺らぎ

この時点では既定PCFを3x3へ変更しない。

### 5.5 Environment

No Environmentは29.1615msでBaselineとの差は0.04msだった。

Environment最適化は後順位のままとする。

---

## 6. 次の診断実装

追加済み:

- `LIGHTING_DEBUG_FLAG_DISABLE_CSM_SHADOWS`
- `LIGHTING_DEBUG_FLAG_DISABLE_POINT_SHADOWS`
- `No CSM Shadow` Preset
- `No Point Shadow` Preset
- Logical Light Layout表示
- Logical LightごとのType / First Entry / Span / Shadow状態表示

UI表示例:

```text
Logical 0: Directional CSM / Entry 0 / Span 4 / Shadow ON
Logical 1: Point           / Entry 4 / Span 6 / Shadow ON
```

これによりLogical Lights 1の意味を推測ではなく実データで確認する。

---

## 7. 次の実機測定順

最新ブランチをビルドし、Material Shaderを再コンパイルしてから測る。

優先順:

1. Logical Light Layoutのスクリーンショット
2. Baseline
3. No CSM Shadow
4. No Point Shadow
5. No Shadow
6. Logical Lights 1
7. Logical Lights 2
8. PCF 1x1
9. PCF 3x3
10. PCF 5x5

60 Frame Warm-up後、120 Resolved SampleのAverage / P95を使用する。

### 判定

#### No Point Shadowで大幅低下

Point Shadowが主因。

次工程:

- Point Light範囲外PixelのShadow評価前Reject
- Point Light VolumeまたはTiled Light Culling
- Point Shadow更新頻度制限
- Point Shadow Face MatrixをShadow専用Bufferへ分離

#### No CSM Shadowで大幅低下

CSMが主因。

次工程:

- Cascadeを深度から直接選択
- 全Cascade fallback探索を通常経路から除去
- 境界Pixelだけ隣接Cascadeを評価
- PCF品質設定

#### PCF差が引き続き小さい

Sample数ではなくMatrix / Branch / Buffer Accessが支配的と判断する。

---

## 8. 中期設計

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

## 9. 実装状況

- [x] GPU Pass単位Timestamp
- [x] 初期Baseline / No Shadow / No Environment
- [x] Packed Entry混在問題を特定
- [x] Logical Light単位Shader走査
- [x] Shadow Atlas Offset Correctness修正
- [x] New Baseline取得
- [x] Packed走査で約24.8%改善を確認
- [x] New No Shadow取得
- [x] New Logical Lights 1取得
- [x] PCF 1x1参考値取得
- [x] No Environment再確認
- [x] CSM / Point Shadow個別Toggle
- [x] Logical Light Layout Telemetry
- [x] Settings / Packed Traversal Smoke更新
- [ ] Windows Build確認
- [ ] Lighting Diagnostic Contract確認
- [ ] Logical Light Type確認
- [ ] No CSM Shadow測定
- [ ] No Point Shadow測定
- [ ] 120 Sample Average / P95取得
- [ ] 恒久Shadow最適化
- [ ] LogicalLightBuffer / ShadowEntryBuffer分離
- [ ] Post Effect内訳計測

---

## 10. 完了条件

- `switch(materialID)`統合を使用していない
- CSM CascadeとPoint FaceをPacked Entry Loopで重複走査しない
- Logical Light制限でPacked Entryを途中切断しない
- 法線早期continue後もShadow Atlas Offsetが一致する
- CSM / Point Shadowコストを個別に測定できる
- Default設定で変更前と同じ見た目になる
- 次の恒久Shadow最適化をAverage / P95に基づいて選択する
