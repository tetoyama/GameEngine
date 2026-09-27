# Step 17-B: Render Packet Foundation

## 今回の実装範囲

既存RenderPassの描画結果を変更せず、Render Packet分離の基盤を実行Scheduleへ追加した。

- API非依存`RenderPacket`
- Transform Snapshot
- RenderLayerからPass Maskへの分類
- Material / Orderを含む64-bit Sort Key
- Worker-local Packet Buffer
- Worker Index順Mergeと決定的Stable Sort
- Frame Generationを持つFrame-local Packet Buffer
- `RenderSystem.Packet.Build` AnyWorker Task
- `RenderSystem.Command.Submit` MainThread Task
- Packet BufferのResource Write / Read依存

## Packet Build対象

- Model
- Mesh
- Sprite
- Billboard
- Particle
- Terrain
- Wave
- Effect

## 現在の互換経路

`RenderSystem.Command.Submit`は完成済みPacketのGenerationを確認した後、既存`Draw()`を呼ぶ。既存RenderPassはまだComponentRegistryを直接走査するため、描画結果を変えずにPacket生成コストとSchedule分離を先に観測できる。

次の小工程でGBuffer / Shadow / Forward / Overlayの順にPacket入力へ接続し、MainThread Submit中のECS再走査を除去する。

## Sort Key仕様

```text
[63:60] RenderLayer
[59:56] RenderPacketKind
[55:40] signed OrderInLayer
[39:08] Material Key
[07:00] reserved
```

Scene Context ID、Entity index / generation、Packet kind、stable sequenceをTie Breakerとして使用する。カメラ依存の透明距離SortはView Packet構築時に別段階で適用する。

## 検証

`RenderPacketBufferSmokeTest`で次を確認する。

- Worker Buffer入力順が異なってもMerge結果が一致する
- Sort Key順とTie Breaker順が安定する
- Frame GenerationとReady状態が更新される
- Packet Kind集計が一致する
- LayerからPass Maskへの分類が正しい


## Opaque Pass接続

GBufferとShadowのジオメトリ提出は、Scene / Transform Entityの再走査から完成済みRender Packet列の走査へ移行した。Light収集、RenderTarget設定、Camera設定、実GPU Drawは引き続きMainThread Pass側が担当する。

既存挙動維持のため、LayerとPacket Kindの両方からPass Maskを解決する。Environment Map EntityはShadow Maskを除外する。


## View Pass接続

ForwardとOverlayもRender Packet入力へ移行した。SortTransparent3DはPacketに保存したWorld Position Snapshotからカメラ距離を計算し、Back-to-Frontで提出する。Overlay UIはPacketのOrderInLayerを降順Sortして提出する。
