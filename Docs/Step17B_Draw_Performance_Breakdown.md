# Step 17-B.1: Draw Performance Breakdown

## 背景

Schedule CaptureではRender Domain全体が約4.6msである一方、Performance MonitorのDraw表示は約12msとなっていた。

両者は計測範囲が異なる。

- Schedule Capture: 主に`SceneManager::Draw()`内のSystem Schedule
- Performance Monitor: `BeginDraw()`から`Present()`完了まで

したがって、差分を未最適化のRender Submit時間と断定せず、CPU区間、Present待機、GPU実行時間を分離してから最適化対象を決定する。

---

## 優先度

**Step 17-Cより前に実施する高優先度の計測工程**

理由:

- 誤った区間を最適化することを防ぐ
- Render Packet化の効果をRender Schedule単体で比較できる
- VSync待機とCPU負荷を区別できる
- ImGui Multi-Viewport、Editor UI、Presentの負荷をSchedule外として可視化できる

---

## CPU計測区間

Performance Monitorへ次の直前完了フレームの値を表示する。

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
7. `Present / VSync Wait`
   - SwapChain Present呼び出し
   - VSync有効時の待機を含む
8. `Unaccounted / Timer Overhead`
   - Draw全体から上記合計を引いた値
9. `Total Draw`
   - `BeginDraw()`から`EndDraw()`まで

全CPU値の単位は秒で保持し、Performance Monitorでmsへ変換する。

---

## 計測契約

- Draw内訳は現在実行中のフレームへ途中表示しない
- `EndDraw()`で確定した値を次フレームのEditor UIへ渡す
- これによりEditor UI、ImGui Render、Presentを含む完全なフレームを表示する
- Performance Monitorの描画コストは次フレームの`Editor UI Build CPU`へ含まれる
- CPU計測とGPU計測を混同しない
- `Present / VSync Wait`はGPU処理時間そのものではない

---

## 実装状況

- [x] `DrawTimingSection`を追加
- [x] `DrawTimingBreakdown`を追加
- [x] TimeServiceでDraw区間を個別計測
- [x] 完了フレームのSnapshotをEditorへ渡す
- [x] Performance Monitorへ現在値と60サンプル平均を表示
- [x] VSync設定を表示
- [x] 未計測時間を表示
- [x] Schedule CaptureでPacket Build / Command Submit分離を確認
- [ ] D3D11 Timestamp / Disjoint QueryによるGPU Frame Time計測
- [ ] VSync ON / OFFで同一Sceneを比較
- [ ] Editor View / Player View条件を揃えた比較
- [ ] 支配区間を確定

---

## 実機検証手順

### 1. 条件固定

次を同一にする。

- Scene
- Windowサイズ
- Player Viewサイズ
- Editor View表示状態
- Play / Pause状態
- Performance Monitor以外のEditor Panel配置
- Camera位置
- 描画オブジェクト数

### 2. VSync ON

最低60フレーム安定後、次の平均値を記録する。

- Total Draw
- Render Schedule CPU
- Editor UI Build CPU
- ImGui Render / Platform Windows
- Present / VSync Wait
- Unaccounted

### 3. VSync OFF

同一条件で同じ値を記録する。

### 4. 判定

#### Presentが支配的

- VSync OFFでTotal Drawが大きく低下する
- Render Schedule CPUはほぼ変わらない

判断:

- 12msの大部分は垂直同期待機
- CPU Renderer最適化値として扱わない
- GPU Frame Timeを追加して描画負荷を別途確認する

#### Render Scheduleが支配的

判断:

- `RenderSystem.Command.Submit`をSchedule Captureで細分化する
- IRenderable内部のComponentRegistry再取得を除去する
- Pipeline / Material / Mesh単位のState Sortを進める
- Draw Call、Constant Buffer更新、重複Bindを削減する

#### Editor UI Buildが支配的

判断:

- Panel単位CPU計測を追加する
- 非表示Panelの更新抑制を確認する
- Hierarchy / Inspector / Assets等の毎Frame走査を調査する

#### ImGui Renderが支配的

判断:

- Multi-Viewport ON / OFFを比較する
- Platform Window数と追加SwapChain提出を確認する
- ImGui Draw Call / Vertex数を記録する

#### Unaccountedが大きい

判断:

- 計測区間の欠落を修正する
- Driver同期、Resource Map待機、暗黙Flushの位置を調査する

---

## GPU計測の次段階

D3D11では次のQueryをFrame境界へ追加する。

- `D3D11_QUERY_TIMESTAMP_DISJOINT`
- Frame Begin Timestamp
- Frame End Timestamp

取得値:

- GPU Frame Time
- Timestamp有効性
- Query結果待機時間

注意:

- 同一フレームでQuery結果を待たない
- 2～4フレーム遅延のリングバッファで回収する
- Query待機をPerformance MonitorのDraw時間へ混入させない
- Device Lost / Disjoint時は値を無効として表示する

---

## 次の作業順

1. Performance Monitor内訳の実機確認
2. VSync ON / OFF比較
3. GPU Frame Time計測追加
4. 支配区間を基に最適化対象を決定
5. IRenderable内部のComponentRegistry参照除去
6. Step 17-C Animation CPU Build / GPU Upload分離

Step 17-Cへ機械的に進むのではなく、計測結果によってRender Submit、Editor UI、ImGui、GPUのどこを先に改善するか決定する。
