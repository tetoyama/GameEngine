# Step 14: Parallel Schedule Executor 完了記録

## 実装内容

- 依存数0のTaskをReady Queueへ投入
- Task完了時に後続Taskの未解決依存数を減算
- `AnyWorker` TaskをJobSystemへ投入
- MainThread Queue対応
- D3D11期間中のRenderThread TaskをMainThread Queueへフォールバック
- Worker例外時にGraphを最後まで排水して呼び出し元へ再送出
- 例外後の後続Task実行を停止
- `SystemTaskContext`へ実行Threadの`JobThreadContext`を公開
- Worker-local Commandを`ExclusiveWorldWrite` Task直前にFlush
- JobSystem停止中の直列フォールバック
- SystemRegistryの直列実行ループを依存実行器へ置換
- `CameraSystem.UpdateViewMatrix`を`Render/Early + AnyWorker`として登録
- `TransformSystem.Draw`を`Render/Default + AnyWorker`として登録

Legacy Systemは引き続き`MainThread / WorldExclusive`として扱い、詳細な`SystemAccess`と`ThreadAffinity`を宣言したTaskだけが並列実行対象となる。

## 現在の移行方針

検証を容易にするため、D3D11のDeviceContext、ImGui、Editor UIを直接触るTaskはMainThread/RenderThread扱いのまま残す。
先にCPU側で完結するTransform整理、Camera行列更新、Component値更新などを`AnyWorker`へ移して、ScheduleProfiler上でMainThread以外のTask実行を確認できる状態を作る。

## 検証

Windows Build run #441で次の6ジョブが成功した。

- Schedule Executor Smoke Test
- Job System Smoke Test
- Script Debug x64
- Script Release x64
- GameEngine Debug x64
- GameEngine Release x64

Smoke Testでは、並列Ready Node、依存Join、MainThread affinity、Worker-local Commandの構造変更前Flush、例外時Graph排水、後続Taskスキップ、JobSystem停止時の直列フォールバックを実行検証した。

今回追加した実Systemのworker化は、次回のコンパイルチェックとEditorのScheduleProfilerで確認する。
