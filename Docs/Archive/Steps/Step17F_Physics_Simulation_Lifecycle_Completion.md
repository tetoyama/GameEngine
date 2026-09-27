# Step 17-F Physics Begin / Fetch and Lifecycle Completion

## 状態

**コード実装完了・Windows CI / 実機Runtime確認待ち**

Step 17の最後の項目として、PhysX `simulate` / `fetchResults`の待機隠蔽を再評価し、停止・終了境界を安全化した。

## 結論

`fetchResults`を次のFixed Stepへ遅延するCross-frame Pipelineは現時点では導入しない。

理由:

- Collision / Trigger通知が1 Fixed Step遅延する
- Physics結果からTransformへの反映が1 Fixed Step遅延する
- Character ControllerやGameplay Scriptの観測順が変わる
- 現行Schedulerでも`simulate`開始後に依存しないTaskを並列実行できる
- 実測せずに遅延を導入する正当性がない

代わりにSchedule ProfilerのFixed Captureから、Fetch待機の隠蔽状況を算出する。

## Overlap分析

Project Settings > Schedule > `Physics Begin / Fetch Overlap`へ次を表示する。

- Simulate Submit時間
- Submit完了からFetch開始までのGap
- Fetch時間
- Fetch中に他Taskが実行された時間のUnion
- 未隠蔽Fetch時間
- Coverage Ratio
- 重複Task数

判断基準:

- Fetch `<= 0.10 ms`: 現行維持
- Coverage `>= 75%`: 現行維持
- Fetch `>= 0.50 ms`かつ未隠蔽 `>= 0.25 ms`: Pre-Fetch Task再配置候補

Cross-frame Fetchは上記条件を満たしても自動採用しない。LatencyとGameplay順序の実機確認を別途必要とする。

## Simulation状態契約

`PhysicsBegin`、`PhysicsFetch`、`DrainSimulation`は同じ`m_simulationMutex`で直列化する。

これにより、SchedulerのFetchとStop / FinalizeのDrainが同時に`fetchResults`を実行しない。

状態:

```text
Idle
  -> PhysicsBegin: simulate成功
InFlight
  -> PhysicsFetchまたはDrainSimulation
Idle
```

- `simulate`失敗時は即座にIdleへ戻す
- blocking `fetchResults`から戻った後は、エラー報告時もIdleへ戻す
- Stop / Finalize前にInFlightならDrainする
- Sceneが存在しない場合もin-flightフラグを解除する

## Scene lock契約

`simulate`と`fetchResults`はScene Write Callとして扱う。

```text
lockWrite
simulate / fetchResults
unlockWrite
```

旧`fetchResults`の`lockRead`経路は削除した。

## Stop契約

Stopは次の順序で処理する。

1. Simulation Callbackを無効化
2. In-flight SimulationをDrain
3. Pending Collision Eventを破棄
4. Collider Runtimeを中央解放関数で解放
5. ActorEntityInfoを解放
6. Simulation状態をIdleへ戻す

Stop時に新しい`simulate(1.0f)`は実行しない。

## Finalize契約

Finalizeは次の順序で処理する。

1. Simulation Callbackを無効化
2. In-flight SimulationをDrain
3. Pending Collision Eventを破棄
4. Collider / Actor Runtimeを解放
5. Sceneを解放
6. Dispatcherを解放
7. PhysX ExtensionsをClose
8. Physicsを解放
9. PVD Transportを取得
10. PVDを解放
11. PVD Transportを解放
12. Foundationを解放

PVDへ手動`disconnect()`は行わない。

## Collider Runtime所有権

ColliderComponentはRuntimeポインタを非所有Aliasとして保持する。

解放は`PhysicSystem::ReleaseColliderRuntime`へ統一する。

- Actor解放前にActorEntityInfoをDetach
- Actorを解放してComponent側ポインタをnull化
- Shape Aliasをnull化
- Material / HeightField / TriangleMesh / ConvexMeshを解放してnull化

Colliderの`OnBeforeRemove`、Stop、Finalizeは同じ中央経路を使用する。二回目以降はnull判定により何もしない。

## Legacy FixedUpdate

Scheduler外の互換経路も独自実装を持たない。

```text
PhysicsUpload
PhysicsBegin
PhysicsFetch
PhysicsDownload
CollisionEventDispatch
```

これによりLegacy経路でもScene Lock、in-flight状態、Collision Event Dispatchの契約が一致する。

## 回帰テスト

`.github/workflows/physics-simulation-overlap.yml`で次を検証する。

- Begin / Fetch Task名・順序・Affinity
- simulate / fetchResultsのWrite Lock
- Begin / Fetch / Drainの共通Simulation Mutex
- blocking Fetch後のin-flight解除
- Stop / Finalize前のDrain
- Stop内に追加simulate / fetchが存在しないこと
- PVD -> Transport解放順
- 手動disconnectが存在しないこと
- Legacy FixedUpdateが共通Pipelineを利用すること
- Overlap分析のInterval Unionと判断閾値

## 実機確認

- [ ] Windows Debug x64 Build
- [ ] Windows Release x64 Build
- [ ] Physics Simulation Overlap Analysis Smoke Test
- [ ] Player ViewでCollision / Trigger通知
- [ ] Dynamic ActorのTransform反映
- [ ] Stop直前にFixed Updateが走る負荷で停止
- [ ] Stop -> Playを連続実行
- [ ] Scene Reloadを連続実行
- [ ] Application終了時にHeap Errorがない
- [ ] PVD接続あり / なしの両方で終了
- [ ] Fixed CaptureをFreezeしてFetch Coverageを記録
