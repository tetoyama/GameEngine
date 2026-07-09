# Step 19-A.9 CSM Sample Cost Optimization

## 状態

**着手中 / CSM挙動維持を最優先**

`_scene`のLighting計測で、Post Effectを除外してもLighting内のShadow Samplingが支配的になり得ることを確認した。

本工程はCSMの見た目とFallback契約を維持したまま、CSM Sample周辺のShader Costだけを削減する。

---

## 禁止事項

次は描画挙動を変える可能性が高いため、本工程では行わない。

- 完全Lit時の後段Cascade Fallbackを条件付きにしない
- Atlas Tile解像度をCascade別にしない
- CSM Cascade数を減らさない
- Split設計を変更しない
- Fallback仕様を変更しない
- 遠CascadeだけPCF半径を下げない

理由:

近距離Cascadeでは見えず、後段Cascadeで初めて影として現れるCasterが存在する。完全Lit時の後段Fallbackはこのケースの救済に必要であり、端付近限定などの条件化は許可しない。

---

## 維持する契約

- PR #46時点のCSM Cascade選択
- 明示的な`currentShadowAtlasOffset + cascadeIndex`
- 完全Lit時の後段Cascade Fallback
- 後段Cascadeで影が出た場合のRaw Depth Load
- Occluder復元
- Previous / First Cascade再投影判定
- `finalShadow = min(finalShadow, shadow)`
- Tile Half-Texel Clamp
- Cascade数4
- Atlas全Entry同一Tile解像度

---

## CSM Bias方針

`_scene`でPeter Panningが確認されたため、CSMではWorldSpace Receiver Biasで受光点位置を直接動かさない。

理由:

- `_scene`のDirectional CSM Lightは`ShadowBias.x=0.3 / y=0.2 / WorldSpace`であり、Texel比例Bias倍率を0.02まで下げても、受光点自体が大きくライト方向・法線方向へ移動して接地影が浮く。
- CSMはCascade選択、UV、後段Fallbackの安定性が重要であり、受光点位置を動かすとCascade採用位置やRaw Depth再投影の入力も変わる。
- CSMのAcne対策は、受光点位置ではなく比較深度側の`Param.w` / Slope Bias / WorldSpace Bias変換 / CSM Texel比例Biasで行う。

`_scene`検証では受光点位置オフセットを止めることでPeter Panningが改善した。  
一方で他SceneではAcneが残る可能性があるため、WorldSpace `ShadowBias.x/y`はCSMでも無視せず、Light ProjectionのZ scaleでNDC比較深度Biasへ変換して使用する。

Point / Spot / 通常Directionalは従来通り`ApplyShadowReceiverBias`を使用する。

---

## 採用する低リスク改善

### 1. PCF 1x1 Fast Path

`KernelRadius <= 0`では二重loopを通さず、`SampleCmpLevelZero`を1回だけ実行する。

結果は既存の1x1 PCFと同一。

### 2. 3x3 / 5x5 PCFの固定展開

`KernelRadius == 1`は3x3、`KernelRadius == 2`は5x5として固定loopを`[unroll]`する。

結果のSample位置、Sample数、平均式は既存と同一。

### 3. 想定外KernelRadiusの旧式Fallback

Material側が3以上のKernelRadiusを指定した場合は、旧式の動的loopへFallbackする。

既存Material互換性を維持する。

### 4. Cascade loopの4段固定unroll

`DIRECTIONAL_CSM_CASCADE_COUNT`は4固定であるため、CSM loopを4段固定で`[unroll]`する。

`cascadeCount`を超えた場合は従来通り処理しない。

---

## 今回の実装

対象:

```text
Source/Shader/commonDefine.h
Source/Shader/Material/ShadowDepthBias.hlsli
Source/Shader/Material/MaterialFunc.hlsli
```

変更内容:

- `SampleCascadeTapPrevious`を追加
- `SampleCascadePCFPrevious`へ1x1 fast pathを追加
- 3x3 / 5x5 PCFを固定展開
- KernelRadius 3以上は旧式loopへFallback
- `ShadowFactorCascadesPrevious`のCascade loopを4段固定unroll
- CSMは`input.worldPos`をそのままCascade判定へ渡し、WorldSpace Receiver Biasによる位置移動を行わない
- WorldSpace `ShadowBias.x/y`をCSM比較深度Biasへ変換する
- `CSM_WORLD_SPACE_BIAS_MAX_NDC`で変換Biasの上限を固定する

変更しない内容:

- CSM Fallback条件
- Raw Depth再投影条件
- Cascade数
- Split
- Atlas Tile割当
- Texel比例Bias式

---

## 検証手順

同一Scene、同一Camera、同一Post Effect設定で次を120 sample以上取得する。

```text
Baseline
No CSM Shadow
No Point Shadow
No Shadow
PCF 1x1
PCF 3x3
PCF 5x5
```

確認する値:

```text
GPU Frame Avg / P95
Player Lighting Avg / P95
Player Shadow Avg / P95
Player Post Effect Avg / P95
```

期待:

- PCF 1x1でLightingの平均が改善する
- 3x3 / 5x5で見た目が変わらない
- CSM Cascade境界の影欠落が増えない
- 完全Lit後段Fallbackの挙動が維持される
- CSM Debug Colorで採用Cascadeが変化しない
- 接地影のPeter PanningがWorldSpace Receiver Bias設定に引きずられない
- 他SceneでWorldSpace Bias設定がCSM Acne抑制に反映される

---

## 次の候補

今回の低リスク改善後もCSM Sample Costが高い場合のみ検討する。

- Atlas Grid / Tile / TexelSizeをCPU側で事前計算してLighting定数へ渡す
- CSM Sample Costを`No CSM Shadow`との差分としてCapture表に明示する
- Player / Editorの全GPU PassをCapture結果に表示し、Unaccounted GPUを減らす

ただし、Fallback条件やCascade設計には触れない。
