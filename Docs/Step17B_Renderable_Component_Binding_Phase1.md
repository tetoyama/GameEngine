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

## 残作業

- Mesh
- Sprite
- Billboard
- Particle
- Terrain
- Wave
- Effect

各Renderableを個別に移行し、回帰確認後に互換`Execute`経路を縮小する。
