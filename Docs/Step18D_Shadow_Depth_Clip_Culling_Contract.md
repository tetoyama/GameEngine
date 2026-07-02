# Step 18-D Shadow Depth Clip Culling Contract

## 目的

ShadowMap Passで、GPU RasterizerのProjection別Depth Clipと、CPU Shadow Caster Cullingの保守性を両立する。

## 背景

GPU RasterizerはProjection種別で分離する。

```text
Directional / CSM
    DepthClipEnable = false

Point / Spot
    DepthClipEnable = true
```

Point / SpotではPerspective Near / Far外のTriangleをGPUでClipし、Depth端へ押し込まれる偽Shadowを防ぐ。

一方、CPU Frustum Cullingで同じNear / Far Planeをそのまま有効化すると、大きいAABBまたは境界を跨ぐCasterをGPUへ送る前に除外できる。この場合、Light Range内のReceiverへ光は届くのに、CasterだけがShadow Mapから消える。

CPU CullingはGPU Rasterizerと完全一致させず、Shadow欠落を避けるため意図的に保守的にする。

## 契約

`CullingViewKind::Shadow`では、Light Typeに関係なく左右・右・上・下の4 Planeだけを使用し、Near / Far Planeを無効化する。

```text
Player / Editor View
    -> Left / Right / Bottom / Top / Near / Far

Shadow View
    -> Left / Right / Bottom / Top
    -> Near / FarはZero Planeとして無効化
```

GPU側ではPoint / SpotだけPerspective Depth Clipを有効化する。

```text
CPU Shadow Culling
    -> Caster AABBを保守的に保持

GPU Rasterizer
    -> Projection種別に応じてTriangleを正確にClip
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
- 全Shadow ViewのNear / Far PlaneがZero Plane
- Near Planeより手前のCasterを通常ViewではCulling
- Far Planeより奥のCasterを通常ViewではCulling
- 同じCasterをShadow Viewでは維持
- 左右Plane外のCasterはShadow ViewでもCulling
- Player / Editor / Tile Index間のView ID分離

## 完了条件

- Directional / CSM GPU RasterizerでDepth Clampを維持
- Point / Spot GPU RasterizerでPerspective Depth Clipを有効化
- 全Shadow CPU CullingでNear / Far範囲外Casterを早期除外しない
- 左右上下のLight Frustum外Casterは除外できる
- Static Shadowと通常Shadow Packetが同じLight View Visibilityを使用する
- Visibility失敗時に影欠落ではなく通常描画へFallbackする
