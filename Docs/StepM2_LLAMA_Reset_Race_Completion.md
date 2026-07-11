# M-2 LLAMA Reset Race Completion

## 状態

**コード実装完了・VSビルド確認待ち（2026-07-10）**

`Docs/ECS_Scheduler_Migration_Plan.md` §2.5 M-2の`LLAMAAgent::ResetContext`と
Worker Threadのデータ競合を解消した。

## 旧実装の問題

- Worker Thread（`WorkerMain` → `RunPromptInternal`）は`m_mutex`を保持せずに
  `m_ctx` / `m_sampler` / `m_pastTokens` / `m_history` / `m_summaryText`へ触れる
- `ResetContext`は`m_mutex`だけを握って呼出Thread（BRAIN）から直接
  `ResetContextUnlocked()`を実行するため、生成中のResetは同一llama状態への
  無同期並行アクセス＝データ競合だった
- BRAIN側は`GetState() == Running`のbusy-waitで回避を試みていたが、
  Idle確認とReset実行の間にWorkerがキュー済みジョブを開始しうるTOCTOUが残る
- `WorkerMain`はジョブpop後・ロック解放後に`m_cancelRequested = false`を
  書くため、その隙間で`Stop()` / `ResetContext()`が立てたcancelを上書きして
  中断要求を取りこぼす

## 実装内容（「中断フラグで安全点まで待つ」方式）

契約: **llama状態へ触れるThreadはWorkerのみ**。外部からのResetは要求フラグ経由。

### ResetContext（呼出Thread側）

1. `m_mutex`保持で未処理ジョブキューを破棄
2. `m_resetRequested = true`＋`m_cancelRequested = true`（生成ループ中断）
3. `m_cv.notify_all()`
4. `m_resetDoneCv`でWorkerの適用完了までブロック
   （キャンセルはトークン単位でチェックされるため有限時間で完了。
   Worker終了`State::Dead`時もDeadlockせず抜ける）

### WorkerMain（安全点での適用）

- `m_cv`のwait述語へ`m_resetRequested`を追加
- ジョブ取得より先にReset要求を処理:
  `ResetContextUnlocked()` → `Idle` → cancel / resetフラグクリア →
  `m_resetDoneCv.notify_all()`
- Worker終了時（Dead遷移後）にも`m_resetDoneCv.notify_all()`

### 付随修正

- `rollbackToSnapshot`: Reset起因のキャンセルではSnapshot復元
  （KVキャッシュの重い再decode）をスキップ。直後の全消去と二重作業になるため
- `m_cancelRequested = false`のクリアを`m_mutex`保持中（ジョブpop直後）へ移動。
  Stop / Resetの中断要求取りこぼしを解消
- BRAIN.cpp: 不要になった`GetState() == Running` busy-waitを除去
  （生成完了を待たず即座に中断・Resetされる挙動へ改善）

## 変更ファイル

- `Source/GameApplication/Service/LlamaService/LLAMAAgent.h`
- `Source/GameApplication/Service/LlamaService/LLAMAAgent.cpp`
- `Source/GameApplication/Engine/Editor/UI/BRAIN/BRAIN.cpp`

## 完了条件

- [x] 呼出Threadからllama状態へ直接触れない
- [x] Reset適用はWorker Threadの安全点に限定される
- [x] 生成中のResetが生成ループを中断させる
- [x] Reset起因キャンセルでSnapshot復元の無駄な再decodeを行わない
- [x] Worker終了時にReset待機がDeadlockしない
- [x] cancelフラグ上書きによる中断取りこぼしがない
- [ ] Windows Debug x64 Build
- [ ] Windows Release x64 Build
- [ ] BRAIN実機確認: 生成中Reset / Idle中Reset / Reset直後の再送信 / 連打
- [ ] Stop → Reset → RunAsyncの順序確認

## 補足

`WorkerMain`末尾のcancel有無で分岐する`if / else`は両分岐とも`Idle`格納で
実質同一のため機能変更していない（将来Stopping状態の扱いを分ける余地として温存）。
