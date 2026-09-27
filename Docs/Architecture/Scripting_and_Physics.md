# Scripting and Physics

Status: **Canonical current architecture — 2026-09-27**

## Script System

ScriptはEngine本体と分離したDLLとして読み込める。

`ScriptSystem` はModule APIを介してScript instanceを生成・破棄し、Engine Component typeをScript側へ登録する。

主なlifecycle:

- Start
- Update
- FixedUpdate
- EditorUpdate
- Draw
- Stop

## Hot reload

`ReloadScriptDLL()` がScript moduleの差し替え境界。

Hot reloadでは、古いmodule由来instanceやcallbackを保持したままDLLをUnloadしないことが重要。

現在はreload基盤が存在するが、Build Service・state migration・完全自動rollbackなどの高度なworkflowは将来拡張領域。

過去のStep 20計画は履歴資料としてArchive扱い。

## ScriptとECS

ScriptからのComponent操作もECSのstructural-changeルールに従う。

Schedule実行中に即時Storage mutationを行わず、必要に応じてCommand buffer経由へ寄せる。

## Physics

Physics backendはPhysX。

`PhysicSystem` はsimulation lifecycleを複数Taskへ分ける。

現在の主要段階:

1. `PhysicsUpload`
2. `PhysicsBegin`
3. `PhysicsFetch`
4. `PhysicsDownload`
5. `CollisionEventDispatch`

これにより、PhysX simulate/fetch境界とECS updateをScheduler上で明示できる。

## Collider runtime

Collider / Shapeのnative runtime resourceはPhysicSystemが管理する。

Mesh colliderなどはModel geometryから構築される。

Physics layer / collision matrixもSystem設定として管理する。

## Collision events

Simulation callbackから得たeventは、その場でGame scriptへ無制限に呼び戻さず、Engine側のevent stagingを経てdispatchする。

目的は、PhysX callback thread / simulation lifetimeとScript / ECS lifetimeを直接結合しないこと。

## Scene loading boundary

Additive Scene LoadのようにStorage登録を伴う処理は、Scheduler実行中に直接実行しない。

`SceneManager::QueueAdditiveSceneLoadFromFilePath()`
で予約し、安全なframe boundaryで処理する。

## Validation

常時CIでPhysics内部すべてを固定しない。

Simulation lifecycleやScheduler契約の変更時は、Physics専用Smoke / overlap analysisを手動で利用する。
