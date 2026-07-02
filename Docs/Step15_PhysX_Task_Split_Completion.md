# Step 15: PhysX Task分割 完了記録

## 状態

コード実装とCI検証は完了。

ローカル実機で次を確認後、Runtime検証も完了とする。

- Dynamic Actorへ重力が適用される
- Collision Enter / Stay / ExitがMainThreadで通知される
- Trigger Enter / ExitがMainThreadで通知される
- Stop → PlayでActorが安全に再生成される
- Player ViewとPhysX Debug描画に回帰がない

## Task構成

Fixed Domainの物理処理を次の5 Taskへ分割した。

1. `PhysicsUpload`
   - Transform / Collider設定をPhysXへ反映
   - MainThread
2. `PhysicsBegin`
   - `PxScene::simulate()`を開始
   - AnyWorker
3. `PhysicsFetch`
   - `PxScene::fetchResults(true)`を実行
   - AnyWorker
4. `PhysicsDownload`
   - Actor PoseをTransformへ反映
   - MainThread
5. `CollisionEventDispatch`
   - Fetch中に収集したCollision / Trigger通知をScriptへ配送
   - MainThread / WorldExclusive

`PhysicsSceneResource`と`PhysicsEventResource`へのAccess宣言によってTask順序を構築する。

## Collision Event

PhysX callbackからScriptを直接実行せず、`ScriptCollisionDispatchBridge`を経由してイベントキューへ保存する。

`PhysicsFetch`完了後の`CollisionEventDispatch`でScript通知を実行するため、ScriptコールバックはMainThreadに維持される。

## Native Resource境界

- Actor / Shape / Material / HeightField / TriangleMesh / ConvexMeshの解放処理をPhysics側へ集約
- `PhysicsRuntimeReleaseBridge`を追加
- ColliderのInspectorからShapeを削除する場合もBridge経由で解放
- Collider破棄時もBridge経由でEntity Runtimeを解放
- Collider内のPhysXポインタは互換用の非所有エイリアスとして明記

## 検証

Windows Build run #468で次の6ジョブが成功した。

- Schedule Executor Smoke Test
- Job System Smoke Test
- Script Debug x64
- Script Release x64
- GameEngine Debug x64
- GameEngine Release x64
