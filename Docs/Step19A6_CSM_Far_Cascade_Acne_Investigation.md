# Step 19-A.6 CSM Far Cascade Acne Investigation

## 状態

**原因確定・修正契約昇格済み(2026-07-09)・残余確認のみ**

実機確認により`CSM Texel Proportional Bias`有効化でFar Cascade Acneの解消を確認。
仮説A(NDC固定BiasがCascade Texel World Sizeへ追従しない)で確定した。
修正は既定ONへ昇格し、Step19A5 §5.1へ契約として記録した。
A/B用の無効化フラグは`LIGHTING_DEBUG_FLAG_DISABLE_CSM_TEXEL_BIAS`。

遠いCascadeでShadow Acneが発生する報告(2026-07-09)の原因を特定し、Step19A5のBias契約を拡張する。

### 実機確認ログ(2026-07-10)

VS 2022 Debug x64でローカルビルド成功(先行してビルド不能だった3件のエラー — `StaticBatchTelemetryUI.h`のunique_ptr→生ポインタ`.get()`漏れ、`PlayerPass`のOverlayUIPass不完全型delete — を修正済。詳細はこのセッションのビルド修正参照)。
起動・描画を実機確認:

- 起動クラッシュなし、Debug Log **Error(0) / Critical(0)**。Editor/Player Viewとも3Dシーン描画OK。
- **Lighting GPU Diagnostics 計測器が実機で正常動作**(§3.5の実装を確認): `Show CSM Cascade Coloring`でカスケード色分けが反映、`CSM Texel Bias Scale` / `CSM Bias Debug Scale`スライダー、`Disable CSM Texel Proportional Bias`、PCF 1x1/3x3/5x5・CSM Bias x2/x4等のA/Bプリセット、120 Sample Captureいずれも存在・機能。
- Logical Light Layout実データ: Logical0 = Directional CSM / Span 4(4カスケード)/ Shadow ON、Logical1 = Point / Span 6(6面)/ Shadow ON。Packed GPU entries=10(CSM4+Point6)。
- 通常視点でDirectional CSM影が描画され、**顕著なAcneは目視されず**。

未確定(要ユーザ視覚サインオフ): §6完了条件のPCF全カーネルでの遠CascadeAcne無し / 近CascadePeter Panning無し / Deferred・Forward一致。A/Bプリセットで各条件を切替えながらの目視判断が必要。

---

## 1. 現象

- 遠距離のCSM CascadeでShadow Acneが出る
- PCF 1x1でも発生する可能性あり(未確定・要再現確認)

## 2. 直近の関連変更(容疑順)

| 変更 | 内容 | CSM遠方Acneへの関与 |
|---|---|---|
| Shadow Atlas PCF統一 | 旧CSM経路の`texelSize * tile`二重掛けを廃止し、PCF footprintが実質grid倍へ拡大 | PCF 1x1で消えるなら主因 |
| `3d65e3ae` | CSM legacy biasへslope scale追加(`ResolveOrthographicShadowDepthBias`) | slope上限到達や正対面では効果なし |
| `9ee782da` 他 | CSM receiver bias / 受光位置バイアス復元系 | world固定量なら遠Cascade texelに負ける |
| `530cf2a0` | Point Face FOV 90.5→92度 | Point専用。CSM無関係(除外可) |

## 3. 現行Bias構造の制約(事実)

`ShadowFactorCascadesPrevious`(MaterialFunc.hlsli)のCSM Biasは:

```text
bias = min(ResolveProjectedShadowDepthBias(Param.w)
           * slopeScale(NdotL, max 9.0),
           MAX_NDC_BIAS 0.01)
```

- NDC固定Bias × 受光面slope scaleのみ
- **CascadeごとのTexel World Sizeに追従しない**
- 遠CascadeはXY範囲が広くtexelあたりの深度誤差が大きいため、同じNDC Biasでは相対的に不足する

## 3.5 実装済み計測器(2026-07-09)

Lighting Diagnostic UIへ次を追加した。すべてRuntime専用でSceneへ保存されない。

| 操作 | 実装 | 用途 |
|---|---|---|
| `Show CSM Cascade Coloring` | `LIGHTING_DEBUG_FLAG_SHOW_CSM_CASCADES` | 実際にSampling採用されたCascadeを色分け(0=赤 1=緑 2=青 3=黄)。Shadow模様は50%混合で視認可 |
| `CSM Bias Debug Scale` | `CbLightingDebug.LightingDebugCsmBiasScale`(旧_LightingDebugPad) | Param.w一時倍率。0=既定x1。MAX_NDC_BIAS上限は維持 |
| `Disable CSM Texel Proportional Bias` | `LIGHTING_DEBUG_FLAG_DISABLE_CSM_TEXEL_BIAS` | 修正本体のA/B無効化(修正は既定ON) |
| A/Bプリセット | `CSM Cascade View` / `No CSM Texel Bias` / `CSM Bias x2` / `x4` | ワンクリック切替 |

仮説A実装の中身(`ShadowDepthBias.hlsli::ResolveCascadeTexelProportionalBias`):

```text
texelWorldSize = (2 / |LightProjection[0][0]|) / (atlasWidth * tile)
worldBias      = texelWorldSize * (CSM_TEXEL_BIAS_CONST_TEXELS 0.25 + min(tan(受光面角), 4.0))
biasFloor      = min(worldBias * |LightProjection[2][2]|, CSM_TEXEL_BIAS_MAX_NDC 0.03)
bias           = max(既存OrthographicBias, biasFloor)
```

注意: 現行CSMは`ShadowFactorCascadesPrevious`(PR#46復元経路)で、Atlas統一PCF契約の
対象外(texelSizeにtile係数を含む)。よって仮説D(PCF footprint拡大)はCSMには
当てはまらない可能性が高い。手順1は念のため実施する。
2026-07-09追記: 統一契約のうちTile Half-Texel ClampとRaw Depth Load Clampのみ
Previous経路へ移植した(Shadow_Atlas_PCF_Contract 状態欄参照)。

## 4. 切り分け手順

順に実施し、結果で分岐する。

- [ ] 1. PCF 1x1で再現確認(UIの`PCF 1x1`プリセット)
    - 出ない → PCF footprint拡大が主因。Kernel半径比例のslope bias導入へ(§5-D)
    - 出る → 2へ
- [ ] 2. `CSM Cascade View`で発生Cascadeを特定(全域か境界付近か)
- [ ] 3. `CSM Bias x2` / `x4`プリセットで確認(Param.wの一時倍率。再保存不要)
    - 消える → Bias量不足。Cascade Texel Size比例Biasへ(§5-A)
    - 消えない → 4へ
- [ ] 4. 正対面(NdotL≈1)でも出るか確認
    - 正対面でも出る → slope以外(深度精度/サンプル位置)。5へ
- [ ] 5. Shadow Atlas解像度・Format確認(低解像度設定は1面512px。遠Cascadeのworld texelが巨大化)
- [ ] 6. Static Batch / Ordinary DrawのCaster差で変化するか
- [ ] 7. Deferred / Forwardで一致するか(不一致ならAtlas統一契約の回帰)

補助: LightingDiagnosticUIの`Disable CSM Shadow Evaluation` / `No CSM Shadow`で寄与を分離できる。

## 5. 仮説と対応方針

| # | 仮説 | 対応 |
|---|---|---|
| A | NDC固定BiasがCascade Texel World Sizeに追従しない | OrthographicバイアスへCascade Texel係数を導入(Step19A5拡張。`Param.w`の再解釈はしない) |
| B | Receiver位置バイアスがworld固定量で遠Cascade texelより小さい | Texel Size比例のNormal Offsetへ |
| C | 1面解像度不足による深度slope誤差 | Cascade分割比・Atlas割当の見直し |
| D | PCF footprint拡大との相互作用(1x1で消える場合) | Kernel半径比例のslope bias |

## 6. 完了条件

- [x] 原因の特定(仮説A: Texel Bias有効化でAcne解消。2026-07-09実機確認)
- [x] 採用した対応をStep19A5へ契約として追記(§5.1)
- [x] 既存Sceneの`Param.w`互換維持(基準Biasとして残置)
- [ ] PCF 1x1 / 3x3 / 5x5すべてで遠CascadeにAcneなし
- [ ] 近CascadeでPeter Panning悪化なし(影の付け根が浮かないこと)
    - 2026-07-09: 定数項1.0texelで接地影の消失を確認 → 0.25texelへ調整。
      `CSM Texel Bias Scale`スライダーで再ビルドなしに追調整可能
- [ ] Deferred / Forwardで結果一致
