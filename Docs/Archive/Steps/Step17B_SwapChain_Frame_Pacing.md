# Step 17-B.3 SwapChain Frame Pacing

## 目的

Editor Panel最適化後も残る不定期Frame Spikeを、GPU描画負荷とSwapChain Queue待機へ分離し、Flip Model SwapChainの先行フレーム数を明示的に制御する。

---

## 2026-06-24 最適化後実機結果

### Editorレイアウト

- GPU Frame Time: Current 19.2636ms / Avg 10.0820ms
- Render Schedule CPU: Current 17.9596ms / Avg 6.5190ms
- Editor UI Build CPU: Current 1.6294ms / Avg 1.6551ms
- Present / Queue Wait: Current 0.1006ms / Avg 0.0877ms

Editor Panel平均:

- PerformanceMonitor: 0.7374ms
- Hierarchy: 0.3685ms
- AssetsBrowser: 0.3698ms
- DebugLogWindow: 0.0815ms
- MenuBar: 0.0368ms
- ViewWindow: 0.0412ms
- Inspector: 0.0073ms
- SystemSetting: 0.0052ms

### Editor Panel最適化効果

2026-06-23基準との比較:

| 区間 | 変更前 | 変更後 | 削減率 |
|---|---:|---:|---:|
| Editor UI Build | 6.7478ms | 1.6551ms | 75.5% |
| AssetsBrowser | 3.5650ms | 0.3698ms | 89.6% |
| Hierarchy | 1.6757ms | 0.3685ms | 78.0% |
| DebugLogWindow | 1.0777ms | 0.0815ms | 92.4% |

PerformanceMonitorはSpike履歴、複数Graph、Panel系列を表示するため0.7374msとなり、最も重いEditor Panelへ移った。通常Editor UI全体は目標どおり軽量化された。

### Player大表示

- GPU Frame Time: Current 10.8194ms / Avg 10.7800ms
- Render Schedule CPU: Current 4.5930ms / Avg 4.7518ms
- Editor UI Build CPU: Current 0.7157ms / Avg 0.7103ms
- Present / Queue Wait: Current 4.4587ms / Avg 5.7179ms

Player表示の通常GPU時間は引き続き約11msで安定している。

---

## Frame Pacing導入前に採取したSpike

### 通常時 Present支配

#### Frame 11914

- Peak: 43.091ms
- Dominant: Present / Queue Wait 32.849ms
- Update: 1.401ms
- Draw: 43.091ms
- GPU: 7.783ms
- Render: 8.075ms
- Editor: 1.606ms

#### Frame 21767

- Peak: 39.141ms
- Dominant: Present / Queue Wait 32.399ms
- GPU: 11.058ms
- Render: 5.526ms
- Editor: 0.677ms

#### Frame 15089

- Peak: 74.573ms
- Dominant: Present / Queue Wait 69.433ms
- GPU: 13.293ms
- Render: 4.404ms
- Editor: 0.519ms

### Resize時

#### Frame 11912

- Peak: 56.480ms
- Dominant: GPU Frame 56.480ms
- Draw: 29.164ms
- Render: 4.644ms
- Present: 22.630ms
- Resize CPU: 2.407ms

#### Frame 11907

- Peak: 28.399ms
- Dominant: Render Schedule CPU 20.194ms
- GPU: 0.970ms
- Editor: 3.384ms
- Present: 3.109ms
- Resize CPU: 2.407ms

### 導入前の判断

通常時Spikeでは、GPU Frame Timeが7.8～13.3msの範囲なのにPresentだけが32～69msへ増加していた。

このため通常Spikeの主要因を次の経路と推定した。

1. CPUがGPU / Desktop Compositorより先にFrameをSwapChainへ投入
2. 2-buffer Flip ModelのQueueにFrameが滞留
3. Queue上限へ達したFrameだけ`Present`が長時間Block
4. 待機が不規則なBurstとして観測される

Resize Spikeは別系統であり、SwapChain再構成後のGPU / Render Pipeline warm-upとして継続計測する。

---

## 実装

### SwapChain作成

ティアリング対応Flip Modelで次を追加する。

- `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING`
- `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT`
- `IDXGISwapChain2::SetMaximumFrameLatency(1)`
- `IDXGISwapChain2::GetFrameLatencyWaitableObject()`

Waitable Object flagがRuntime / Driverに拒否された場合は、`ALLOW_TEARING`を維持した通常SwapChainへ再作成する。

Waitable flagなしの互換経路だけ、`IDXGIDevice1::SetMaximumFrameLatency(1)`を使用する。

### Frame開始

`BeginDraw`直後、GPU Timestamp開始前にWaitable Objectを待機する。

待機は新しいCPU計測区間へ分離する。

- `Frame Pacing Wait`

これによりQueue待機は`Present`内の不規則なBlockではなく、Frame開始前の制御された待機として計測される。

### Present

表示名を次へ変更する。

- `Present / Residual Queue Wait`

Frame Pacing導入後もPresentに大きな待機が残る場合、Desktop Window Manager、Driver、OS Scheduler、Window Occlusion等の外部要因を疑う。

### Resize

`ResizeBuffers`へは推測で再構成したflagではなく、SwapChain作成に実際に成功した`m_SwapChainFlags`をそのまま渡す。

これにより次のflag整合を維持する。

- `ALLOW_TEARING`
- `FRAME_LATENCY_WAITABLE_OBJECT`

### 安全策

- Wait timeout: 100ms
- Timeout回数をPerformance Monitorへ表示
- 最初のTimeoutだけWarning Log
- ShutdownでWaitable Handleを`CloseHandle`
- 非対応環境では自動Fallback

---

## Performance Monitor追加項目

- Frame Pacing: `Waitable Object` / `DXGI Fallback`
- Timeout count
- Frame Pacing Wait Current / Average / Graph
- Spike詳細のPacing時間
- Present / Residual Queue Wait

---

## 2026-06-24 Frame Pacing導入後の追加観測

Frame Pacing導入後、従来30～70msへ跳ねていた`Present`は約0.06～0.11msまで低下し、待機は`Frame Pacing Wait`へ移動した。

### Frame 13481

- Peak: 34.703ms
- Dominant: GPU Frame 34.703ms
- Update: 1.143ms
- Draw: 23.662ms
- GPU: 34.703ms
- Frame Pacing Wait: 13.908ms
- Render Schedule: 8.271ms
- Editor UI: 1.044ms
- Present: 0.065ms

### Frame 13361

- Peak: 39.613ms
- Dominant: Frame Pacing Wait 29.676ms
- Update: 0.982ms
- Draw: 39.613ms
- GPU: 20.244ms
- Frame Pacing Wait: 29.676ms
- Render Schedule: 8.301ms
- Editor UI: 1.064ms
- Present: 0.061ms

### Frame 11142

- Peak: 35.966ms
- Dominant: GPU Frame 35.966ms
- Update: 1.092ms
- Draw: 26.487ms
- GPU: 35.966ms
- Frame Pacing Wait: 15.466ms
- Render Schedule: 9.398ms
- Editor UI: 1.119ms
- Present: 0.107ms

### Frame 9889

- Peak: 43.384ms
- Dominant: Frame Pacing Wait 33.987ms
- Update: 1.997ms
- Draw: 43.384ms
- GPU: 17.381ms
- Frame Pacing Wait: 33.987ms
- Render Schedule: 6.227ms
- Editor UI: 2.444ms
- Present: 0.098ms

### 追加観測から確定したこと

- Frame Pacing自体は機能している
- 待機位置は`Present`から`Frame Pacing Wait`へ移動した
- `Present / Residual Queue Wait`の30～70ms Spikeは解消している
- UpdateとEditor UIは主要因ではない
- 残るSpikeはGPU / DWM / Driver / OS Schedulerによる前フレーム消費遅延、またはGPU計測値とCPUフレームの対応不一致を含む

---

## 現在の計測上の制約

GPU Timestamp Queryは4フレームリングで非同期回収している。

現在の実装ではQuery Setへ計測対象のFrame Serialを保持しておらず、回収できた最後のGPU時間を単一の`GPU Frame Time`として表示している。

そのため、Spike履歴上の次の値は必ずしも同一フレームではない。

- Update CPU
- Draw CPU
- Frame Pacing Wait
- Render Schedule CPU
- GPU Frame Time

例として、Frame 9889へ表示されたGPU 17.381msは、Frame 9889より数フレーム前のGPU実行時間である可能性がある。

したがって、現状のログだけで次を断定してはならない。

- GPU 17msのフレームが直接34msのPacing待機を発生させた
- GPU 35msと同一フレームでRender Scheduleが9msだった
- GPU SpikeとCPU Spikeが必ず同時発生した

この制約を解消するまで、GPU / CPUの因果関係は仮説として扱う。

---

## 次の計測契約

### 1. GPU QueryへFrame Serialを付与

各`GpuFrameTimingQuerySet`へ次を保持する。

- submittedFrameSerial
- begin / end Timestamp
- pending state
- valid state

### 2. GPU計測結果を構造体で返す

単一の`double`ではなく、次を返す。

```cpp
struct GpuFrameTimingResult {
    uint64_t frameSerial;
    double seconds;
    bool valid;
};
```

### 3. CPU Frame履歴をリング保持

CPU側も次をFrame Serial付きで保持する。

- Update
- Draw
- Frame Pacing Wait
- Render Schedule
- Editor UI
- Present
- Resize / Startup flags

### 4. 同じFrame Serial同士だけを結合

GPU Query回収時に、同じSerialのCPU Frame RecordへGPU時間を追記する。

GPU結果が未回収のFrameは、GPU値を0や直前値で代用せず`pending`として表示する。

### 5. GPU Pass単位Timestampへ拡張

Frame対応付け完了後、次を個別計測する。

- Shadow
- GBuffer
- Lighting
- Forward
- PostEffect
- ImGui / BackBuffer Composition

同一フレーム内のGPU Pass合計とGPU Frame Timeを比較し、Driver / DWM側の未計測時間も確認する。

---

## 現時点の原因候補

優先順ではなく、現時点で残る候補を分離して記録する。

### A. 実際のGPU処理Spike

Frame 13481や11142ではGPU Frame Timeが約35msと表示されている。

候補:

- Shadow Map
- GBuffer
- Lighting
- PostEffect
- ImGui / BackBuffer Composition
- Dynamic Buffer / Texture Upload
- Driver内でのCommand実行偏り

ただしFrame Serial未対応のため、同じSpike FrameのGPU時間とは未確定。

### B. GPU / DWM / OS Schedulerの消費遅延

Frame Pacing Waitは前フレームを次へ進められる状態になるまでの待機であり、次の影響を含み得る。

- GPU Schedulerによる他Processとの切替
- Desktop Window Manager Composition
- Driver内部処理
- Window Occlusion / Monitor同期
- OS Schedulerによる一時停止

### C. GPU QueryとCPU Frameの対応不一致

現在最も優先して除外すべき計測上の不確実性。

Frame Serial対応後に、AとBを再判定する。

---

## 検証項目

### Build

- [x] Engine Debug x64
- [x] Engine Release x64
- [x] Script Debug x64
- [x] Script Release x64
- [x] RHI Smoke Test
- [x] D3D11 Real Triangle Smoke

### Runtime

- [x] `Frame Pacing: Waitable Object`相当の待機を確認
- [x] Present平均の低下
- [x] Present 30～70ms Spikeの減少
- [x] Frame Pacing Waitへ通常待機が移動
- [x] Player / Editor描画を確認
- [x] VSync OFF実機確認
- [ ] Timeout countが通常0
- [ ] Resize後の描画回帰確認
- [ ] VSync ON実機確認
- [ ] GPU QueryとCPU FrameのSerial対応
- [ ] GPU Pass単位Timestamp

---

## 次の作業順

1. GPU Query SetへFrame Serialを追加
2. CPU Frame RecordをSerial付きリングバッファ化
3. GPU / CPUを同じSerialで結合
4. Spike DiagnosticsのGPU値を`pending / resolved`表示へ変更
5. GPU Pass単位Timestampを追加
6. GPU実行SpikeとDWM / Driver待機を再判定
7. Resize専用warm-up経路を別途調査

Frame Serial対応が完了するまでは、GPU Frame Timeと同じ行に表示されたCPU値を直接の因果関係として扱わない。
