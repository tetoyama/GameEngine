# Step 19-A.5 Shadow Bias Contract

## 状態

**互換性修正・責務分離済み・実機調整待ち**

Shadow Biasの単位を明示し、既存Sceneの見た目を維持しながらWorld Space Biasへ段階移行する。

---

## 1. 修正理由

以前の修正では既存Sceneの`Param.w`を、固定NDC BiasからWorld Biasへそのまま再解釈していた。

```text
旧Scene Param.w = 0.005 NDC
    ↓ 値を維持したまま意味だけ変更
0.005 world units
```

この変更には互換性がなく、既存Sceneの影が変化して当然だった。

さらに、Perspective Depthの微分からWorld BiasをNDCへ近似変換する実装は、設定値の意味を分かりにくくし、Near / Far / Receiver Depthへ再び依存させていた。

この方式は撤回した。

---

## 2. Biasデータ契約

`LIGHT`へ専用のBias Payloadを追加した。

```text
LIGHT.ShadowBias.x = Depth Bias
LIGHT.ShadowBias.y = Normal Bias
LIGHT.ShadowBias.z = Bias Mode
LIGHT.ShadowBias.w = Reserved
```

`Param.w`は旧Scene互換用のLegacy NDC値としてのみ残す。

```text
Param.x = Point / Spot Range
Param.y = Spot Inner Angle
Param.z = Spot Outer Angle
Param.w = Legacy NDC Bias mirror
```

新規処理の正本は`ShadowBias`であり、`Param.w`はGPU Shader互換Adapterへ渡すミラー値となる。

---

## 3. Bias Mode

### 3.1 Legacy NDC

```text
ShadowBias Mode = Legacy NDC
ShadowBias.x     = Projected depth bias
ShadowBias.y     = 0
Param.w          = ShadowBias.x
```

Shadow比較:

```text
compareDepth = projectedDepth - NdcBias
```

用途:

- 既存Sceneの見た目維持
- 旧値との比較
- World Space移行時の基準

制約:

- World Space上の効果はProjectionとReceiver Depthに依存する
- Point / SpotのFar変更で見た目が変化し得る
- Normal Biasは使用しない

### 3.2 World Space

```text
ShadowBias Mode = World Space
ShadowBias.x     = Depth Bias in world units
ShadowBias.y     = Normal Bias in world units
Param.w          = 0
```

Receiver PositionをShadow Projection前に移動する。

```text
biasedPosition = worldPosition
               + directionToLight * depthBias
               + surfaceNormal
                 * normalBias
                 * (1 - saturate(N dot L))
```

Depth Bias:

- ReceiverをLight方向へ移動する
- 単位はWorld Unit
- Far Planeから独立する

Normal Bias:

- ReceiverをSurface Normal方向へ移動する
- Grazing Angleほど強くなる
- NormalがLightを正面に向く場合は0になる

World Space ModeではProjected Depthから追加Biasを引かない。

---

## 4. 既存Scene移行

YAMLに`ShadowBias`が存在しない場合:

```text
ShadowBias = LegacyNdc(Param.w)
```

これにより既存Sceneは旧NDC Biasの見た目を維持する。

保存後は次が明示される。

```yaml
Param: [range, inner, outer, legacyBias]
ShadowBias: [depthBias, normalBias, mode, reserved]
```

新規Light:

```text
Mode              = World Space
Depth Bias World  = 0.005
Normal Bias World = 0.02
Legacy Param.w    = 0
CastShadow        = OFF
```

既存Sceneを自動でWorld Spaceへ変換しない。Scene Scaleと必要なBias量に一意な変換式がないため、Inspectorで明示的に切り替える。

---

## 5. 同期境界

`ShadowBiasSynchronization`が次を保証する。

```text
Legacy NDC Mode
    Param.w = ShadowBias.x

World Space Mode
    Param.w = 0
```

同期箇所:

- YAML Decode
- Inspector Edit
- GPU Light Submission Boundary

スクリプトが`ShadowBias`を直接変更した場合も、GPU送信前に正規化する。

---

## 6. Shader責務

```text
ShadowDepthBias.hlsli
    Bias Mode判定
    World Receiver Offset
    Legacy NDC Adapter

MaterialLightingTraversal.hlsli
    Light方向とN dot Lを計算
    Receiver Bias適用
    Shadow評価へBias済み位置を渡す

DeferredFunc.hlsli / FowardFunc.hlsli
    Shadow Atlas Sampling
    Legacy Param.w比較深度Adapter
```

Point Shadowでは次を分離する。

```text
Face Selection Position = Bias前のWorld Position
Projection Position     = Bias後のWorld Position
```

BiasでPoint Face選択が切り替わり、Cube Face境界が不安定になることを防ぐ。

---

## 7. Inspector

```text
Shadow Receiver Bias
    Shadow Bias Mode
        Legacy NDC
        World Space
```

Legacy NDC:

```text
Depth Bias (NDC)
Normal Bias disabled
```

World Space:

```text
Depth Bias (World)
Normal Bias (World)
```

単位をラベルへ明示し、1つの`Shadow Bias`値に複数の意味を持たせない。

---

## 8. 回帰テスト

### ShadowBiasPolicySmokeTest

- Legacy Mode生成
- World Mode既定値
- NaN / Infinity補正
- 上限Clamp
- Mode切替
- Legacy Param.w同期
- World ModeでParam.wが0

### LightShadowParticipationSmokeTest

- GPU送信前のBias正規化
- Direct Script Mutation対応
- Unshadowed EntryへのBias保持

### Lighting Diagnostic Contract

- `LIGHT.ShadowBias` Payload存在
- 微分近似`depthDerivative`再導入禁止
- Receiver BiasをProjection前に適用
- Point Face選択へBias前座標を使用
- YAML未指定時Legacy移行
- GPU Submission Boundary同期
- Deferred / Forward Shader Compile

---

## 9. 実機確認

### 9.1 Legacy互換

既存Sceneを変更せず開き、修正前のLegacy NDC結果と比較する。

期待:

- `Param.w = 0.005`は0.005 NDCとして維持される
- Scene再保存・再読込で結果が変わらない
- Point / Spot / Directional / CSMで旧Bias挙動が維持される

### 9.2 World Space

各Lightで次を確認する。

```text
Depth Bias  = 0 / 0.001 / 0.005 / 0.02
Normal Bias = 0 / 0.005 / 0.02 / 0.1
```

期待:

- Depth Bias増加でAcneが減る
- 過大値ではPeter Panningが増える
- Normal BiasはGrazing SurfaceのAcneを抑える
- NormalがLightを正面に向く面ではNormal Biasの影響が小さい
- Point / SpotのRange変更だけでWorld Bias量が変化しない

### 9.3 Point Face境界

```text
XY Edge
XZ Edge
YZ Edge
XYZ Corner
```

期待:

- Bias調整で選択Faceが不安定にならない
- Face境界でShadowが瞬間的に切り替わらない

### 9.4 描画経路

- Deferred / Forward
- Static Batch / Ordinary Draw
- Editor / Player

すべてで同じBias結果になること。

---

## 10. 実装状況

- [x] Bias Payloadを`LIGHT`へ分離
- [x] Legacy NDC / World Space Mode追加
- [x] 既存YAMLをLegacy NDCへ移行
- [x] 新規LightをWorld Space既定化
- [x] World Receiver Depth Bias追加
- [x] World Receiver Normal Bias追加
- [x] 微分NDC近似撤回
- [x] `Param.w`二重適用防止
- [x] GPU送信境界で同期
- [x] Point Face選択位置とProjection位置を分離
- [x] Shader責務分割
- [x] CPU Smoke Test追加
- [x] Deferred / Forward Compile Smoke更新
- [ ] Windows Build確認
- [ ] Lighting Diagnostic Contract確認
- [ ] Existing Scene Legacy比較
- [ ] World Bias実機調整
- [ ] Point Face境界確認
- [ ] Static / Dynamic比較
