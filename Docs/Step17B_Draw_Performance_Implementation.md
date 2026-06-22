# Step 17-B.1 Draw Performance Instrumentation Implementation

## 実装内容

- `TimeService`へDraw区間計測を追加
- 完了済みフレームの`DrawTimingBreakdown`を保持
- EngineのDraw経路を次のCPU区間へ分割
  - Frame Setup
  - ImGui Begin
  - Render Schedule
  - Debug Draw
  - Editor UI Build
  - ImGui Render / Platform Windows
  - Present / VSync Wait
- Performance Monitorへ現在値と60サンプル平均を追加
- Totalから計測区間合計を引いたUnaccounted時間を追加
- VSync設定を表示
- GPU Frame Timeは未実装であることを明示

## 計測上の重要事項

Performance MonitorはEditor UI構築中に描画されるため、現在実行中のフレーム全体を表示できない。

このため、`EndDraw()`で確定した直前フレームのSnapshotを次フレームへ渡す。Editor UI、ImGui Render、Presentを含む完全なDraw時間を表示でき、Performance Monitor自身の負荷も次フレームのEditor UI Buildへ含まれる。

## 検証

- [x] Schedule Captureで`RenderSystem.Packet.Build`と`RenderSystem.Command.Submit`の分離を確認
- [x] Migration PlanへStep 17-B.1を追加
- [x] CPU計測コードを実装
- [ ] Windows Build Debug x64
- [ ] Windows Build Release x64
- [ ] Performance Monitor実機表示
- [ ] VSync ON / OFF比較
- [ ] D3D11 GPU Timestamp Query

実機表示とVSync比較は、同一Scene、同一Windowサイズ、同一View表示状態で行う。
