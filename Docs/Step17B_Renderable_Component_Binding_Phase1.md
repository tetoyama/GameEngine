# Step 17-B Renderable Component Binding Phase 1

## 目的

RenderPassから`IRenderable`を呼び出した後に、Renderable内部で同じEntityのComponentを`ComponentRegistry`から再取得している経路を段階的に除去する。

## Phase 1

- RenderPacketへ型付きの非所有Component Bindingを追加する
- TransformのWorld MatrixをPacket Build時にSnapshotする
- 全主要RenderPassを`IRenderable::ExecutePacket`経由へ統一する
- 未移行Renderableは従来の`Execute`へフォールバックする
- Model描画をPacket直接参照へ移行する

## 寿命契約

Component Bindingは所有権を持たない。
Render Domain中はStructural Mutationを行わず、同じRender Packet世代のBuild完了からSubmit完了までのみ参照する。

## 緊急回帰修正

### RenderPacket提出Layer

- Spriteが既定`Opaque3D`の場合は`OverlayUI`へ正規化
- Mesh / Particle / EffectがForward対応Layer以外の場合は`Transparent3D`へ正規化
- 正規化後のLayerをPacket、Pass Mask、Sort Keyの全てへ使用

これにより、Forward／Overlay専用RenderableがLayerとKindのPass Mask積によって`None`になり、描画対象から消える問題を修正した。

### Sprite

- RectTransform計算にBackBuffer固定サイズではなく`RenderPassContext::screenSize`を使用
- UV Bufferをゼロ初期化
- UV Sliceをセルサイズとして一貫して計算
- Sprite自身のInputLayout／VS／PSを明示Bind

### Particle

- InputLayout／VS／PSを明示Bind
- Additive BlendとDepth ReadOnlyを設定
- 描画後にAlpha BlendとDepth Writeへ復元
- UV Bufferをゼロ初期化

### Effect

- 既定Opaque LayerからForward提出Layerへ正規化し、Effekseer描画呼び出しへ到達させる

### Sprite Gizmo

- 描画と同じ`CalculateRectTransform`から`CalculateWorldMatrix`を使用
- Editor Viewの実表示サイズを共通Viewportとして使用
- 2D Gizmo結果へ3D親World逆行列を二重適用しない
- Gizmo操作後は従来どおり`ReverseCalculateRectTransform`で保存値へ戻す

## テスト

`RenderPacketBufferSmokeTest`へ次を追加した。

- Opaque SpriteがOverlayへ提出されること
- Opaque ParticleがForwardへ提出されること
- Opaque EffectがForwardへ提出されること

## 残作業

- 実機でSprite描画を確認
- 実機でParticle描画を確認
- 実機でEffect描画を確認
- Sprite Gizmoの表示位置とドラッグ後位置を確認
- Mesh / Sprite / Billboard / Particle / Terrain / Wave / EffectのComponent Binding移行
