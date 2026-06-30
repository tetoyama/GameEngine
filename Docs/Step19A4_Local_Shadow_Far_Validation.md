# Step 19-A.4 Local Shadow Far Validation

## 状態

**構造修正済み・実機確認待ち**

Point / Spot ShadowのFar範囲をLight Rangeと一致させ、Range境界での影消失と偽Shadowを検証する。

Shadow Biasの単位・互換性・調整方法は次へ分離した。

```text
Docs/Step19A5_Shadow_Bias_Contract.md
```

---

## 1. 確認した問題

### 1.1 SpotだけFarを1000倍していた

旧契約:

```text
Point Shadow Far = Param.x
Spot Shadow Far  = Param.x * 1000
Lighting Range   = Param.x
```

Spotでは光が届かない範囲までShadow Projectionが伸び、PointとSpotで`Param.x`の意味が一致していなかった。

### 1.2 CPU CullingがPerspective Farを無視していた

旧Shadow Cullingは、全ShadowがDepth Clampを使う前提でNear / Far Planeを無効化していた。

Point / Spotを`DepthClipEnable = TRUE`へ分離した後もCPU側だけ旧契約が残り、GPUとCPUのClip範囲が一致していなかった。

---

## 2. 現在の契約

```text
Point Lighting Range = Param.x
Point Shadow Far     = Param.x
Spot Lighting Range  = Param.x
Spot Shadow Far      = Param.x
Local Shadow Near    = 0.1
Minimum Depth Span   = 0.1
```

不正Range:

```text
NaN / Infinity / Near以下
    -> Near + Minimum Depth Span
```

共通Policy:

```text
LocalLightShadowProjection.h
```

`LocalLightShadowProjection`はNear / Farだけを担当し、Bias計算を持たない。

---

## 3. Light Type別Depth Clip

```text
Directional / CSM
    GPU DepthClipEnable = FALSE
    CPU Near/Far Plane  = OFF

Point / Spot
    GPU DepthClipEnable = TRUE
    CPU Near/Far Plane  = ON
```

Directional / CSMはLight Near Plane手前のCasterを保持するためDepth Clampを使う。

Point / SpotはPerspective Frustum外の形状を深度端へClampしない。

---

## 4. Inspector

Point / Spotでは次を表示する。

```text
Range / Shadow Far
Shadow Clip: Near / Far / Ratio
```

RangeはLighting減衰の終端とShadow Farの両方に使われる。

`Far / Near > 10000`ではPerspective Depth精度警告を表示する。

Bias設定はFar設定と混在させず、独立した`Shadow Receiver Bias`セクションへ表示する。

---

## 5. 回帰テスト

### LocalLightShadowProjectionSmokeTest

- 正常Range保持
- NaN / Infinity / 負値補正
- Near / Far Ratio

### RenderPacketViewCullingSmokeTest

- Directional / CSM ShadowではNear / Far Planeを無効化
- Point / Spot ShadowではNear / Far Planeを保持
- Perspective Near手前Casterを除外
- Perspective Far外Casterを除外

### Lighting Diagnostic Contract

- Spot `Range * 1000`の再導入を禁止
- Point / Spot共通Far Policyを確認
- Perspective Depth ClipをCulling Viewへ伝播

---

## 6. 実機確認

Point Lightを固定し、Rangeを次へ変更する。

```text
5
10
25
50
100
500
```

各RangeでCasterを次へ置く。

```text
Range - 0.1
Range
Range + 0.1
```

期待:

- Range内CasterだけがShadowを生成する
- Range外CasterがFar PlaneへClampされない
- Lighting減衰境界とShadow Farが一致する
- Range変更でFaceごとの境界位置が変わらない

同じ条件を次の6方向で確認する。

```text
+X / -X
+Y / -Y
+Z / -Z
```

---

## 7. 残る場合の次候補

1. Point Face Atlas Tileを跨ぐPCF Tap
2. Far / Near Ratio過大によるPerspective Depth精度
3. Caster BoundsがFar Planeを跨ぐ場合のCPU AABB判定
4. Static Batchと通常DrawのProjection不一致
5. Range外ReceiverでAmbientだけが残る見え方
6. TextureCube / Texture2DArrayへの移行

---

## 8. 実装状況

- [x] Point / Spot Far契約確認
- [x] Spot `Range * 1000`削除
- [x] Local Shadow Clip Policy追加
- [x] 不正Range補正
- [x] Point / Spot CPU Near / Far Culling有効化
- [x] InspectorへNear / Far / Ratio表示
- [x] Bias責務をStep 19-A.5へ分離
- [ ] Windows Build確認
- [ ] Lighting Diagnostic Contract確認
- [ ] Render Packet View Culling Smoke確認
- [ ] 6方向Far境界実機確認
- [ ] Range 5..500実機確認
- [ ] Static / Dynamic Caster比較
