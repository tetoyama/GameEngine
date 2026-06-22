from __future__ import annotations

import codecs
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read_text(path: Path) -> tuple[str, bool, str]:
    data = path.read_bytes()
    has_bom = data.startswith(codecs.BOM_UTF8)
    text = data.decode("utf-8-sig")
    eol = "\r\n" if "\r\n" in text else "\n"
    return text, has_bom, eol


def write_text(path: Path, text: str, has_bom: bool) -> None:
    encoding = "utf-8-sig" if has_bom else "utf-8"
    path.write_bytes(text.encode(encoding))


def replace_exact(path: str, old: str, new: str, expected: int = 1) -> None:
    file_path = ROOT / path
    text, has_bom, _ = read_text(file_path)
    count = text.count(old)
    if count != expected:
        raise RuntimeError(
            f"{path}: expected {expected} occurrence(s) of {old!r}, found {count}"
        )
    write_text(file_path, text.replace(old, new), has_bom)


def replace_block(path: str, old: str, new: str) -> None:
    file_path = ROOT / path
    text, has_bom, eol = read_text(file_path)
    old_eol = old.replace("\n", eol)
    new_eol = new.replace("\n", eol)
    count = text.count(old_eol)
    if count != 1:
        raise RuntimeError(f"{path}: expected one target block, found {count}")
    write_text(file_path, text.replace(old_eol, new_eol, 1), has_bom)


TASK_NAME_REPLACEMENTS = {
    "Source/GameApplication/Engine/Scene/System/Command/EntityCommandCommitSystem.h": [
        (
            'std::string("EntityCommandCommitSystem.") + domainName + "Attach"',
            'std::string("EntityCommandCommitSystem.") + domainName + ".Prepare"',
        ),
        (
            'std::string("EntityCommandCommitSystem.") + domainName + "Commit"',
            'std::string("EntityCommandCommitSystem.") + domainName + ".Commit"',
        ),
    ],
    "Source/GameApplication/Engine/Scene/System/Physic/PhysicSystemTasks.inl": [
        ('"PhysicSystem.PhysicsUpload"', '"PhysicSystem.Scene.Upload"'),
        ('"PhysicSystem.PhysicsBegin"', '"PhysicSystem.Simulation.Simulate"'),
        ('"PhysicSystem.PhysicsFetch"', '"PhysicSystem.Simulation.Fetch"'),
        ('"PhysicSystem.PhysicsDownload"', '"PhysicSystem.Scene.Download"'),
        ('"PhysicSystem.CollisionEventDispatch"', '"PhysicSystem.Collision.Dispatch"'),
    ],
    "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/renderSystem.cpp": [
        ('"RenderSystem.UpdateAnimationTime"', '"RenderSystem.AnimationTime.Commit"'),
        ('"RenderSystem.EditorUpdateAnimation"', '"RenderSystem.Animation.Upload"'),
        ('"RenderSystem.Draw"', '"RenderSystem.Frame.Submit"'),
    ],
    "Source/GameApplication/Engine/Scene/System/Render/Terrain/terrainSystem.h": [
        ('"TerrainSystem.Draw"', '"TerrainSystem.Mesh.Upload"'),
    ],
    "Source/GameApplication/Engine/Scene/System/Render/Terrain/waveSystem.h": [
        ('"WaveSystem.EditorUpdate"', '"WaveSystem.Vertex.Upload"'),
    ],
    "Source/GameApplication/Engine/Scene/System/Transform/transformSystem.cpp": [
        ('"TransformSystem.Draw"', '"TransformSystem.Hierarchy.Cleanup"'),
    ],
    "Source/GameApplication/Engine/Scene/System/Transform/followSystem.cpp": [
        ('"FollowSystem.EditorUpdate"', '"FollowSystem.Transform.Resolve"'),
    ],
    "Source/GameApplication/Engine/Scene/System/Render/Camera/cameraSystem.cpp": [
        ('"CameraSystem.UpdateViewMatrix"', '"CameraSystem.View.Build"'),
    ],
    "Source/GameApplication/Engine/Scene/System/Audio/audioSystem.h": [
        ('"AudioSystem.Update"', '"AudioSystem.Playback.Commit"'),
    ],
    "Source/GameApplication/Engine/Scene/System/Render/Particle/particleSystem.cpp": [
        ('"ParticleSystem.Update"', '"ParticleSystem.Simulation.Simulate"'),
    ],
    "Source/GameApplication/Engine/Scene/System/Render/Effect/effectSystem.h": [
        ('"EffectSystem.Update"', '"EffectSystem.Runtime.Simulate"'),
        ('"EffectSystem.EditorUpdate"', '"EffectSystem.Render.Commit"'),
    ],
    "Source/GameApplication/Engine/Scene/System/Script/ScriptSystem.cpp": [
        ('"ScriptSystem.FixedUpdate"', '"ScriptSystem.Fixed.Dispatch"'),
        ('"ScriptSystem.Update"', '"ScriptSystem.Frame.Dispatch"'),
        ('"ScriptSystem.EditorUpdate"', '"ScriptSystem.Editor.Dispatch"'),
        ('"ScriptSystem.Draw"', '"ScriptSystem.Render.Dispatch"'),
    ],
    "Source/GameApplication/Engine/Scene/System/Script/CustomScriptSystem.cpp": [
        ('"CustomScriptSystem.FixedUpdate"', '"CustomScriptSystem.Fixed.Dispatch"'),
        ('"CustomScriptSystem.Update"', '"CustomScriptSystem.Frame.Dispatch"'),
        ('"CustomScriptSystem.EditorUpdate"', '"CustomScriptSystem.Editor.Dispatch"'),
        ('"CustomScriptSystem.Draw"', '"CustomScriptSystem.Render.Dispatch"'),
    ],
}

for file_path, replacements in TASK_NAME_REPLACEMENTS.items():
    for old, new in replacements:
        replace_exact(file_path, old, new)

registry_path = "Source/GameApplication/Engine/Scene/Registry/systemRegistry.h"
replace_block(
    registry_path,
    """#include <memory>
#include <utility>
#include <vector>""",
    """#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>""",
)
replace_block(
    registry_path,
    """\t\tfor(size_t index = 0; index < m_systems.size(); ++index) {
\t\t\tSystemScheduleBuilder builder(
\t\t\t\tm_systems[index].get(),
\t\t\t\tm_systemRegistrationOrders[index],
\t\t\t\tm_tasks
\t\t\t);
\t\t\tm_systems[index]->RegisterTasks(builder);
\t\t}

\t\tstd::sort(""",
    """\t\tfor(size_t index = 0; index < m_systems.size(); ++index) {
\t\t\tSystemScheduleBuilder builder(
\t\t\t\tm_systems[index].get(),
\t\t\t\tm_systemRegistrationOrders[index],
\t\t\t\tm_tasks
\t\t\t);
\t\t\tm_systems[index]->RegisterTasks(builder);
\t\t}

#ifndef NDEBUG
\t\tValidateTaskNames();
#endif

\t\tstd::sort(""",
)
replace_block(
    registry_path,
    """private:
\tstatic constexpr size_t kDomainCount = 4;

\tstatic constexpr size_t DomainIndex(SystemTaskDomain domain) {
\t\treturn static_cast<size_t>(domain);
\t}

\tvoid EnsureTasksBuilt() {""",
    """private:
\tstatic constexpr size_t kDomainCount = 4;

\tstatic constexpr size_t DomainIndex(SystemTaskDomain domain) {
\t\treturn static_cast<size_t>(domain);
\t}

#ifndef NDEBUG
\tvoid ValidateTaskNames() const {
\t\tstd::unordered_set<std::string> registeredNames;
\t\tregisteredNames.reserve(m_tasks.size());

\t\tfor(const SystemTask& task : m_tasks) {
\t\t\tassert(!task.name.empty() && "SystemTask name must not be empty");

\t\t\tconst size_t firstSeparator = task.name.find('.');
\t\t\tassert(
\t\t\t\tfirstSeparator != std::string::npos &&
\t\t\t\tfirstSeparator != 0 &&
\t\t\t\tfirstSeparator + 1 < task.name.size() &&
\t\t\t\t"SystemTask name must use <SystemName>.<Operation> or <SystemName>.<Feature>.<Stage>"
\t\t\t);
\t\t\tassert(
\t\t\t\ttask.name.find("..") == std::string::npos &&
\t\t\t\t"SystemTask name must not contain empty segments"
\t\t\t);

\t\t\tconst bool inserted = registeredNames.insert(task.name).second;
\t\t\tassert(inserted && "SystemTask names must be globally unique");
\t\t}
\t}
#endif

\tvoid EnsureTasksBuilt() {""",
)

step17_path = "Docs/Step17_Task_Naming_And_Render_Task_Decomposition_Plan.md"
DOC_NAME_REPLACEMENTS = [
    ("`RenderSystem.EditorUpdateAnimation`", "`RenderSystem.Animation.Upload`"),
    ("`TerrainSystem.Draw`", "`TerrainSystem.Mesh.Upload`"),
    ("`WaveSystem.EditorUpdate`", "`WaveSystem.Vertex.Upload`"),
    ("`RenderSystem.Draw`", "`RenderSystem.Frame.Submit`"),
]
for old, new in DOC_NAME_REPLACEMENTS:
    replace_exact(step17_path, old, new, expected=1)

replace_block(
    step17_path,
    """## Step 17-A: SystemTask命名規則の統一

優先度: **最優先**

状態: **未着手**""",
    """## Step 17-A: SystemTask命名規則の統一

優先度: **最優先**

状態: **完了**""",
)
for item in [
    "全`RegisterTasks()`のTask名を一覧化",
    "DomainとTask名の不一致を修正",
    "`Draw` / `Update` / `EditorUpdate`の曖昧な名称を役割名へ変更",
    "Physics Taskを`Upload / Simulate / Fetch / Download / Dispatch / Commit`へ統一",
    "Render Taskを`Build / Upload / Submit`へ統一",
    "Schedule Profiler表示とYAML Exportで同一名称を使用",
    "Task名重複を検出するDebug Validationを追加",
    "命名規則をScheduler文書へ追加",
]:
    replace_exact(step17_path, f"- [ ] {item}", f"- [x] {item}")
replace_block(
    step17_path,
    """- [x] 命名規則をScheduler文書へ追加

### 完了条件""",
    """- [x] 命名規則をScheduler文書へ追加

完了記録:

- `Docs/Step17A_Task_Naming_Completion.md`

### 完了条件""",
)

migration_path = "Docs/ECS_Scheduler_Migration_Plan.md"
replace_block(
    migration_path,
    """## Step 17: Task命名統一とMainThread Task分割

状態: **未着手**""",
    """## Step 17: Task命名統一とMainThread Task分割

状態: **実装中**""",
)
replace_block(
    migration_path,
    """詳細:

- `Docs/Step17_Task_Naming_And_Render_Task_Decomposition_Plan.md`""",
    """詳細:

- `Docs/Step17_Task_Naming_And_Render_Task_Decomposition_Plan.md`
- `Docs/Step17A_Task_Naming_Completion.md`""",
)
replace_block(
    migration_path,
    """# 3. 現在の作業位置

1. Step 17-A SystemTask命名規則の統一
2. Step 16-FのQueue間同期
3. Step 16-FのPass Culling
4. D3D11最小実描画Triangle Test
5. 既存Player View実機回帰
6. Step 17-B Render Packet基盤
7. Step 17-C Animation CPU Build / GPU Upload分離

Task命名統一は、以降のCapture比較と依存解析の前提になるため最優先で実施する。""",
    """# 3. 現在の作業位置

1. Step 16-FのQueue間同期
2. Step 16-FのPass Culling
3. D3D11最小実描画Triangle Test
4. 既存Player View実機回帰
5. Step 17-B Render Packet基盤
6. Step 17-C Animation CPU Build / GPU Upload分離
7. Step 17-D Terrain CPU Mesh Build / GPU Upload分離

Step 17-AのTask命名統一は完了。以降のCapture、Profiler、YAML Export、依存解析では統一後のTask名を基準とする。""",
)

completion_path = ROOT / "Docs/Step17A_Task_Naming_Completion.md"
if completion_path.exists():
    raise RuntimeError(f"{completion_path} already exists")
completion_path.write_text(
    """# Step 17-A: SystemTask Naming Completion

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
""",
    encoding="utf-8",
    newline="\n",
)

expected_names = {
    "EntityCommandCommitSystem.Fixed.Prepare",
    "EntityCommandCommitSystem.Fixed.Commit",
    "EntityCommandCommitSystem.Frame.Prepare",
    "EntityCommandCommitSystem.Frame.Commit",
    "EntityCommandCommitSystem.Editor.Prepare",
    "EntityCommandCommitSystem.Editor.Commit",
    "EntityCommandCommitSystem.Render.Prepare",
    "EntityCommandCommitSystem.Render.Commit",
    "ScriptSystem.Fixed.Dispatch",
    "ScriptSystem.Frame.Dispatch",
    "ScriptSystem.Editor.Dispatch",
    "ScriptSystem.Render.Dispatch",
    "CustomScriptSystem.Fixed.Dispatch",
    "CustomScriptSystem.Frame.Dispatch",
    "CustomScriptSystem.Editor.Dispatch",
    "CustomScriptSystem.Render.Dispatch",
    "TransformSystem.Hierarchy.Cleanup",
    "FollowSystem.Transform.Resolve",
    "CameraSystem.View.Build",
    "RenderSystem.AnimationTime.Commit",
    "RenderSystem.Animation.Upload",
    "RenderSystem.Frame.Submit",
    "AudioSystem.Playback.Commit",
    "ParticleSystem.Simulation.Simulate",
    "EffectSystem.Runtime.Simulate",
    "EffectSystem.Render.Commit",
    "TerrainSystem.Mesh.Upload",
    "PhysicSystem.Scene.Upload",
    "PhysicSystem.Simulation.Simulate",
    "PhysicSystem.Simulation.Fetch",
    "PhysicSystem.Scene.Download",
    "PhysicSystem.Collision.Dispatch",
    "WaveSystem.Vertex.Upload",
}
if len(expected_names) != 33:
    raise RuntimeError("Task inventory contains duplicate expected names")

source_text = "\n".join(
    (ROOT / path).read_text(encoding="utf-8-sig")
    for path in TASK_NAME_REPLACEMENTS
)
for name in expected_names:
    if name.startswith("EntityCommandCommitSystem."):
        continue
    count = source_text.count(f'"{name}"')
    if count != 1:
        raise RuntimeError(f"Expected one registration for {name}, found {count}")

ambiguous_suffixes = (".Update", ".FixedUpdate", ".EditorUpdate", ".Draw")
registered_literal_pattern = re.compile(
    r"builder\.Add(?:Query)?Task(?:<[^;]+?>)?\(\s*\"([^\"]+)\"",
    re.DOTALL,
)
registered_names = registered_literal_pattern.findall(source_text)
ambiguous_names = [
    name for name in registered_names if name.endswith(ambiguous_suffixes)
]
if ambiguous_names:
    raise RuntimeError(f"Ambiguous task names remain: {ambiguous_names}")

print(f"Step 17-A task naming applied: {len(expected_names)} unique task names")
