# Step 17-B.1 Draw Performance Breakdown

## 背景

Schedule CaptureではRender Domain全体が約4.6msである一方、Performance MonitorのDraw表示は約12msとなっていた。

両者は計測範囲が異なる。

- Schedule Capture: 主に`SceneManager::Draw()`内のSystem Schedule
- Performance Monitor: `BeginDraw()`から`Present()`完了まで

したがって、差分を未最適化のRender Submit時間と断定せず、CPU区間、Present待機、GPU実行時間を分離してから最適化対象を決定する。

2026-06-30時点で、画面またはPlayer Viewを大きくするとFrame Rateが急激に低下することを確認した。描画Object数を変えず、表示Pixel数の増加だけで性能が低下するため、現在の主要仮説をCPU / Draw Call BoundからGPU Pixel / Memory Bandwidth Boundへ変更する。

この観測だけでは確定しないため、Pass単位GPU TimestampとRender Scale比較によって証明する。

詳細な実装手順:

- `Docs/Step19A_GPU_Pixel_Cost_Optimization.md`

---

## 優先度

**最優先の計測工程**

Static Batch、ECS並列化、Task分割は継続するが、性能改善を目的とする作業の優先順位は次へ変更する。

1. GPU Pass単位Timestamp
2. 固定Render Scale
3. GBuffer Format / Bytes Per Pixel台帳
4. World Position Target削除
5. Parameter Target縮小とPlayer Picking分離
6. SSAO / SSR / Bloom等の低解像度化
7. Full Screen Pass統合
8. 描画並列化の再評価

理由:

- 画面サイズ依存の低下はCPU Task分割だけでは改善しにくい
- Static BatchはDraw Callを減らすがPixel数は減らさない
- GBufferは6 Color Targetを出力しており帯域負荷が大きい
- World PositionはDepthから復元できる可能性がある
- `uint4` ParameterはPlayer Viewの用途に対して過大な可能性がある
- 誤った区間を最適化することを防ぐ

Compile Error、描画欠落、Resource Lifetime等のCorrectness修正は常に性能工程より優先する。

---

## CPU計測区間

Performance Monitorへ次の直前完了Frameの値を表示する。

1. `Frame Setup`
   - BackBuffer / Depth Clear
   - Frame開始処理
2. `ImGui Begin / UI Frame`
   - ImGui Backend NewFrame
   - DockSpace構築開始
3. `Render Schedule CPU`
   - `SceneManager::Draw()`
   - System Schedule
   - Render Packet Build / Command Submit
4. `Debug Draw CPU`
   - Debug描画準備
5. `Editor UI Build CPU`
   - 全Editor PanelのImGui Command生成
   - Performance Monitor自身を含む
6. `ImGui Render / Platform Windows`
   - Main Viewport DrawData提出
   - Multi-Viewport更新と描画
7. `Present / Queue Wait`
   - SwapChain Present呼び出し
   - VSync有効時の待機を含む
8. `Unaccounted / Timer Overhead`
   - Draw全体から上記合計を引いた値
9. `Total Draw`
   - `BeginDraw()`から`EndDraw()`まで

全CPU値の単位は秒で保持し、Performance Monitorでmsへ変換する。

---

## CPU計測契約

- Draw内訳は現在実行中のFrameへ途中表示しない
- `EndDraw()`で確定した値を次FrameのEditor UIへ渡す
- Editor UI、ImGui Render、Presentを含む完全なFrameを表示する
- Performance Monitorの描画コストは次Frameの`Editor UI Build CPU`へ含める
- CPU計測とGPU計測を混同しない
- `Present / Queue Wait`はGPU処理時間そのものではない
- Query回収待機時間をGPU時間へ含めない

---

## 実装状況

- [x] `DrawTimingSection`を追加
- [x] `DrawTimingBreakdown`を追加
- [x] TimeServiceでDraw区間を個別計測
- [x] 完了FrameのSnapshotをEditorへ渡す
- [x] Performance Monitorへ現在値と60 Sample平均を表示
- [x] VSync設定を表示
- [x] 未計測時間を表示
- [x] Schedule CaptureでPacket Build / Command Submit分離を確認
- [x] Performance Monitor CPU内訳を実機表示
- [x] Editor Panel単位CPU計測を実装
- [x] D3D11 Timestamp / Disjoint QueryによるGPU Frame Time計測を実装
- [ ] GPU Frame Timeの実機表示確認
- [ ] GPU Pass単位Timestamp Query Pool
- [ ] Player / Editor / Shadow / Post Effect別GPU時間
- [ ] Internal Resolution / Pixel Count表示
- [ ] GBuffer Format / Bytes Per Pixel表示
- [ ] Editor Panel単位CPU時間の実機表示確認
- [ ] VSync ON / OFFで同一Sceneを比較
- [ ] 720p / 1080p / 1440p比較
- [ ] Render Scale 1.00 / 0.75 / 0.50比較
- [ ] Player View / Editor View / 両方表示比較
- [ ] 支配Passを最終確定

---

## 2026-06-23 CPU実機観測

### Editorレイアウト表示

- VSync: OFF
- Total Draw: 12.9716ms
- Render Schedule CPU: 5.6725ms
- Editor UI Build CPU: 6.7990ms
- ImGui Render / Platform Windows: 0.1718ms
- Present / Queue Wait: 0.0876ms

判断:

- Editor UI Buildが約52%を占める
- Render Scheduleが約44%を占める
- Editor使用時はPanel単位の調査が必要

### Player大表示

- VSync: OFF
- Total Draw: 10.4272ms
- Render Schedule CPU: 4.5389ms
- Editor UI Build CPU: 0.3200ms
- ImGui Render / Platform Windows: 0.0569ms
- Present / Queue Wait: 5.3123ms

判断:

- Render ScheduleはSchedule Captureの約4.6msと整合する
- Present / Queue Waitが約51%を占める
- VSync OFFのため垂直同期待ちとは断定しない
- GPU負荷、Frame Queue、Desktop Compositor待ちをGPU Timestampで分離する

2条件は表示領域とEditor Panel配置が異なるため、直接的な性能比較ではなく支配区間の発見に使用する。

---

## 2026-06-30 解像度依存観測

観測:

- 画面またはPlayer Viewを拡大するとFrame Rateが低下する
- Object数やGame Logic負荷を増やさなくても低下する
- CPU側のStatic BatchやScheduler改善だけでは体感改善が小さい

現時点の仮説:

```text
GBuffer MRT Write
    + Deferred Lighting Read
    + Full Screen Post Effects
    + Player / Editor二重描画
    -> Pixel数増加に比例してGPU時間と帯域が増加
```

未確定事項:

- GBuffer、Lighting、Post Effectのどれが支配しているか
- World Position Targetの実際のFormatと帯域
- `uint4` Parameter TargetがPlayer Viewでも必要か
- SSAO / SSR / BloomがFull Resolutionか
- Editor ViewとPlayer Viewの両方が同時に拡大されているか
- Present待機とGPU実行時間の関係

この段階ではGPUが限界と断定せず、解像度別Pass Timestampで確定する。

---

## GPU Pass計測契約

D3D11ではFrame単位のDisjoint Queryに加え、Pass Begin / End Timestampを追加する。

最低限のScope:

- Shadow Map
- Player GBuffer
- Player Deferred Lighting
- Player Post Effect
- Editor GBuffer
- Editor Deferred Lighting
- Editor Post Effect
- SSAO
- SSR
- Bloom
- Debug / Overlay
- ImGui Main Viewport
- ImGui Platform Windows
- Final Composite / Upscale
- GPU Frame Total

注意:

- 同一FrameでQuery結果を待たない
- 2～4 Frame遅延のRing Bufferで回収する
- `GetData`でFlushを要求しない
- Device Lost / Resize / Disjoint時は値を無効化する
- 未実行Passを0msとして表示しない
- Scope名は固定IDとし毎FrameAllocationしない
- Pass合計とGPU Frame Totalの差を`Unaccounted GPU`として表示する

---

## Baseline検証手順

### 1. 条件固定

次を同一にする。

- Scene
- Camera位置
- Play / Pause状態
- Object数
- Light数
- Shadow設定
- Post Effect設定
- VSync OFF
- Performance Monitor以外のEditor Panel配置

60 Frame以上Warm-up後、120 Frame以上の平均とP95を記録する。

### 2. 解像度比較

```text
1280 x 720
1920 x 1080
2560 x 1440
```

各条件で次を記録する。

- GPU Frame Time
- Pass別GPU Time
- CPU Render Schedule
- Present / Queue Wait
- Pixel Count
- Draw Call
- VRAMまたはRender Target推定容量

### 3. 描画構成比較

- Player Viewのみ
- Editor Viewのみ
- Player + Editor
- GBuffer + Lightingのみ
- Post Effect全停止
- Shadow停止
- SSAOのみ停止
- SSRのみ停止
- Bloomのみ停止

### 4. Render Scale比較

```text
1.00
0.75
0.50
```

Window Sizeは固定し、Internal Render Sizeだけを変更する。

### 5. 判定

#### Pixel / Bandwidth Bound

- Pixel数増加に近い比率でGPU時間が増える
- Render Scale低下でGBuffer / Lighting / Post時間が大きく下がる
- Draw Call削減ではGPU時間が大きく変わらない

次の工程:

- World Position Target削除
- Parameter Target縮小
- Player Picking Target分離
- Half Resolution Effect
- Full Screen Pass統合

#### Geometry / Vertex Bound

- 解像度を下げてもGPU時間があまり下がらない
- Object数やTriangle数で時間が増える

次の工程:

- LOD
- Occlusion Culling
- Mesh / Instance構造
- Vertex Shader / Skinning

#### Shadow Bound

- Shadow停止でGPU時間が大きく下がる

次の工程:

- Shadow Resolution
- Cascade数
- PCF Kernel
- Light別更新頻度
- Static Shadow Cache

#### Editor二重描画

- PlayerまたはEditor片方だけで大きく時間が下がる

次の工程:

- 非表示ViewのRender停止
- View更新頻度
- Editor Preview Resolution
- View別Render Scale

---

## GBuffer調査対象

現行Shader出力:

```text
Albedo
World Normal
World Position
Material
Emissive
Scene ID / Object ID / Shader ID / Flags (`uint4`)
```

最適化前に実際のRender Target生成Formatを台帳化する。

| Target | Format | Bytes / Pixel | Player用途 | Editor用途 | Consumer |
|---|---:|---:|---|---|---|
| Albedo | 調査 | 調査 | 必須 | 必須 | Lighting / Post |
| Normal | 調査 | 調査 | 必須 | 必須 | Lighting / SSAO / SSR |
| Position | 調査 | 調査 | Depth復元候補 | Depth復元候補 | Lighting / SSR / Post |
| Material | 調査 | 調査 | 必須 | 必須 | Lighting |
| Emissive | 調査 | 調査 | 条件付き | 条件付き | Lighting / Bloom |
| Param | 調査 | 調査 | 縮小候補 | Picking必須 | Material選択 / Picking |
| Depth | 調査 | 調査 | 必須 | 必須 | Depth / Position復元 |

推定帯域:

```text
MiB / Frame = width * height * bytesPerPixel / 1024 / 1024
MiB / Second = MiB / Frame * FPS
```

Read回数とMSAA Sample Countも別途加算する。

---

## 次の作業順

1. GPU Frame Timeの実機表示確認
2. Pass単位GPU Timestamp Query Pool
3. Performance MonitorへPass時間、解像度、Pixel Countを表示
4. 720p / 1080p / 1440p Baseline
5. Player / Editor / 両方表示比較
6. 固定Render Scale 1.00 / 0.75 / 0.50
7. GBuffer Format / Bytes Per Pixel台帳
8. World PositionをDepth Reconstructionへ移行
9. Player / Editor Parameter Target分離
10. SSAO / SSR / BloomのHalf Resolution化
11. Full Screen Pass統合
12. RenderDoc Before / After比較
13. 描画並列化の再検討

Static BatchのCorrectness、Compile Gate、描画欠落修正は並行して継続するが、性能改善の主工程はGPU Pixel Cost側へ移す。

## 完了条件

- GPU Frame TimeをPass単位で説明できる
- 画面サイズ増加時に支配するPassを特定できる
- Window SizeとInternal Render Sizeが分離される
- GBuffer TargetごとのFormatとBytes Per Pixelを確認できる
- World Position Target削除のBefore / Afterを測定できる
- Player Viewで不要なPicking Targetを書かない
- Post EffectのFull / Half Resolution差を比較できる
- 720p / 1080p / 1440pでGPU時間を記録できる
- 変更前より画面サイズ増加時のFrame Rate低下が緩和される
