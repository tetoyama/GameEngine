# Step 17-A: SystemTask Naming Completion

## 完了内容

全`RegisterTasks()`を監査し、Schedule Capture、Profiler、YAML Export、依存グラフで責務を判別できるTask名へ統一した。

命名変更だけを行い、次の実行契約は変更していない。

- Domain
- Phase
- Priority
- RegistrationOrder
- SystemAccess
- ThreadAffinity
- 実行Callback

## 命名契約

標準形式:

```text
<SystemName>.<Operation>
<SystemName>.<Feature>.<Stage>
<SystemName>.<Feature>.<Stage>.Chunk<Index>
```

標準Stage:

- `Collect`
- `Prepare`
- `Build`
- `Simulate`
- `Fetch`
- `Resolve`
- `Commit`
- `Upload`
- `Submit`
- `Dispatch`
- `Cleanup`

## 統一後Task一覧

| System | Task |
|---|---|
| EntityCommandCommitSystem | `EntityCommandCommitSystem.Fixed.Prepare` |
| EntityCommandCommitSystem | `EntityCommandCommitSystem.Fixed.Commit` |
| EntityCommandCommitSystem | `EntityCommandCommitSystem.Frame.Prepare` |
| EntityCommandCommitSystem | `EntityCommandCommitSystem.Frame.Commit` |
| EntityCommandCommitSystem | `EntityCommandCommitSystem.Editor.Prepare` |
| EntityCommandCommitSystem | `EntityCommandCommitSystem.Editor.Commit` |
| EntityCommandCommitSystem | `EntityCommandCommitSystem.Render.Prepare` |
| EntityCommandCommitSystem | `EntityCommandCommitSystem.Render.Commit` |
| ScriptSystem | `ScriptSystem.Fixed.Dispatch` |
| ScriptSystem | `ScriptSystem.Frame.Dispatch` |
| ScriptSystem | `ScriptSystem.Editor.Dispatch` |
| ScriptSystem | `ScriptSystem.Render.Dispatch` |
| CustomScriptSystem | `CustomScriptSystem.Fixed.Dispatch` |
| CustomScriptSystem | `CustomScriptSystem.Frame.Dispatch` |
| CustomScriptSystem | `CustomScriptSystem.Editor.Dispatch` |
| CustomScriptSystem | `CustomScriptSystem.Render.Dispatch` |
| TransformSystem | `TransformSystem.Hierarchy.Cleanup` |
| FollowSystem | `FollowSystem.Transform.Resolve` |
| CameraSystem | `CameraSystem.View.Build` |
| RenderSystem | `RenderSystem.AnimationTime.Commit` |
| RenderSystem | `RenderSystem.Animation.Upload` |
| RenderSystem | `RenderSystem.Frame.Submit` |
| AudioSystem | `AudioSystem.Playback.Commit` |
| ParticleSystem | `ParticleSystem.Simulation.Simulate` |
| EffectSystem | `EffectSystem.Runtime.Simulate` |
| EffectSystem | `EffectSystem.Render.Commit` |
| TerrainSystem | `TerrainSystem.Mesh.Upload` |
| PhysicSystem | `PhysicSystem.Scene.Upload` |
| PhysicSystem | `PhysicSystem.Simulation.Simulate` |
| PhysicSystem | `PhysicSystem.Simulation.Fetch` |
| PhysicSystem | `PhysicSystem.Scene.Download` |
| PhysicSystem | `PhysicSystem.Collision.Dispatch` |
| WaveSystem | `WaveSystem.Vertex.Upload` |

## Debug Validation

`SystemRegistry::RebuildTasks()`でDebug時に次を検証する。

- 空のTask名を拒否
- `<SystemName>.<Operation>`以上の区切りを要求
- 空Segmentを拒否
- Task名のグローバル重複を拒否

ProfilerとYAML Exportは従来どおり`SystemTask::name`を直接使用するため、追加の名称変換層は設けない。

## 次工程

Step 16-FのQueue間同期へ戻る。
