# Step 19-A.2 Lighting Bottleneck Diagnosis

## 状態

**最優先・原因特定済み・構造修正中**

2026-06-30のPass単位計測により、Player LightingがGPU Frameの主要ボトルネックであることを確認した。

初期Baseline:

| Scope | GPU Time | GPU Frame比 |
|---|---:|---:|
| GPU Frame | 61.6301ms | 100% |
| Player Lighting | 39.6063ms | 約64.3% |
| Player Post Effect | 17.5012ms | 約28.4% |
| Player Shadow Map生成 | 2.1187ms | 約3.4% |
| Player GBuffer | 0.8786ms | 約1.4% |
| ImGui | 0.0512ms | 約0.1% |
| Unaccounted GPU | 1.4742ms | 約2.4% |

LightingとPost EffectでGPU Frameの約92.7%を占めるため、Lightingを先に処理する。

診断UI:

```text
Project Settings
    -> Lighting
```

診断設定はRuntime専用であり、Scene YAMLまたはProject YAMLへ保存しない。

---

## 1. Material Shader方針

### 1.1 `switch(materialID)`統合を使用しない

過去の実機検証で、Material IDによる統合Shader分岐は軽量化しなかった。

同一Wave内に異なるMaterialが混在すると複数経路が直列化される可能性があるため、次を禁止する。

- `switch(materialID)`による全Material関数の統合
- 動的`if / else if`によるMaterial実装選択
- `[branch]`指定だけを根拠とした統合
- Material統合とShadow最適化の同時変更

Material別Deferred Pixel Shaderは維持する。

Material別Full Screen Drawが支配的だと判明した場合も、候補はStencil TestによるPixel Shader起動前制限とし、Material ID分岐へ戻さない。

### 1.2 診断分岐はDraw-uniform値だけを使う

許可する診断項目:

- Shadow評価の全体無効化
- PCF Kernel Radiusの全体Override
- Environment Reflectionの全体無効化
- 評価する最大Logical Light数

Pixelごとに異なる診断分岐は追加しない。

---

## 2. 2026-06-30 A/B測定結果

同一Scene、Camera、概ね同じPlayer Viewサイズ、VSync OFFで取得した単一Resolved Frameの参考値。

| 条件 | GPU Frame | Player Lighting | Player Shadow | Player Post | Baseline比 Lighting差 |
|---|---:|---:|---:|---:|---:|
| Baseline | 72.6944ms | 38.7113ms | 3.0341ms | 17.5688ms | 0.0000ms |
| No Shadow | 52.6815ms | 16.9738ms | 3.1539ms | 17.1909ms | -21.7375ms / -56.2% |
| Old Lights 1 | 36.7918ms | 4.2906ms | 2.9788ms | 17.0148ms | -34.4207ms / -88.9% |
| No Environment | 71.1808ms | 38.9612ms | 3.0536ms | 16.9544ms | +0.2499ms / +0.6% |

### 2.1 No Environment

`No Environment`でPlayer Lightingは低下せず、約0.25ms増加した。

この差は単一Frame計測の揺らぎの範囲と判断する。

結論:

- Environment Reflectionは現在の39ms問題の主要因ではない
- Prefiltered Environment Map化は後順位へ移す
- Environment経路は今回の恒久最適化対象に選ばない

### 2.2 No Shadow

`No Shadow`でPlayer Lightingが38.7113msから16.9738msへ低下した。

差分:

```text
Lighting-side Shadow evaluation cost ~= 21.7375ms
```

Shadow Map生成時間は約3msのままなので、主要コストはShadow Map生成ではなくLighting Shader内のShadow参照、PCF、CSM fallback、Point Face選択にある。

結論:

- Shadow参照は明確な主要因
- PCF 1x1 / 3x3 / 5x5比較は引き続き必要
- Shadow参照を完全に切るだけでは残り約17msのLighting負荷が残る

### 2.3 Old Lights 1

旧`Lights 1`は`ActiveLightCount`をPacked GPU Entry数として1へ切り詰めていた。

測定時のBuffer:

```text
Packed GPU entries: 10
Shadow-casting GPU entries: 10
```

10 EntryはLogical Light 10個とは限らず、CSM CascadeとPoint Shadow Faceを含む。

想定構成:

```text
CSM:   4 Cascade Entries
Point: 6 Face Entries
Total: 10 Packed Entries
Logical Lights: 2
```

旧PresetはCSMを1 Cascadeへ切断していたため、`4.2906ms`を「Logical Light 1個のコスト」として扱ってはならない。

ただし、Packed Entry数を減らすとLightingが大幅に低下した事実から、次の構造問題を確認できた。

- Pixelごとに全Packed Entryを順次走査していた
- CSM後続CascadeとPoint後続Faceも`LIGHT`構造体を読み込んでからskipしていた
- `LIGHT`はView / Projection Matrixを含み大きい
- 使用しないShadow Entryの反復読込が全画面で発生していた

---

## 3. 確定した主要因

現時点の優先順位:

1. **Packed Shadow EntryとLogical Lightの混在走査**
2. **Lighting Shader内Shadow評価とPCF**
3. Post Effect
4. Material別Full Screen Draw
5. Environment Reflection

Light Loop問題は単純な「Light数が多い」問題ではない。

```text
Logical Light
    -> Shadow Atlas用に複数Packed Entryへ展開
    -> Lighting ShaderがPacked EntryをLogical Lightとして走査
```

というデータモデル混在が原因である。

---

## 4. 実装した構造修正

### 4.1 Logical Light単位のPacked Entry走査

追加:

- `PackedLightEntryTraversal.h`
- CSM / PointのEntry Span解決
- Packed Entry数からLogical Light数を算出
- Logical Light上限から保持すべきPacked Entry数を算出

契約:

```text
CSM first entry   Dummy == 1
    span = Position.w cascade count

Point first entry Dummy == -1
    span = Position.w face count

Ordinary light
    span = 1
```

### 4.2 Shader Loop

旧処理:

```text
for each packed entry
    load full LIGHT
    continuation entryならskip
```

新処理:

```text
entryIndex = 0
while entryIndex < ActiveLightCount
    load first entry of logical light
    resolve entry span
    evaluate logical light once
    entryIndex += span
```

これにより、CSM 4 CascadeとPoint 6 Faceの構成は10回のLogical Light評価ではなく2回になる。

### 4.3 Shadow Atlas OffsetのCorrectness修正

旧処理は`NdotL <= 0`で早期continueすると、Logical Lightが占有するShadow Atlas Entry数を進めない場合があった。

その結果、後続Lightが誤ったAtlas Tileを参照する可能性があった。

新処理では次をLighting計算より先に確定する。

- Current Entry Index
- Entry Span
- Current Shadow Atlas Offset
- Next Entry Index
- Next Shadow Atlas Offset

法線方向による早期continueが発生してもAtlas Offsetは正しく進む。

### 4.4 診断Presetの意味修正

`Maximum Active Lights`を`Maximum Logical Lights`へ変更した。

```text
Lights 1
    -> CSMなら全Cascadeを保持したLogical Light 1個

Lights 2
    -> CSM + Pointなら全10 Packed Entryを保持したLogical Light 2個
```

CSMまたはPointを途中で切断しない。

### 4.5 UI Telemetry

Project Settings > Lightingへ次を表示する。

- Player / Editor Resolution
- Player / Editor Pixel Count
- Packed GPU Entry Count
- Logical Light Count
- Shadow Logical Light Count
- Shadow Entry Count

---

## 5. 次の実機測定

構造修正後は旧結果と直接比較せず、新Baselineを取り直す。

優先順:

1. New Baseline
2. New No Shadow
3. New Logical Lights 1
4. New Logical Lights 2
5. PCF 1x1
6. PCF 3x3
7. PCF 5x5 / Material Default

今回のSceneがLogical Light 2個なら、Lights 4 / 8 / Allは同じ結果になるため必須ではない。

60 Frame Warm-up後、120 Resolved SampleのAverageとP95を記録する。

記録項目:

- GPU Frame Average / P95
- Player Lighting Average / P95
- Player Shadow Average / P95
- Player Post Effect Average / P95
- Player Resolution / Pixel Count
- Packed Entry Count
- Logical Light Count
- Shadow Logical Light Count

### 判定

#### New Baselineが大幅に低下

Packed Entry反復走査が主要因だったと確定する。

#### Logical Lights 1と2の差が大きい

Point Lightまたは2個目のLogical Lightが主要因。

次にPoint Shadowのみ無効化する診断を追加する。

#### New No Shadowとの差が大きい

Shadow参照が引き続き主要因。

PCF縮小とCSM Cascade選択最適化へ進む。

#### 1x1 / 3x3 / 5x5がSample数に比例

既定品質を3x3、Low品質を1x1とする候補を検討する。

---

## 6. 中期設計

現在の手動Span走査は互換修正であり、最終形ではない。

最終候補:

```text
LogicalLightBuffer
    one entry per Directional / Point / Spot light
    color / direction / attenuation / shadow metadata index

ShadowEntryBuffer
    one entry per CSM cascade / point face / spot map
    view / projection / atlas tile / bias
```

Lighting Loopは`LogicalLightBuffer`だけを走査し、Shadow評価時だけ`ShadowEntryBuffer`を参照する。

この分離は以前からの設計方針である、Lighting DataとShadow Dataの分離にも一致する。

---

## 7. 実装状況

- [x] GPU Pass単位Timestamp
- [x] Player Lighting / Shadow / Post Effect分離
- [x] 初期Baseline取得
- [x] No Shadow参考測定
- [x] No Environment参考測定
- [x] 旧Packed Entry Lights 1参考測定
- [x] Environmentが主要因でないことを確認
- [x] Shadow参照が主要因であることを確認
- [x] Packed Entry混在走査を特定
- [x] `PackedLightEntryTraversal`追加
- [x] Logical Light単位Shader走査
- [x] Shadow Atlas Offset早期continue修正
- [x] 診断上限をLogical Light単位へ変更
- [x] Packed / Logical Light Telemetry
- [x] Packed Light Traversal Smoke
- [x] PBR Shader Compile Workflowへ接続
- [ ] Windows Build確認
- [ ] Lighting Diagnostic Contract確認
- [ ] New Baseline測定
- [ ] New No Shadow測定
- [ ] New Logical Lights 1 / 2測定
- [ ] PCF比較
- [ ] 恒久Shadow最適化
- [ ] LogicalLightBuffer / ShadowEntryBuffer分離
- [ ] Post Effect内訳計測

---

## 8. 完了条件

- `switch(materialID)`統合を使用していない
- CSM CascadeとPoint FaceをLogical Lightとして重複評価しない
- Logical Light制限でPacked Entryを途中切断しない
- 法線早期continue後もShadow Atlas Offsetが一致する
- Default設定で変更前と同じ見た目になる
- New BaselineでPlayer Lightingの改善量を確認できる
- PCF 1x1 / 3x3 / 5x5のAverage / P95を比較できる
- 次の恒久Shadow最適化を数値に基づいて選択する
