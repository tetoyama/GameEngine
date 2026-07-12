# MiniGame Collection Validation Runbook

## Branch and base

- Work branch: `game/minigame-collection`
- Base branch: `refactor/ecs-scheduler-foundation`
- Base commit: `355b9ac450d2461bac3fffb49820d73d8b2e8f2e`
- Draft pull request: `#48`

The game branch owns all mini-game-specific code and assets. Do not copy these runtime classes into the PR #45 refactoring branch.

## Startup and Multi-Scene composition

`GameApplication` is not responsible for choosing or loading the MiniGameCollection Entry Scene. It remains a generic Engine host.

Open the following scene through the normal Scene workflow, then enter Play:

```text
Asset/Game/MiniGameCollection/Scene/Entry/MiniGameCollectionEntry.scene
```

The Entry Scene serializes the initial Multi-Scene composition:

```yaml
AdditiveScenePaths:
  - Asset/Game/MiniGameCollection/Scene/Persistent/MiniGamePersistent.scene
DestroyEntryAfterLoaded: true
```

`MiniGameCollectionEntryRuntime` queues every path in `AdditiveScenePaths`. Scene initialization is applied at the next safe frame boundary before the ECS schedule starts. After all declared scenes are active, the Entry Scene destroys itself when `DestroyEntryAfterLoaded` is true.

The Persistent Scene owns the common camera, directional light, selection UI, countdown/result presentation, pooled audio voices, fallback one-shot effect voices, screen flash, camera shake, UI tween state, and scene transition service.

Mini-game scenes are loaded additively and contain only their rules runtime. Stage, player, CPU, tile, sheep, pen, obstacle, and presentation fallback entities are created through the queued structural API.

Do not use the Persistent Scene as the normal startup test. The Entry Scene is the validation entry point for this feature.

## Runtime UI boundary

Player-facing mini-game UI must not use ImGui.

The following UI is rendered through `MiniGameRuntimeUi`, `MainRenderer`, and Direct2D / DirectWrite:

- Entry loading state
- game selection
- rule and control explanation
- timer
- score and alive-state row
- countdown and GO display
- Presentation Spike timing track and outcome
- result, retry, selection-return, and next-game guidance
- score / hit HUD burst
- screen flash overlay

Runtime scripts enqueue 2D commands during the Render schedule. They must not draw Direct2D immediately because `RenderSystem.Command.Submit` runs in the Late Render phase and would overwrite the UI when copying PlayerPass to the SwapChain.

ImGui is permitted only for explicit development and diagnostic surfaces:

- component `inspector()` output
- Backshot F3 hit / rear-cone debug overlay
- existing Engine editor and debug windows

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

### Item rush

The final 20 seconds add deterministic Bomb and Star drops.

- Unclaimed Bomb: clears the surrounding 3x3 area and stuns players in range.
- Claimed Bomb: paints the surrounding 3x3 area with the claimant's color.
- Star: temporary speed, stun immunity, and contact stun.

### Presentation-density stabilization

Routine one-tile scoring is intentionally lighter than leader changes and item events.

- Low-intensity `Score` presentation commands are coalesced per Scene.
- Routine score sound, fallback effect, camera shake, and HUD burst run at most once every 160 ms.
- Leader changes, Bomb, Star, Hit, Success, Failure, and Result presentation remain immediate.

### Contested-tile stabilization

Movement painting now uses all of the following safeguards:

1. A new candidate tile must remain stable for 75 ms.
2. A player has a 120 ms repaint cooldown after a confirmed paint.
3. If multiple players submit the same tile in one Tick, that tile is contested and receives no movement paint.
4. After the contest ends, the remaining player must pass the normal confirmation time before claiming it.
5. Bomb-modified ownership can still be reclaimed, but no longer repaints every frame while players overlap.

Validation points:

- Walking to a new tile changes it to the player's color without feeling delayed.
- Walking onto another player's tile transfers one point from the old owner to the new owner.
- Standing on one tile does not repeatedly score.
- Two players pushing together on one tile do not flip ownership every frame.
- Boundary jitter between adjacent tiles does not create continuous paint events.
- Normal one-tile paint does not cause continuous whole-screen flashing or camera shake.
- Leader changes and item events remain visually strong.
- Player contact produces only a small separation and short knockback.
- CPU targets remain stable long enough for intent to be readable.

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
- CPU selects an intercept point on the side opposite its own pen, then closes in to push.
- Entering a pen immediately confirms the score and prevents rescoring.

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
- CPU target hold and decision delay prevent superhuman retargeting.

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
```

After every transition, verify:

- No previous player, CPU, sheep, tile, pen, obstacle, timer, or runtime entity remains.
- No previous one-shot audio voice remains in `Playing` state.
- No previous fallback effect voice remains visible.
- Camera transform has returned to its registered base position.
- Screen flash alpha is zero.
- Presentation-throttle state for the old SceneToken is cleared.
- The new scene receives a new SceneToken.

## Automated validation

Workflow:

```text
.github/workflows/minigame-collection-core.yml
```

The workflow validates runtime UI boundaries, PlayerPass composition, scene-driven startup, deferred additive loading, presentation contracts, portable C++20 rules tests, required Scene assets, and the full Windows Debug x64 solution build.

## Current status

Current HEAD: `ac2b5cdd090c9d6f27e27a8f606af20f0097ea8c`.

The latest automated jobs are queued. Interactive acceptance should focus on routine presentation density, contested-tile behavior, and whether 75 ms / 120 ms remain responsive during normal movement.
