# ElemenTactics Progress

Last updated: 2026-07-13

## Baseline

- PR #45 branch: `refactor/ecs-scheduler-foundation`
- Baseline HEAD: `3dffea8e7e001413fa3ff71a854a1c54e05ba0f2`
- Dedicated branch: `game/elementactics-boardgame`
- Draft integration PR: #50

## Completed

### Repository isolation

- Created the dedicated branch directly from the confirmed PR #45 HEAD.
- No ElemenTactics files were added to the PR #45 branch.
- The game branch remains a direct descendant of the recorded baseline.

### Engine audit

- Confirmed multi-scene and deferred scene loading APIs.
- Confirmed keyboard, mouse, wheel, and gamepad input APIs.
- Confirmed Sprite, Audio, Effect, Particle, Camera, Prefab, ECS safe-reference, and command-buffer paths.
- Confirmed `LLAMAService` model/agent creation and `LLAMAAgent` asynchronous generation, stop, output, and reset paths.
- Recorded the safe integration boundaries in `EngineApiAudit.md`.

### Pure rules engine

Implemented:

- all 19 board cells and center identity
- arbitrary valid eight-card distribution and ordering
- zero-card piece omission
- complete matchup table
- legal action generation
- two-action turns
- same-piece repeated actions and battles
- move and battle targeting
- battle rotation, loss, and draw behavior
- piece defeat and attacker movement rule
- scouting
- center arrival by movement or defender defeat
- center reorder and reuse cooldown
- immediate Light/Dark defeat
- viewer-filtered public information and event history
- invariant checks

### Runtime flow and player UI

Implemented without player-facing ImGui:

- dedicated ElemenTactics scene
- Title
- Mode Select
- Rules
- Deck Setup
- local two-player privacy handoff
- Match Introduction
- Battle Board
- Center Reorder
- Result
- Retry and return to Title
- runtime mouse hit testing and board interaction
- Runtime Text facade backed by DirectWrite/WIC and existing Sprite rendering

The current presentation is functional text/symbol UI. Final original card, piece, board, logo, audio, and effect assets are still pending.

### AI and LLM boundary

Implemented:

- conservative public-information belief state
- legal-action heuristic scoring
- critical-card attack/defense values
- scouting and center value
- deterministic near-tie variation
- center reorder ordering heuristic
- compact structured LLM prompt
- structured action parsing
- exact legal-list validation
- final Rules Engine validation before mutation
- request generation/session tracking
- timeout and cancellation
- stale board-state serial rejection
- heuristic fallback
- scene-stop agent shutdown and context reset path
- compact public-reasoning data

### Automated validation

Standalone C++20 contracts are included for:

- 25/25 matchup entries
- `8-0-0`, `7-1-0`, `6-1-1`, `4-4-0`, `3-3-2`
- invalid deck rejection
- zero-card pieces not spawned
- battle and draw deck mutation
- attacker position rules
- Light/Dark immediate game set
- same-piece second action and two consecutive battles
- repeated same-pair scouting
- center reorder validation and cooldown
- center capture triggering reorder without extra action cost
- public hidden-order boundary
- heuristic legal action
- LLM legal and illegal output handling
- LLM request timeout/cancel/reset lifecycle
- board layout and invalid-input non-consumption

The workflow also builds the complete GameEngine solution in Debug x64 and Release x64.

## Latest build investigation

The 2026-07-13 local Debug x64 build reached the ElemenTactics integration files and exposed three primary compile issues:

1. Windows `min`/`max` macros expanded inside `std::min` and `std::max` calls.
2. `ElemenTacticsRuntimeAiBridge.cpp` used obsolete `AgentConfig::n_predict` instead of the current `max_tokens` field.
3. The controller compatibility name was stored separately from `CustomScriptComponent::scriptName`.

Fixes applied:

- undefine Windows `min`/`max` macros at the ElemenTactics LLM and RuntimeText boundaries
- replace `n_predict` with `max_tokens`
- bind the controller compatibility name to the base `scriptName`

The many syntax errors after the first `std::min`/`std::max` failures were cascade errors. A new local Debug x64 build is required to identify any remaining independent issue.

The `yaml-cppd.pdb` LNK4099 messages are non-fatal debug-symbol warnings and were not the cause of the failed GameEngine build.

## Currently playable

The code path now contains a complete functional flow from Title through deck setup, match, result, retry, and Title return. Both local two-player and CPU modes are connected at the model/controller level.

Runtime completion has not yet been verified by a successful full Engine build and interactive launch after the latest fixes. Therefore this is not yet classified as player-ready.

## Open work

1. Rebuild Debug x64 after the latest compile fixes and resolve the next independent error, if any.
2. Build Release x64.
3. Launch the dedicated scene and validate all screen transitions at 1280x720.
4. Verify Runtime Text rendering, mouse coordinates, queue-created UI entities, and cleanup.
5. Verify LLM model loading, asynchronous request completion, timeout, cancellation, and heuristic fallback in the running Engine.
6. Verify local-player privacy handoff and hidden-information boundaries interactively.
7. Add original board, card, piece, attribute-symbol, title-logo, audio, and effect assets.
8. Add battle, scout, reorder, King defeat, Assassin defeat, and GAME SET presentation sequencing.
9. Add board-target highlighting synchronized with public AI reasoning.
10. Run at least ten consecutive retries and Title returns while checking LLM tasks, events, entities, audio, effects, and history cleanup.
11. Update the PR description and final completion checklist after runtime validation.

## Known blockers and risks

- The latest complete Engine build result is still pending after the macro and AgentConfig fixes.
- Runtime Text creates textures dynamically; resource release and repeated-screen rebuild behavior require interactive profiling.
- The current LLM bridge only initializes from an already loaded model. Game-owned model-loading behavior still requires runtime verification and likely refinement.
- The initial belief model is safe against hidden-information leakage but strategically simple; multi-hypothesis sampling remains future AI-strength work.
- Prefab instantiation uses immediate structure mutation; ElemenTactics runtime entities currently use Queue APIs instead.
