# MiniGame Collection Validation Runbook

## Branch and base

- Work branch: `game/minigame-collection`
- Base branch: `refactor/ecs-scheduler-foundation`
- Base commit: `355b9ac450d2461bac3fffb49820d73d8b2e8f2e`
- Draft pull request: `#48`

The game branch owns all mini-game-specific code and assets. Do not copy these runtime classes into the PR #45 refactoring branch.

## Startup architecture

`GameApplication` must remain independent from the mini-game collection. It only creates the Engine context and executes `Initialize`, `Run`, and `Shutdown`.

The mini-game collection is started by opening or launching this scene through the Engine's normal scene workflow:

```text
Asset/Game/MiniGameCollection/Scene/Entry/MiniGameCollectionEntry.scene
```

Expected composition flow:

```text
Normal scene launch path
  -> MiniGameCollectionEntry.scene
       -> MiniGameCollectionEntryRuntime queues MiniGamePersistent.scene
       -> SceneManager applies the additive load outside ECS schedule execution
       -> MiniGamePersistent.scene starts
       -> Entry scene marks itself for destruction
       -> Persistent + selected mini-game scene form the runtime Multi-Scene composition
```

Do not add mini-game scene loading to `GameApplication`. Do not hard-code the entry scene into application configuration as part of the feature branch. A standalone build may select the entry scene through the existing build/start-scene configuration, while the Editor may open the entry scene normally before Play.

Do not use `MiniGamePersistent.scene` as the normal entry point. Direct persistent-scene startup bypasses the scene-driven composition contract and does not verify the Entry-to-Persistent transition.

The persistent scene owns the common camera, directional light, selection UI, countdown/result presentation, pooled audio voices, fallback one-shot effect voices, screen flash, camera shake, UI tween state, and scene transition service.

Mini-game scenes are loaded additively and contain only their rules runtime. Stage, player, CPU, tile, sheep, pen, obstacle, and presentation fallback entities are created through the queued structural API.

## Deferred additive scene loading

`Scene::Initialize` registers component storage. It must not run while an ECS schedule is executing.

All mini-game additive loads therefore use:

```text
QueueAdditiveSceneLoadFromFilePath(path)
```

The queued paths are processed by the Engine at the next safe frame boundary before `SceneManager::Update`. Runtime scripts must not call `LoadFromFilePath` directly.

This rule applies to:

- Entry scene loading Persistent
- selection loading the chosen game
- retry
- next-game transition

## Runtime UI boundary

Player-facing mini-game UI must not use ImGui.

The following UI is rendered through `MiniGameRuntimeUi`, `MainRenderer`, and Direct2D / DirectWrite:

- entry loading display
- game selection
- rule and control explanation
- timer
- score and alive-state row
- countdown and GO display
- Presentation Spike timing track and outcome
- result, retry, selection-return, and next-game guidance
- score / hit HUD burst
- screen flash overlay

ImGui is permitted only for explicit development and diagnostic surfaces:

- component `inspector()` output
- Backshot F3 hit / rear-cone debug overlay
- existing Engine editor and debug windows

Gameplay state, player input, result navigation, and normal presentation must remain usable when all debug ImGui windows are hidden. The validation workflow rejects new `ImGui::` calls in the non-debug mini-game runtime UI files.

## Selection controls

```text
W / S or Up / Down : select game
Enter or Space      : start selected game
```

The selection contains the three required games in this order:

1. Color Territory
2. Sheep Roundup
3. Backshot

## Shared result controls

```text
R               : retry the current game
B or Backspace  : unload the game scene and return to selection
N               : load the next game
```

`Escape` remains reserved for the engine's application-exit confirmation and is not used for mini-game navigation.

## Presentation Spike

Scene:

```text
Asset/Game/MiniGameCollection/Scene/PresentationTest/PresentationSpike.scene
```

Control:

```text
Space       : stop the moving marker
R           : retry after result
B/Backspace : return to selection
N           : continue to Color Territory
```

Expected sequence:

```text
3 -> 2 -> 1 -> GO
input window
SUCCESS / CLOSE / MISS
result
retry / selection / next
```

The pure presentation smoke test executes ten attempts with nine retries and then shutdown. The runtime scene must additionally be checked visually ten times to confirm that audio voices, effect voices, UI tweens, camera offsets, screen flash, and scene-token events do not remain active.

## Color Territory

Rule:

```text
床を自分の色に塗れ！
```

Control:

```text
WASD or arrow keys : move
```

Duration: 40 seconds.

Validation points:

- Walking to a new tile changes it to the player's color.
- Walking onto another player's tile transfers one point from the old owner to the new owner.
- Standing on one tile does not repeatedly score.
- Player contact produces only a small separation and short knockback.
- Easy, Normal, and Hard CPU use different decision interval, information radius, target hold, prediction, and mistake values.
- CPU targets remain stable long enough for intent to be readable.
- During the final ten seconds, CPU utility increasingly favors attacking the current leader.
- Score, leader change, final result, tie, retry, selection return, and next-game transitions are visible.

## Sheep Roundup

Rule:

```text
羊を自分の囲いへ入れろ！
```

Control:

```text
WASD or arrow keys : move
```

Duration: 50 seconds, or until all sheep have entered pens.

Validation points:

- Sheep flee from the combined influence of nearby players.
- Sheep use controlled velocity rather than rigid-body force as the outcome authority.
- Wall avoidance turns sheep away before they remain attached to a boundary.
- Direction interpolation prevents rapid 180-degree oscillation.
- Local cohesion keeps nearby sheep loosely grouped without making a rigid flock.
- CPU selects an intercept point on the side opposite its own pen, then closes in to push.
- CPU does not retarget every frame.
- Entering a pen immediately confirms the score, changes sheep presentation, and prevents rescoring.
- Score, leader change, final result, tie, retry, selection return, and next-game transitions are visible.

## Backshot

Rule:

```text
相手の背中を撃て！
```

Controls:

```text
WASD or arrow keys : move and face
Space              : shoot forward
F3                 : toggle rear-cone and shot debug overlay
```

Duration: 35 seconds, or until one combatant remains.

Validation points:

- Shooting only selects a target inside the configured forward arc and range.
- Obstacles block line of sight deterministically.
- Rear elimination uses the victim forward vector and victim-to-attacker direction dot product.
- Front and side hits do not eliminate.
- The configured rear threshold is visible in the F3 overlay.
- CPU evaluates target rear opportunity, incoming threat, wall distance, line of sight, remaining player count, and cooldown.
- CPU target hold and decision delay prevent superhuman retargeting.
- Elimination, remaining-two warning, final hit, final result, tie-by-timeout, retry, selection return, and next-game transitions are visible.

## Scene cleanup order

The persistent transition transaction is:

```text
Lock gameplay input
Wait for finishing presentation
Cancel presentation by SceneToken
Invoke rules shutdown callback
Mark the mini-game scene for unload
Wait until SceneManager removes it
Queue the requested next additive scene, or complete for selection return
Wait until the queued scene is present
```

After every transition, verify:

- No previous player, CPU, sheep, tile, pen, obstacle, timer, or runtime entity remains.
- No previous one-shot audio voice remains in `Playing` state.
- No previous fallback effect voice remains visible.
- Camera transform has returned to its registered base position.
- Screen flash alpha is zero.
- No previous result or countdown UI remains.
- The new scene receives a new SceneToken.

## Automated validation

Workflow:

```text
.github/workflows/minigame-collection-core.yml
```

Jobs and checks:

1. Verify that `GameApplication` has no MiniGameCollection dependency.
2. Verify the Entry Scene and Entry Runtime composition contract.
3. Reject immediate `LoadFromFilePath` calls from mini-game runtime code.
4. Reject `ImGui::` usage from player-facing mini-game runtime UI files while allowing the Inspector and Backshot F3 diagnostic overlay.
5. Compile and run portable C++20 model, rules, presentation, and cleanup smoke tests with GCC warnings treated as errors.
6. Validate required scene asset structure, including the Entry Scene.
7. Build the complete `GameEngine.sln` Debug x64 configuration with MSVC.

The workflow uses a branch/ref concurrency group with `cancel-in-progress: true`, so obsolete runs from intermediate commits do not block the latest validation.

## Required manual sequence

Open `MiniGameCollectionEntry.scene`, then run this complete loop at least three times:

```text
Entry loading display
Persistent selection
Color Territory
Result
Next
Sheep Roundup
Result
Next
Backshot
Result
Return to selection
```

Then run ten Presentation Spike retries and ten retries of each formal mini-game. Record any entity-count growth, retained sound, retained effect, stale UI, stale CPU movement, invalid ComponentRef, missing model, assertion, or scene-load failure in the implementation plan before changing code.

During the manual pass, hide all normal ImGui editor/debug windows and confirm that every selection, instruction, timer, score, countdown, result, and navigation prompt remains visible through the Direct2D runtime UI.

## Current verification boundary

Automated compilation, scene-entry enforcement, deferred-load enforcement, UI-boundary enforcement, and pure-rule tests can be completed in GitHub Actions. Visual composition, input feel, CPU readability, camera framing, audio balance, effect intensity, Direct2D layout at the target resolution, and repeated live scene cleanup require running the Windows executable. Do not mark those manual checks complete solely because compilation succeeds.
