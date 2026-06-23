# Step 17-B.2 Frame Spike Diagnostics and Editor Panel Optimization

## 目的

GPU Frame TimeとEditor Panel単位CPU時間の実機計測結果を基に、通常フレームの支配要因を削減し、起動直後・Resize直後・不定期に発生するラグスパイクを後から特定できる状態にする。

---

## 2026-06-23 実機計測

### Editorレイアウト

- GPU Frame Time: Current 15.8753ms / Avg 13.6433ms
- Render Schedule CPU: Current 9.7448ms / Avg 5.5118ms
- Editor UI Build CPU: Current 8.0226ms / Avg 6.7478ms
- Present / Queue Wait: Current 0.0799ms / Avg 0.0861ms

Editor Panel平均:

- AssetsBrowser: 3.5650ms
- Hierarchy: 1.6757ms
- DebugLogWindow: 1.0777ms
- PerformanceMonitor: 0.3351ms
- MenuBar: 0.0304ms
- ViewWindow: 0.0428ms
- Inspector: 0.0082ms
- SystemSetting: 0.0046ms

`AssetsBrowser + Hierarchy + DebugLogWindow`はEditor UI Buildの約93.6%を占める。

### Player大表示 1

- GPU Frame Time: Current 11.6864ms / Avg 10.9221ms
- Render Schedule CPU: Current 5.5764ms / Avg 4.4877ms
- Editor UI Build CPU: Current 0.4350ms / Avg 0.3696ms
- Present / Queue Wait: Current 6.0139ms / Avg 6.8665ms

### Player大表示 2

- GPU Frame Time: Current 11.5927ms / Avg 11.3128ms
- Render Schedule CPU: Current 5.3106ms / Avg 4.7298ms
- Editor UI Build CPU: Current 0.4688ms / Avg 0.3757ms
- Present / Queue Wait: Current 14.0649ms / Avg 8.3705ms

### 判断

- Player表示時のGPU実行時間は約11ms
- Render Schedule CPUは約4.5～4.7ms
- Present待機はGPU Frame Timeと同時に増加しており、CPUがGPU Queueの空きを待つ状態と整合する
- Editor表示時はGPU負荷に加えてEditor UI Buildが約6.7ms必要
- GPU最適化とEditor UI最適化は別系統として進める

---

## Editor Panel最適化

### AssetsBrowser

変更前:

- Assetツリーを毎フレーム`directory_iterator`で列挙
- 各フォルダが子フォルダを持つか毎フレーム再走査
- 選択ディレクトリを毎フレーム列挙・ソート
- 全ネストフォルダをDefaultOpenで初回展開

変更後:

- フォルダツリーと選択ディレクトリのメタデータをキャッシュ
- `Refresh`操作、フォルダ作成・削除・名前変更時に無効化
- 走査中の参照を破棄しないよう、キャッシュ無効化を次フレームへ遅延
- ネストフォルダをLazy Openへ変更
- Icon Texture取得を1アイテム1回へ削減
- 非表示・Collapse時は即時return

外部ツールでAssetを変更した場合は`Refresh`を使用する。将来は`ReadDirectoryChangesW`によるイベント駆動更新を検討する。

### Hierarchy

変更前:

- Root判定後、各Entityについて全Entityを再走査
- 子の有無判定で全Entityを再走査
- 検索時の子孫判定で再帰的に全Entityを再走査
- 検索文字列をEntityごとに小文字化

変更後:

- Sceneごとに1回だけ`parent -> children`隣接表を構築
- Root一覧を同時に構築
- 子の有無と再帰描画は隣接表を使用
- 検索文字列はフレーム内で1回だけ小文字化
- 検索なしの場合は子孫検索を実行しない
- 非表示・Collapse時は即時return

### DebugLogWindow

変更前:

- 各Log Levelの件数を毎フレーム全ログ走査で計算
- 全ログを毎フレームフィルタ・文字列整形・ImGui提出
- MemoryLogSinkの内部配列をロックなしで直接参照

変更後:

- MemoryLogSinkへRevisionとthread-safe Snapshotを追加
- Log追加・Clear・検索・Level Filter変更時だけキャッシュ再構築
- Level件数をSnapshot更新時に1回で集計
- フィルタ済みIndexをキャッシュ
- `ImGuiListClipper`で表示範囲だけ描画
- 非表示・Collapse時は即時return

---

## Resize対策

- 幅または高さが0のResizeを無視
- 現在サイズと同じResizeを無視
- D2D Release / SwapChain Resize / D2D Recreate全体のCPU時間を記録
- Resize SerialをPerformance Monitorへ渡す
- Resize後8フレームをSpike Diagnostics上で`[Resize]`として分類

Windowの手動ドラッグ中は既存実装どおり`WM_EXITSIZEMOVE`まで実リソースResizeを遅延する。

---

## Frame Spike Diagnostics

Performance Monitorへ直近32件のSpike Sequenceを保存する。

初期閾値:

- 20ms

判定対象:

- Update CPU
- Total Draw CPU
- GPU Frame Time

記録内容:

- Frame番号
- Peak時間
- 支配区間
- Update / Draw / GPU / Render Schedule / Editor UI / Present
- 最も重いEditor Panel
- 起動後180フレーム以内の`[Startup]`
- Resize後8フレーム以内の`[Resize]`
- Resize処理自体のCPU時間

連続して閾値を超えた場合は1件のSequenceとしてまとめ、最大値を保持する。

---

## 検証状況

- [x] GPU Frame Time実機表示
- [x] Editor Panel単位CPU時間実機表示
- [x] 通常時の支配区間を特定
- [x] AssetsBrowserキャッシュ化
- [x] Hierarchy親子探索の隣接表化
- [x] DebugLogWindowのRevision Cache / Clipper化
- [x] 同一サイズ・ゼロサイズResize除外
- [x] Frame Spike Diagnostics実装
- [ ] Windows Build Debug x64
- [ ] Windows Build Release x64
- [ ] 最適化後のEditor Panel実機再計測
- [ ] Startup / Resize Spike履歴の実機確認
- [ ] 不定期Spike発生時の履歴採取

---

## 次の判断

通常フレーム:

1. Editor Panel最適化後の差分を確認
2. GPU約11msのPass内訳をTimestampで細分化
3. Shadow / GBuffer / Lighting / PostEffect / ImGuiのGPU支配Passを特定
4. IRenderable内部ComponentRegistry参照除去とState Bind削減を継続

Spike:

1. `Frame Spike Diagnostics`で`Startup`、`Resize`、通常Spikeを分類
2. GPUが支配する場合はResource Upload・Shader初回実行・GPU Passを確認
3. Update CPUが支配する場合はAsset I/O、Scene更新、ログ、Editor処理を確認
4. Presentが支配する場合はGPU Queue滞留と直前GPU Frame Timeを比較

Spike原因を推測だけで修正せず、保存された最大フレームの内訳を基準に次の修正を決定する。
