# Step 16-G: Player View Regression Guard

## 自動化した回帰防止

- 停止・一時停止中でも出力SRVが未生成なら初回描画を必ず実行する
- Player Viewの利用可能サイズを`SceneManagerContext::PlayerScreenSize`へ同期する
- 全ての早期returnでMain Render Targetを復元する
- 出力SRVがnullの場合は`ImGui::Image`へ渡さず診断表示へ切り替える
- 非表示・折り畳み状態では不要なPlayer描画を行わない
- Fullscreen経路でもPlayer画面サイズをContextへ同期する

## 自動検証

`PlayerViewRefreshPolicySmokeTest`で次を確認する。

- Playing / Stepでは毎フレーム描画
- Stopped / Pausedでも出力未生成なら即時描画
- 出力生成後のStopped / Pausedは1秒間隔に制限

## 実機確認が必要な項目

- Editor ViewとPlayer Viewを同時表示した状態で両方が正常に更新される
- Player ViewのResize後にアスペクト比と描画領域が一致する
- Play / Pause / Stop / Step遷移後も映像が残る
- PostEffectとOverlay UIがPlayer Viewへ表示される
- Player Viewを閉じたPlaying状態でMain Windowへ描画される

実機確認が完了するまでMigration Planの`既存Player View実機回帰`は未完了のまま維持する。
