# Step 18-D Shadow Depth Clip Culling Contract

## 目的

ShadowMap PassのGPU RasterizerとCPU Frustum Cullingで、Depth方向のCaster判定を一致させる。

## 背景

`ShadowMapPass`は`DepthClipEnable=false`で描画する。

この状態ではGPUがLight ViewのNear / Far範囲外CasterをRasterize対象として残す。一方、標準の6 Plane FrustumをCPU Cullingへそのまま使用すると、Near / Far範囲外CasterをCPUだけが除外し、通常描画時には存在していた影が欠落する。

## 契約

`CullingViewKind::Shadow`では左右・右・上・下の4 Planeだけを使用し、Near / Far Planeを無効化する。

```text
Player / Editor View
    -> Left / Right / Bottom / Top / Near / Far

Shadow View
    -> Left / Right / Bottom / Top
    -> Near / FarはZero Planeとして無効化
```

Zero Planeは`CullingMath::IntersectsFrustum`でOutside判定を発生させない。

Camera EntityなしのViewを一般化せず、次の条件を満たす場合だけShadow Viewとして有効にする。

```text
view.kind == CullingViewKind::Shadow
view.instanceID != 0
```

## Tile Identity

Shadow Atlas TileごとのView IDは次を合成する。

- 親View種別: Player / Editor
- 親View Instance ID
- Shadow Tile Index: Directional / CSM Cascade / Spot / Point FaceのAtlas順

PlayerとEditor、同一View内の別Tileは異なるVisibility Viewを持つ。

## Correctness Fallback

- View Identityが不正な場合は`ShouldRender=true`へ戻す
- Visibility容量Overflowでは部分集合を公開しない
- Selection / GPU Upload失敗では通常Shadow Packetを維持する
- Queue成功前にReplacement Setを更新しない

## Smoke Test

`RenderPacketViewCullingSmokeTest`で次を検証する。

- Player / Editor ViewのNear / Far Planeが有効
- Shadow ViewのNear / Far PlaneがZero Plane
- Near Planeより手前のCasterを通常ViewではCulling
- Far Planeより奥のCasterを通常ViewではCulling
- 同じCasterをShadow Viewでは維持
- 左右Plane外のCasterはShadow ViewでもCulling
- Player / Editor / Tile Index間のView ID分離

## 完了条件

- `DepthClipEnable=false`時にNear / Far範囲外CasterをCPUだけで除外しない
- 左右上下のLight Frustum外Casterは除外できる
- Static Shadowと通常Shadow Packetが同じLight View Visibilityを使用する
- Visibility失敗時に影欠落ではなく通常描画へFallbackする
