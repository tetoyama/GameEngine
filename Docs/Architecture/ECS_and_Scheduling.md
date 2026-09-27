# ECS and Scheduling

Status: **Canonical current contract — 2026-09-27**

旧 `ECS_Scheduler_Migration_Plan.md` とStep 11〜18の作業記録を、現在仕様だけに整理した文書。

## Entity / Reference

Entityは再利用を前提とし、古いEntityと再利用後Entityを区別できる世代管理を行う。

長期間保持する参照では、生ポインタではなく安全なEntity / Component referenceを優先する。

## Component Storage

ComponentごとにStorage戦略を選択できる。

現在定義されている戦略:

- `Dense`
- `SparseStable`
- `DirectPaged`
- `Archetype`

`TransformComponent` は明示的に `DirectPaged` を使用する。

Entity state tag、Culling、Renderer component、Script component、Game固有Runtime componentも同じRegistryへ登録される。

Storage戦略は「すべてを一つのECS方式へ寄せる」のではなく、Component特性に合わせて選ぶ。

## SystemTask

`ISystem` は処理を `SystemTask` として登録する。

実行Domain:

- `Fixed`
- `Frame`
- `Editor`
- `Render`

Systemそのものの登録順ではなく、Task単位でSchedulerへ渡す。

## Ordering

基本順序は以下で決定する。

1. Domain
2. Phase
3. Priority
4. Registration Order

その上で、TaskのResource / Component Read-Write宣言から依存辺を追加する。

`SystemScheduleCompiler` は依存グラフを構築し、Topological Orderを生成する。

## Parallel execution

`JobSystem` はworker thread、Fence、Scratch allocator、Command bufferを持つ。

Schedulerは競合しないTaskを並列実行できる。

並列化自体を目的にはせず、次を優先する。

- Read / Write契約が明示されていること
- 構造変更が実行中Taskから直接行われないこと
- Main thread affinityが必要な処理を分離すること
- Script / Physics / Renderの寿命境界を壊さないこと

## Structural change

Schedule実行中の即時構造変更は原則禁止。

`StructuralChangeGuard` がDebug時に禁止区間を検出し、
`EntityCommandBuffer` のPlayback区間だけ構造変更を許可する。

Editor DomainやScene Loadなど、Schedule外の明示的な構造変更は別扱い。

## Culling

Entity状態とCulling情報はComponentとして扱う。

Culling結果はRendererへ直接Entity pointerを渡すのではなく、Render packet / RenderWorld側のvisibilityへ反映する。

## Static batching

Static entityは通常Entityと同じWorldに存在するが、Renderer側でStaticBatch候補、Instance data、Visibility、GPU uploadへ分離する。

StaticBatch内部構造はRenderer実装詳細であり、ECS側のCanonical契約にはしない。

## Tests

自動CIで常時守るECS/Scheduler系契約は現在次を中心とする。

- JobSystem
- DependencyScheduleExecutor
- SystemAccess
- DirectPaged storage
- Transform rotation

詳細なStorage telemetryや移行時の内部構造検査は恒久ゲートにしない。

## Historical documents

過去のMigration PlanとStep Completionは `Docs/Archive/` へ移動済み。

現在仕様を確認するときは本書とソースコードを使用する。
