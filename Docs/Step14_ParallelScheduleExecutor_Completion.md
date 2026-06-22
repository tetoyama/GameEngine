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
- `ISystem`から旧式の`FixedUpdate` / `Update` / `EditorUpdate` / `Draw`仮想関数を削除
- `ISystem::RegisterTasks`の旧互換Task自動生成を廃止
- `CameraSystem.UpdateViewMatrix`を`Render/Early + AnyWorker`として登録
- `TransformSystem.Draw`を`Render/Default + AnyWorker`として登録
- `ParticleSystem.Update`を`Frame/Default + AnyWorker`として登録
- `FollowSystem.Update` / `FollowSystem.EditorUpdate`を`AnyWorker`として登録
- Script / Render / Effect / Terrain / Physics はDX11、ImGui、Script実行順、PhysX制約を考慮し、明示Task化した上で必要箇所のみMainThreadに固定

## 現在の移行方針

Systemの実行入口は`RegisterTasks`へ一本化する。
`Start` / `Stop`はScene再生状態の切り替え時に即時実行されるLifecycle処理として残し、毎Frame / Fixed / Editor / Render の処理はすべてScheduler Taskとして登録する。

検証を容易にするため、D3D11のDeviceContext、ImGui、Editor UI、Script本体実行を直接触るTaskはMainThread扱いのまま残す。
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

今回追加した`ISystem`旧callback削除と実Systemの明示Task化は、次回のコンパイルチェックとEditorのScheduleProfilerで確認する。
