# Step 17-B.2 Editor Performance Validation

## 対象

- AssetsBrowser filesystem metadata cache
- Hierarchy parent / children adjacency table
- DebugLogWindow revision cache and visible-row clipping
- redundant Resize suppression
- Frame Spike Diagnostics

## Build validation

最終コードをWindows Build run #1260で検証した。

- [x] Engine Debug x64
- [x] Engine Release x64
- [x] Script Debug x64
- [x] Script Release x64
- [x] Migration Plan Validation
- [x] RHI Smoke Test
- [x] D3D11 RHI Real Triangle Smoke

初回検証run #1257では、Windows SDKの`max`マクロと`std::max`が衝突してEngine buildが失敗した。`(std::max)(...)`形式へ修正後、run #1260ですべて成功した。

## Runtime validation

### Editor layout

2026-06-23の基準と同じScene・Panel配置を使用し、最低60フレーム安定後に次を記録する。

- Editor UI Build CPU average
- AssetsBrowser average
- Hierarchy average
- DebugLogWindow average
- GPU Frame Time average

期待する動作:

- AssetsBrowserがfilesystemを毎フレーム列挙しない
- HierarchyがEntityごとの全Entity子探索を行わない
- DebugLogWindowの負荷がログ変更時と表示行数を中心に増減する

### Asset refresh

1. Editor外部でAssetを追加する
2. Assets Browserの`Refresh`を押す
3. 追加項目が表示されることを確認する
4. Context Menuからフォルダを作成・削除する
5. Crashや不正なTree走査がないことを確認する

### Resize

1. Main Windowを手動Resizeする
2. `WM_EXITSIZEMOVE`後も描画が正常であることを確認する
3. 同じ最終サイズへのResizeを繰り返す
4. 不要なResource再生成が抑制されることを確認する
5. 記録されたResize CPU時間を確認する

### Frame spikes

1. `Frame Spike Diagnostics`の閾値を20msにする
2. Engineを起動し、一度Resizeする
3. 起動直後の記録に`[Startup]`が付くことを確認する
4. Resize関連の記録に`[Resize]`が付くことを確認する
5. 不定期Spikeが起きた際、最新記録の次を保存する
   - dominant section
   - Update / Draw / GPU / Render / Editor / Present
   - dominant Editor panel
   - Resize時はResize CPU時間

## Remaining work

- [ ] Editor Panel最適化後の実機比較
- [ ] Startup / Resize Spike履歴確認
- [ ] 不定期Spike採取
- [ ] Shadow / GBuffer / Lighting / PostEffect / ImGuiのGPU Pass単位Timestamp
