# M-5 Constant Buffer Upload Completion

## 状態

**コード実装完了・VSビルド確認待ち（2026-07-11）**

`Docs/ECS_Scheduler_Migration_Plan.md` §2.5 M-5「定数バッファ毎セッター全体Upload」を
2 Phaseで解消した。

## 旧実装の問題

- `GraphicsContext`の個別Setter（World / View / Projection / UV / Material /
  Light / CameraPosition / Parameter / ObjectInfo）が呼出のたびに
  `UpdateSubresource`でCB全体をUpload
- 1 Drawあたり複数回の全体Uploadが発生し、Draw Cost（Pixel Costとは別軸）を押し上げ
- バッファはDEFAULT Usageで、DYNAMIC + Mapによるドライバ最適化経路を使えない

## Phase 1: グループ化Setter（コミット済み実装）

`Service/Graphics/D3D11ConstantBufferUpload.h`を新設。

- CPUミラー全体を1回でUploadする契約（部分書き込みは意図的に拒否）
- `UpdateSubresource` / `DynamicMap`の2戦略を同一APIで提供
- 16byte整列・サイズ上限・trivially copyableをstatic_assertで強制

グループ化Setterを追加し、呼出側を移行:

- `SetPerCameraConstants(cameraPosition, view, projection)` — カメラ定数を1回でUpload
- `SetPerObjectConstants(world, material, uv)` — オブジェクト定数を1回でUpload
- `SetStaticBatchObjectConstants(material, uv, objectInfo)` — Static Batch用
  （WorldはInstance Buffer供給のためミラー内Worldを維持）
- `StageObjectInfo` — ObjectInfoをミラーへstageし、後続のグループUploadへ相乗り

移行済み呼出側: Sprite / Mesh / Terrain / BillBoard / Particle / Model / Wave
各Renderable、ShadowMap / GBuffer / Editor / Player / OverlayUI / Forward各Pass、
Static Batch提出経路。

## Phase 2: DYNAMIC + Map(WRITE_DISCARD)切替（本工程）

- `GraphicsContext::kConstantBufferUploadStrategy`（constexpr）を新設し、
  **生成（`CreateConstantBuffers`）と全Upload経路が同一定数を共有**する
  - 現在値: `DynamicMap`
  - ロールバックは定数1行を`UpdateSubresource`へ戻すだけ
  - DYNAMICバッファへの`UpdateSubresource`はD3D11で不正のため、
    生成と戦略の分離を構造的に禁止するのが本設計の要点
- 共有CB 3本（b0 `CbPerFrame` / b1 `CbPerCamera` / b2 `CbPerObject`）の生成を
  `D3D11ConstantBufferUpload::Create`経由へ（DynamicMapでは
  `DYNAMIC + CPU_ACCESS_WRITE`）
- 個別Setter 9箇所の直接`UpdateSubresource`をポリシー経由へ置換
- Map(WRITE_DISCARD)は毎回ミラー全体を書くため、DISCARDのリネーム挙動と
  契約が一致する（バインドはD3D11ランタイムが追従。再バインド不要）

## 変更ファイル（Phase 2）

- `Source/GameApplication/Service/Graphics/graphicsContext.h`
- `Source/GameApplication/Service/Graphics/graphicsContext.cpp`

## 完了条件

- [x] 個別Setterの直接`UpdateSubresource`が0件
- [x] 生成とUploadが同一戦略定数を共有する
- [x] グループ化Setter含む全経路がポリシー経由である
- [ ] Windows Debug x64 Build
- [ ] Windows Release x64 Build
- [ ] 実機描画回帰（全Renderable種 / Shadow / GBuffer / Static Batch / Editor View）
- [ ] Performance MonitorでDraw CPU時間のPhase 1 / Phase 2比較計測
  （Step 17-B計測基盤を使用）

## 補足

- Phase 1はユーザー実装（コミット`e42b2d91`〜`3674090e`ほか）。
  本ドキュメントは両Phaseの契約を一箇所に記録するために作成した
- 計測でDynamicMapが劣る場合（ドライバ依存）、`kConstantBufferUploadStrategy`を
  `UpdateSubresource`へ戻して運用してよい。両戦略はUpload契約が同一のため
  挙動差は生じない
