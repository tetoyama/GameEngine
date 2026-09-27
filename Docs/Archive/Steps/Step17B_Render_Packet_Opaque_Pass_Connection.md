# Step 17-B: Render Packet Opaque Pass Connection

## 完了範囲

- GBuffer geometry submissionをRender Packet入力へ変更
- Shadow caster submissionをRender Packet入力へ変更
- Scene Context IDから非所有SceneContextを解決
- PacketのMaterial KeyからObjectInfoを設定
- Packet Kindから既存IRenderableを解決
- Environment Map EntityをShadow対象外としてPacket Build時に分類

## 互換Pass分類

既存描画結果を変えないため、LayerだけでなくRenderable Kindも含めてPass Maskを決定する。

- Model / Billboard / Terrain: Shadow, GBuffer, Forward
- Wave: GBuffer, Forward
- Mesh / Particle / Effect: Forward
- Sprite: Forward, Overlay
- Debug LayerのModel系: GBufferとShadowを維持
- Transparent LayerのModel系: Forwardに加えてShadow casterを維持

## 未移行

- Forward透明距離Sort
- Overlay UI order sort
- Renderable内部のComponentRegistry参照
- Transform Snapshotの直接利用

次工程はForward / OverlayをPacket入力へ接続し、View依存SortをPacket Viewへ分離する。
