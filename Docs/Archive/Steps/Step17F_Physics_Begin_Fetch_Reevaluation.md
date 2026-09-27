# Step 17-F Physics Begin–Fetch Wait Hiding Reevaluation

## 状態

**計測・判定基盤実装完了。代表Sceneでの実機Capture待ち。**

PhysXの`simulate()`と`fetchResults(true)`を別Fixed Taskへ分けた現行構造について、追加のCross-frame Pipelineが必要かを再評価した。

結論として、計測前にFetchを次のFixed Stepへ延期する変更は行わない。
現行Schedulerは`Simulation.Fetch`がWorkerで待機している間も、依存しないMainThread / Worker Taskを実行できる。まず、その並行実行でFetch待機がどこまで覆われているかを測定する。

## 現行Fixed Task

```text
PhysicSystem.Scene.Upload          Late /  0 / MainThread
PhysicSystem.Simulation.Simulate   Late / 10 / AnyWorker
PhysicSystem.Simulation.Fetch      Late / 20 / AnyWorker
PhysicSystem.Scene.Download        Late / 30 / MainThread
PhysicSystem.Collision.Dispatch    Late / 40 / MainThread
```

`PhysicsSceneResource`のAccess Hazardにより、Physics内部は次の順序を維持する。

```text
Upload -> Simulate -> Fetch -> Download -> Dispatch
```

一方、Physics Resourceへ触れないFixed Taskは、`Simulation.Fetch`と別Laneで同時実行できる。

## Cross-frame Fetchを自動導入しない理由

Fetchを次のFixed Stepへ送ると、次の意味論が変わる。

- Dynamic ActorからTransformへの反映が1 Fixed Step遅れる
- Collision / Trigger通知が1 Fixed Step遅れる
- RaycastやGameplay処理が参照するPhysics Worldの世代が複雑になる
- Stop / Scene Unload時に未回収Simulationを必ずDrainする必要がある
- Fixed Stepが連続実行されるFrameで複数Simulation世代の管理が必要になる

Fetch待機が十分小さい、または他Taskで覆われている場合、この複雑化に見合う効果はない。

## 追加した分析

`PhysicsSimulationOverlapAnalysis.h`は最新のFixed Schedule Captureから次を算出する。

- `Simulation.Simulate` Task時間
- Simulate終了からFetch開始までのGap
- `Simulation.Fetch` Task時間
- Fetch区間と交差した他Task区間のUnion
- 他Taskで覆われたFetch時間
- 覆われなかったFetch時間
- Fetch Coverage Ratio
- Fetch中に重なったTask数

複数Taskが同じ時間帯に重なっても二重加算しない。

## 判定基準

### Fetch Negligible

```text
Fetch <= 0.10 ms
```

現行のSame-step Fetchを維持する。

### Effective Overlap

次のいずれかを満たす。

```text
Coverage >= 75%
Uncovered Fetch <= 0.10 ms
```

現行Scheduler内で待機が十分隠れているため、Same-step Fetchを維持する。

### Deeper Pipelining Candidate

次のすべてを満たす。

```text
Fetch >= 0.50 ms
Uncovered Fetch >= 0.25 ms
Effective Overlapではない
```

この場合も直ちにCross-frame化はしない。
まずPhysicsに依存しないFixed処理をSimulate後へ配置できるかを検討する。

Cross-frame化は、次の実機検証を通過できる場合だけ別工程として実施する。

- Collision通知の1 Step遅延がGameplayへ影響しない
- Transform同期遅延がCamera / Character操作へ影響しない
- Stop / Play / Scene ReloadでSimulation世代が残らない
- 複数Fixed Step実行Frameで結果順序が決定的
- Uncovered Fetchの削減がFrame全体で有意

## Editor表示

Project Settingsの`Schedule`タブへ、次のパネルを追加した。

```text
Physics Begin / Fetch Overlap
```

表示内容:

- Simulate submit
- Submit-to-fetch gap
- Fetch
- Covered by other tasks
- Coverage percentage
- Uncovered fetch
- Overlapping task count
- 現行維持または追加検討の判定

Schedule Profilerの`Freeze`が有効な場合はFrozen Fixed Captureを使用する。
代表負荷の瞬間をFreezeして比較できる。

## 回帰テスト

`PhysicsSimulationOverlapAnalysisSmokeTest.cpp`で次を検証する。

- Fixed以外のCaptureを拒否
- Physics Task不足時のUnavailable
- 微小Fetch判定
- 重複区間のUnion計算
- Effective Overlap判定
- Partial Overlap判定
- Deeper Pipelining Candidate判定
- 失敗Taskを分析対象外にすること

専用Workflow:

```text
Physics Simulation Overlap Analysis
```

## 実機計測手順

1. Project Settingsを開く
2. `Schedule`タブを開く
3. `Capture Task Timings`を有効にする
4. Physics負荷が代表値になるSceneを再生する
5. 負荷が高い瞬間に`Freeze`する
6. `Physics Begin / Fetch Overlap`を確認する
7. Schedule YAMLも同時に出力する
8. 軽負荷・通常負荷・最大負荷で比較する

最低限、次のScene条件を測定する。

- Colliderが少ない通常Scene
- Dynamic Actorが多いScene
- Contact / Triggerが大量に発生するScene
- HeightField / Mesh Colliderを含むScene

## 次の判断

代表Captureで`Deeper Pipelining Candidate`が継続して出る場合、次の順序で検討する。

1. Physics非依存Fixed TaskをSimulate後へ移動
2. Fetch TaskのWorker占有とJobSystem Worker数を比較
3. PhysX Dispatcher Thread数との過剰Subscriptionを比較
4. それでも有意な未隠蔽時間が残る場合だけCross-frame Fetch設計へ進む

現時点では、Same-step FetchがGameplay意味論と安全性の基準である。
