# MiniGame Collection Validation Runbook

## Branch and base

- Work branch: `game/minigame-collection`
- Base branch: `refactor/ecs-scheduler-foundation`
- Original base commit: `355b9ac450d2461bac3fffb49820d73d8b2e8f2e`
- Draft pull request: `#48`

The game branch owns all mini-game-specific code and assets. Do not copy these runtime classes into the PR #45 refactoring branch.

## Startup architecture

`GameApplication` remains independent from the mini-game collection. It only creates the Engine context and executes `Initialize`, `Run`, and `Shutdown`.

Start the collection through the normal Scene workflow:

```text
Asset/Game/MiniGameCollection/Scene/Entry/MiniGameCollectionEntry.scene
```

Expected composition:

```text
Normal Scene launch
  -> MiniGameCollectionEntry.scene
       -> queue MiniGamePersistent.scene
       -> apply additive load outside ECS schedule execution
       -> Persistent Scene starts
       -> Entry Scene destroys itself
       -> Persistent + selected MiniGame Scene
```

Do not load a MiniGame Scene from `GameApplication`. Do not use `MiniGamePersistent.scene` as the normal entry point.

## Deferred additive loading

`Scene::Initialize` registers Component Storage and must not execute while an ECS schedule is active.

All Entry, selection, retry, next-game, and title-return transitions therefore use the queued additive loading boundary. Runtime scripts must not call `LoadFromFilePath` directly.

## Runtime UI boundary

Player-facing UI uses `MiniGameRuntimeUi` and Direct2D / DirectWrite composited into PlayerPass.

ImGui is allowed only for explicit diagnostics:

- Component Inspector
- Backshot F3 rear-cone / shot visualization
- Existing Engine editor and debug windows

Selection, countdown, score, Result Menu, and normal presentation must remain usable with all debug ImGui windows hidden.

## Common gameplay controls

```text
WASD / arrow keys : move
Space             : game-specific action when used
```

## Game selection controls

```text
Up / Down : select game
Space     : start selected game
```

The selection order is:

1. Color Territory
2. Sheep Roundup
3. Backshot

## Shared Result Menu

Every formal mini-game Result uses the same three-row Direct2D menu:

```text
もう一度
次のゲーム
タイトルに戻る
```

Controls:

```text
Up / Down / Left / Right : move cursor by one row
Space                    : confirm selected row
```

Contract:

- Initial selection is `もう一度`.
- Selection is clamped and does not wrap at either end.
- From the initial selection, `タイトルに戻る` requires two downward cursor moves and then Space.
- Moving the cursor never performs a transition by itself.
- Only Space confirms.
- Escape is reserved for the Engine's application-exit request and is not a Result navigation key.
- `R`, `N`, `B`, and Backspace are not direct Result shortcuts.
- Retry or re-entering a game resets the cursor to `もう一度`.

Manual Result validation:

1. Finish a mini-game.
2. Confirm that `もう一度` is highlighted.
3. Press Down once and confirm `次のゲーム` is highlighted without transitioning.
4. Press Down again and confirm `タイトルに戻る` is highlighted without transitioning.
5. Press Escape and confirm no mini-game transition is submitted.
6. Press Space and confirm that the selected action alone executes.
7. Repeat for Retry, Next Game, and Return to Title in all three games.

## Presentation Spike

Scene:

```text
Asset/Game/MiniGameCollection/Scene/PresentationTest/PresentationSpike.scene
```

During the timing phase, Space stops the marker. After the result, direct R/N/B/Escape guidance is removed and the shared arrow / Space action routing is used.

The Presentation Spike is a diagnostic Scene. The three formal games are the acceptance target for the full visual three-row Result Menu.

## Color Territory

Duration: 40 seconds.

Validation:

- Walking to a new Tile paints it.
- A contested Tile does not alternate ownership every frame.
- Normal paint presentation is throttled and does not flicker excessively.
- BOMB clear / claim / explosion results are readable.
- STAR speed and invulnerability are readable.
- STAR contact attack is consumed after one successful hit.
- Result Menu uses arrows and Space only.

## Sheep Roundup

Duration: 50 seconds.

Validation:

- Sheep flee from controlled player influence rather than rigid-body randomness.
- Normal sheep award 1 point.
- Golden sheep award 3 points and are visually distinct.
- Scored sheep slots are recycled until the timer expires.
- Early game maintains a readable flock.
- FLOCK RUSH increases the active flock and golden frequency in the second half.
- CPU target holding keeps its intent readable.
- Result Menu uses arrows and Space only.

## Backshot

Duration: 35 seconds, or until one combatant remains.

Controls:

```text
WASD / arrow keys : move and face
Space             : shoot forward
F3                : debug overlay only
```

Validation:

- A normal shot produces a visible tracer.
- Obstacles block line of sight deterministically.
- Rear hits eliminate; front and side hits guard.
- CPU waits for a rear opportunity instead of firing constantly from the front.
- Result Menu uses arrows and Space only.

## Scene cleanup order

```text
Lock gameplay input
Wait for finishing presentation
Cancel presentation by SceneToken
Shutdown rules
Unload the MiniGame Scene
Wait for removal
Queue the requested target Scene, or complete title return
Wait for target Scene presence
```

After every transition verify:

- No prior player, CPU, sheep, Tile, pen, obstacle, Timer, or Runtime Entity remains.
- No prior one-shot Audio Voice remains active.
- No prior Effect, Particle, local Light pulse, HUD Burst, or Screen Flash remains.
- Camera returns to its registered base transform.
- Result Menu selection does not leak into the next result.
- The new Scene receives a new SceneToken.

## Automated validation

Workflow:

```text
.github/workflows/minigame-collection-core.yml
```

Automated checks cover:

1. GameApplication independence.
2. Entry Scene composition.
3. Deferred additive loading.
4. Player-facing ImGui prohibition.
5. PlayerPass Runtime UI composition.
6. Result Menu labels, arrow navigation, Space confirmation, clamped selection, and Escape-to-title remapping prohibition.
7. Portable C++20 rule and presentation smoke tests.
8. Required Scene YAML structure.
9. Complete Debug x64 solution build with MSVC.

Visual cursor behavior, input feel, audio balance, effect intensity, and repeated live Scene cleanup require local Windows execution and must not be marked complete only from compilation.
