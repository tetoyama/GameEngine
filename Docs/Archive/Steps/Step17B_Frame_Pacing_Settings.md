# Step 17-B.4 Frame Pacing設定可変化

## 目的

現在のSwapChain Frame Pacingは`MaximumFrameLatency = 1`で固定されている。

この設定はCPUの先行フレーム数を最小化し、入力遅延とSwapChain Queue滞留を抑える一方、CPUとGPUの並列性を強く制限する。そのため、Frame Pacing導入後に観測された最大FPS低下の原因候補となる。

最大FPS、入力遅延、フレーム安定性のトレードオフを実機で比較できるように、Maximum Frame LatencyをEngineConfigとProject Settings UIから変更可能にする。

---

## 現状

- `IDXGISwapChain2::SetMaximumFrameLatency(1)`で固定
- Waitable Object非対応時も`IDXGIDevice1::SetMaximumFrameLatency(1)`で固定
- Performance MonitorにはFrame Pacing方式とTimeout回数のみ表示
- Maximum Frame Latencyの現在値は表示されない

現在の`1`固定は低遅延を優先する設定であり、必ずしも最大FPSに適した設定ではない。

---

## 設定契約

### EngineConfig

`EngineGraphicsConfig`へ次を追加する。

```cpp
struct EngineGraphicsConfig {
    RHI::BackendType backend = RHI::BackendType::Direct3D11;
    uint32_t maximumFrameLatency = 2;
};
```

YAMLは次の形式とする。

```yaml
Graphics:
  Backend: Direct3D11
  MaximumFrameLatency: 2
```

### 許容値

Project Settings UIでは次の3段階を公開する。

| 値 | 表示 | 方針 |
|---:|---|---|
| 1 | Low Latency | 入力遅延を最小化し、CPU先行を最も強く制限する |
| 2 | Balanced | 入力遅延とCPU/GPU並列性のバランスを取る |
| 3 | Maximum Throughput | CPU先行を増やし、最大FPSを優先する |

既定値は`2`とする。

設定ファイルから範囲外の値を読み込んだ場合は`1～3`へClampし、Warning Logを出す。

`Auto`は一度`SetMaximumFrameLatency()`を適用した後にDriver既定値へ戻す明確なAPI契約がないため、今回のUIには含めない。

---

## Project Settings UI

`Project Settings > Application > Rendering`へ次を追加する。

```text
Rendering API           Direct3D 11
Maximum Frame Latency   Low Latency / Balanced / Maximum Throughput
```

各選択肢にはTooltipで次を表示する。

- `1 / Low Latency`: Lowest input latency. May reduce maximum FPS.
- `2 / Balanced`: Recommended balance between latency and throughput.
- `3 / Maximum Throughput`: Allows more CPU/GPU overlap. May increase latency.

Rendering APIと異なり、Maximum Frame Latencyは可能な限り実行中に即時反映する。

設定適用に失敗した場合はUI上の値を実際の適用値へ戻し、Debug LogへHRESULTを記録する。

---

## GraphicsContext契約

次のAPIを追加する。

```cpp
bool SetMaximumFrameLatency(uint32_t latency);
uint32_t GetMaximumFrameLatency() const noexcept;
```

適用処理は次の順序とする。

1. `latency`を`1～3`へClamp
2. Waitable Object経路では`IDXGISwapChain2::SetMaximumFrameLatency(latency)`を呼ぶ
3. Fallback経路では`IDXGIDevice1::SetMaximumFrameLatency(latency)`を呼ぶ
4. HRESULT成功時だけ現在値を更新する
5. 失敗時は以前の適用値を維持する

SwapChain再生成時にもEngineConfigの設定値を再適用する。

---

## Performance Monitor

次を表示する。

```text
Frame Pacing: Waitable Object
Maximum Frame Latency: 2 (Balanced)
Timeouts: 0
```

Frame Spike履歴とSchedule Captureへ設定値を記録し、異なる設定で取得した計測結果を混同しないようにする。

---

## FPS低下の検証

Frame PacingがFPS低下の原因かを断定するため、同一条件で比較する。

### 固定条件

- 同一Scene
- 同一EditorレイアウトまたはPlayer大表示
- VSync OFF
- 同一Windowサイズ
- Resize直後とStartup直後を除外
- 同一描画設定

### 比較設定

- Maximum Frame Latency 1
- Maximum Frame Latency 2
- Maximum Frame Latency 3

### 記録項目

- 平均FPS
- 1% Low相当のFrame Time
- Draw CPU
- Frame Pacing Wait
- Render Schedule CPU
- Present / Residual Queue Wait
- GPU Frame Time
- Timeout count
- 入力遅延の主観確認

GPU Frame TimeはCPU Frame Serial対応完了後の値を使用する。Serial対応前のGPU値とCPU値を直接比較して原因を断定しない。

---

## 判断基準

- `1`から`2`または`3`へ変更してFPSが明確に回復する場合、FPS低下にはCPU/GPU先行制限が寄与している
- FPSがほぼ変わらずGPU Frame Timeが支配的な場合、GPU処理がボトルネック
- Frame Pacing Waitだけが減りPresent待機が増える場合、待機位置が移動しただけで総Frame Timeは改善していない
- `3`でSpikeや入力遅延が悪化する場合、既定値は`2`を維持する

最大FPSだけでなく、入力遅延とFrame Time分散を含めて既定値を決定する。

---

## 実装手順

1. `EngineGraphicsConfig`へ`maximumFrameLatency`を追加
2. `EngineConfig.yaml`のLoad / Saveへ追加
3. 読み込み値を`1～3`へ検証・Clamp
4. `GraphicsContext::SetMaximumFrameLatency()`を追加
5. SwapChain初期化時の固定値`1`を設定値へ置換
6. Fallback経路の固定値`1`を設定値へ置換
7. Project SettingsのRenderingへComboを追加
8. UI変更時に実行中のSwapChainへ即時適用
9. Performance Monitorへ現在値を表示
10. Spike履歴と計測ログへ設定値を保存
11. Latency 1 / 2 / 3比較計測を実施
12. 実測結果から既定値`2`の妥当性を再確認

---

## 完了条件

- [ ] EngineConfigからMaximum Frame Latencyを読み書きできる
- [ ] Project Settings UIで`1～3`を選択できる
- [ ] 設定変更を実行中に安全に反映できる
- [ ] SwapChain再生成後も設定値が維持される
- [ ] Waitable Object経路とFallback経路の両方へ適用される
- [ ] HRESULT失敗時に以前の設定へRollbackされる
- [ ] Performance Monitorへ現在値を表示する
- [ ] Engine Debug / Release x64コンパイル成功
- [ ] Script Debug / Release x64コンパイル成功
- [ ] VSync OFFでLatency 1 / 2 / 3を比較する
- [ ] VSync ONで描画回帰がない
- [ ] Resize後に設定値と描画が維持される
- [ ] Timeout countが通常0である

---

## 優先順位

GPU QueryとCPU FrameのSerial対応と並行可能だが、性能比較はSerial対応後の計測値を正として扱う。

実装順は次とする。

1. Frame Serial対応
2. Maximum Frame Latency設定可変化
3. Latency 1 / 2 / 3比較
4. GPU Pass単位Timestamp
5. IRenderable内部のComponentRegistry参照除去
