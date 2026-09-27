# Step 17-B Render Packet / Frame Serial 計測契約 完了記録

## 完了日

2026-06-24

## 対象

- Render Packet直接参照化
- IRenderable内部のComponentRegistry再検索除去
- GPU Timestamp QueryとCPU Frame RecordのFrame Serial対応
- GPU計測結果の一度きり消費契約
- Performance Monitorの同一Serial結合

---

## 1. Render Packet移行

### 完了内容

- `IRenderable::Execute`を`const RenderPacket&`受け取りへ統一
- GBuffer / Shadow / Forward / OverlayのSubmit時SceneContext検索を除去
- Model / Mesh / Sprite / Billboard / Particle / Terrain / Wave / EffectをPacket Binding参照へ移行
- Packet Build時に描画対象ComponentをFrame-local非所有Bindingとして取得
- World MatrixとParent World MatrixをPacketへSnapshot化
- Renderable内での`ComponentRegistry::GetComponent`再検索を除去
- API非依存の16要素Matrix SnapshotとD3D11変換Adapterを分離
- Packet Build時のSort情報とSubmit時の描画情報を同一Snapshotへ統一

### 併せて修正した問題

- ModelのUV Slice終端計算をセルサイズ加算へ修正
- UV列数を`1 / UV_Slice_X`から求めるよう修正
- Animation用Dynamic Vertex Bufferの範囲外アクセスを防止
- Static Vertex Buffer / Index Bufferの範囲・null検証を追加

### 残る制約

Component BindingはFrame-localな非所有Pointerであり、Packet BuildからMainThread Submit完了までSchedulerのRead HazardとBarrierで寿命を保証する。

Component構造変更やEntity破棄をRender Submitと並行させない契約は継続する。

---

## 2. GPU / CPU Frame Serial計測契約

### 完了内容

- Draw開始時に単調増加するFrame Serialを発行
- `TimeService::BeginDraw(frameSerial)`でCPU Frame Recordを開始
- Update / Draw内訳 / Resize情報を同じ完了Frame Recordへ保存
- `GpuFrameTimingQuerySet`へ`submittedFrameSerial`を追加
- `BeginGpuFrameTiming(frameSerial)`で同一SerialをGPU Queryへ付与
- GPU結果を`GpuFrameTimingResult`として回収
- 回収結果をFrame Serial順へ安定Sort
- `ConsumeResolvedGpuFrameTimings()`で未消費結果だけを一度返す
- 消費後は内部結果列を`swap`で空にし、同一GPU値の重複記録を防止

### GPU結果状態

```cpp
enum class GpuFrameTimingStatus : uint8_t {
    Pending,
    Resolved,
    Invalid,
    Dropped
};
```

- `Pending`: Query結果待ち
- `Resolved`: 有効なGPU時間を取得済み
- `Invalid`: Query失敗、Disjoint、Frequency不正、Timestamp逆転
- `Dropped`: Query Ringが埋まって計測を開始できなかった

GPU結果が未回収のFrameへ直前値や0を有効値として代入しない。

---

## 3. Performance Monitor

### 完了内容

- CPU Frame履歴を実際のFrame Serialで管理
- GPU結果を同一SerialのCPU Frameへだけ結合
- GPU結果がCPU記録より先に届いた場合はDeferred Mapへ一時保持
- GPU Graphの各SlotをFrame Serialと対応付け
- GPU結果の遅延到着後にSpike履歴を再構築
- `Pending / Resolved / Invalid / Dropped`をUI表示
- `Unaccounted Draw CPU`をDominant候補へ追加
- Editor Panel結果、Resize Tag、Startup Tagを同じCPU Frame Recordへ保持

これにより、GPU時間とCPU Draw / Pacing / Render Scheduleを別フレームの値で直接比較する問題を解消した。

---

## 4. コンパイル確認

### Render Packet直接参照化

GitHub Actions run: `28063925060`

- [x] Engine Debug x64
- [x] Engine Release x64
- [x] Script Debug x64
- [x] Script Release x64

### Frame Serial計測契約

GitHub Actions run: `28064720723`

- [x] Engine Debug x64
- [x] Engine Release x64
- [x] Script Debug x64
- [x] Script Release x64

---

## 5. 実機で未確認の項目

コンパイル確認は完了したが、以下はWindows実機起動で確認する。

- [ ] Player View描画
- [ ] Editor View描画
- [ ] Model / Mesh / Sprite / Billboard / Particle / Terrain / Wave / Effect表示
- [ ] Shadow / GBuffer / Forward / Overlay表示
- [ ] Animation Dynamic Vertex Buffer表示
- [ ] GPU状態が`Pending`から`Resolved`へ遷移する
- [ ] Spike履歴のCPU / GPU Frame Serialが一致する
- [ ] Query Ring遅延時に`Dropped`が表示される
- [ ] Resize後の描画回帰がない
- [ ] VSync ON / OFFで描画回帰がない

---

## 6. 次の作業順

1. 実機描画回帰確認
2. Frame Serial対応後のSpike再計測
3. Maximum Frame Latency 1 / 2 / 3設定UI実装
4. Latency設定別のFPS / Pacing /入力遅延比較
5. GPU Pass単位Timestamp
   - Shadow
   - GBuffer
   - Lighting
   - Forward
   - PostEffect
   - ImGui / BackBuffer Composition
6. ModelRendererComponentからD3D11 Runtime資源を分離

Frame Serial対応前に取得したGPU / CPU同時表示ログは、因果関係の判断材料として再利用しない。
