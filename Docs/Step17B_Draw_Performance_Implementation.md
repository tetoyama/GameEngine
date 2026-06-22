# Step 17-B.1 Draw Performance Instrumentation Implementation

## CPU計測実装

- `TimeService`へDraw区間計測を追加
- 完了済みフレームの`DrawTimingBreakdown`を保持
- EngineのDraw経路を次のCPU区間へ分割
  - Frame Setup
  - ImGui Begin
  - Render Schedule
  - Debug Draw
  - Editor UI Build
  - ImGui Render / Platform Windows
  - Present / Queue Wait
- Performance Monitorへ現在値と60サンプル平均を追加
- Totalから計測区間合計を引いたUnaccounted時間を追加
- VSync / Tearing Supportを表示

## GPU計測実装

- `D3D11_QUERY_TIMESTAMP_DISJOINT`
- Frame Begin / End Timestamp
- 4フレーム分のQuery Ring Buffer
- `D3D11_ASYNC_GETDATA_DONOTFLUSH`による非同期回収
- Query未完了時はCPUを待機させず計測をスキップ
- Disjoint / Frequency不正時は値を無効化
- GPU Frame Timeの現在値と有効サンプル平均を表示

GPU TimestampはBackBuffer Clear前からImGui描画完了までを囲み、Present自体は含めない。

## Editor Panel計測実装

`EditorService`の各Panelを個別に計測する。

- MenuBar
- PerformanceMonitor
- Hierarchy
- Inspector
- AssetsBrowser
- DebugLogWindow
- ViewWindow
- SystemSetting

Performance Monitorには前回完了したPanel計測を渡し、現在値と60サンプル平均を表示する。

## 計測上の重要事項

Performance MonitorはEditor UI構築中に描画されるため、現在実行中のフレーム全体を表示できない。

このため、`EndDraw()`で確定した直前フレームのSnapshotを次フレームへ渡す。Editor UI、ImGui Render、Presentを含む完全なDraw時間を表示でき、Performance Monitor自身の負荷も次フレームのEditor UI Buildへ含まれる。

GPU Queryも同一フレームでは待たず、完了した過去フレームの結果だけを表示する。

## 2026-06-23 CPU実機観測

### Editorレイアウト

- Total Draw: 12.9716ms
- Render Schedule CPU: 5.6725ms
- Editor UI Build CPU: 6.7990ms
- Present / Queue Wait: 0.0876ms

Editor UI Buildが最大区間だった。

### Player大表示

- Total Draw: 10.4272ms
- Render Schedule CPU: 4.5389ms
- Editor UI Build CPU: 0.3200ms
- Present / Queue Wait: 5.3123ms

Render ScheduleはSchedule Captureの約4.6msと整合した。VSync OFFでもPresent / Queue Waitが大きいため、GPU Frame Timeとの比較が必要。

## 検証

- [x] Schedule Captureで`RenderSystem.Packet.Build`と`RenderSystem.Command.Submit`の分離を確認
- [x] Migration PlanへStep 17-B.1を追加
- [x] CPU計測コードを実装
- [x] Performance Monitor CPU内訳を実機表示
- [x] GPU Timestamp Queryを実装
- [x] Editor Panel単位計測を実装
- [x] Windows Build Debug x64（CPU内訳実装時点）
- [x] Windows Build Release x64（CPU内訳実装時点）
- [ ] GPU / Panel計測追加後のWindows Build Debug x64
- [ ] GPU / Panel計測追加後のWindows Build Release x64
- [ ] GPU Frame Time実機表示
- [ ] Editor Panel単位CPU時間実機表示
- [ ] 同一条件でVSync ON / OFF比較

CPU内訳実装時点ではWindows Build run #1230でEngine / ScriptのDebug / Release x64コンパイルに成功した。
