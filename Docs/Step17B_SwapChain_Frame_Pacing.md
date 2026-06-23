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

## 採取したSpike

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

---

## 判断

通常時Spikeでは、GPU Frame Timeが7.8～13.3msの範囲なのにPresentだけが32～69msへ増加している。

このため通常Spikeの主要因は、個別GPU Passの実行時間ではなく次の経路と判断する。

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

## 検証項目

### Build

- [ ] Engine Debug x64
- [ ] Engine Release x64
- [ ] Script Debug x64
- [ ] Script Release x64
- [ ] RHI Smoke Test
- [ ] D3D11 Real Triangle Smoke

### Runtime

- [ ] `Frame Pacing: Waitable Object`表示
- [ ] Timeout countが通常0
- [ ] Present平均の低下
- [ ] Present 30～70ms Spikeの減少
- [ ] Frame Pacing Waitへ通常待機が移動
- [ ] Player / Editor描画回帰なし
- [ ] Resize後の描画回帰なし
- [ ] VSync OFF実機確認
- [ ] VSync ON実機確認

---

## 次の判断

Frame Pacing後もSpikeが発生した場合、記録位置で分類する。

- `Frame Pacing Wait`支配: GPU / Compositorの消費待ち
- `Present / Residual Queue Wait`支配: DXGI / DWM / Driver側の追加Block
- `GPU Frame`支配: GPU Pass単位Timestampへ進む
- `Render Schedule CPU`支配: MainThread描画SubmitとIRenderable内部参照を調査
- `[Resize]`: Resize専用warm-up経路を調査
