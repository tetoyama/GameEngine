# Step 17-B: Render Packet View Pass Connection

## 完了範囲

- Forward PassのScene / Transform Entity再走査を除去
- Transparent3Dを完成済みPacket順で提出
- SortTransparent3DをWorld Position SnapshotからBack-to-Front Sort
- Overlay UIのScene / Entity再走査を除去
- Overlay UIをOrderInLayer降順でSort
- Material Key、Scene Context ID、Entity、Packet KindをPacketから使用

## MainThreadに残す処理

- Shader / Pipeline bind
- RenderTarget / Depth state設定
- Object constant設定
- IRenderableによる実Draw
- Camera依存のView Sort

## Workerへ移した処理

- Renderable Component検出
- RenderLayer / Material / Order分類
- Local / World Transform Snapshot
- Pass Mask分類
- 決定的Packet Merge

## 残作業

- Schedule Captureで`RenderSystem.Packet.Build`と`RenderSystem.Command.Submit`の分離確認
- IRenderable内部のComponentRegistry参照除去
- PacketへDraw Resource Handle / Geometry Rangeを格納

Capture確認完了後、Step 17-C Animation CPU Build / GPU Upload分離へ進む。
