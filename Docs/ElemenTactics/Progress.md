# ElemenTactics Progress

Last updated: 2026-07-12

## Baseline

- PR #45 branch: `refactor/ecs-scheduler-foundation`
- Baseline HEAD: `3dffea8e7e001413fa3ff71a854a1c54e05ba0f2`
- Dedicated branch: `game/elementactics-boardgame`

## Completed

### Repository isolation

- Created the dedicated branch directly from the current PR #45 HEAD.
- No ElemenTactics files were added to the PR #45 branch.

### Engine audit

- Confirmed multi-scene and deferred scene loading APIs.
- Confirmed keyboard, mouse, wheel, and gamepad input APIs.
- Confirmed Sprite, Audio, Effect, Particle, Camera, Prefab, ECS safe-reference, and command-buffer paths.
- Confirmed `LLAMAService` asynchronous model/agent creation and `LLAMAAgent` asynchronous generation, stop, output, and reset paths.
- Identified missing general runtime text support and missing request-level LLM timeout/callback facade.

### Pure rules engine

Implemented:

- all 19 board cells and center identity
- arbitrary valid eight-card distribution and ordering
- zero-card piece omission
- complete matchup table
- legal action generation
- two-action turns
- move and battle targeting
- battle rotation, loss, and draw behavior
- piece defeat and attacker movement rule
- scouting
- center reorder and reuse cooldown
- immediate Light/Dark defeat
- viewer-filtered public information and event history
- invariant checks

### AI foundation

Implemented:

- conservative public-information belief state
- legal-action heuristic scoring
- critical-card attack/defense values
- scouting and center value
- deterministic near-tie variation
- center reorder ordering heuristic
- compact LLM prompt
- structured action parsing
- exact legal-list validation
- public-reason sanitization

### Automated validation

The standalone C++20 smoke test passes with warnings treated as errors under the available local compiler. A Windows/MSVC GitHub Actions workflow is included.

Covered cases:

- 25/25 matchup entries
- `8-0-0`, `7-1-0`, `6-1-1`, `4-4-0`, `3-3-2`
- invalid deck rejection
- zero-card pieces not spawned
- battle and draw deck mutation
- attacker position rules
- Light/Dark immediate game set
- same-piece second action
- repeated same-pair scouting
- center reorder validation and cooldown
- public hidden-order boundary
- heuristic legal action
- LLM legal and illegal output handling

## Currently playable

The rules model can execute a complete match programmatically, and AI components can choose validated actions. There is not yet a player-facing runtime scene, board, deck setup screen, or result screen.

## Open work

1. Confirm the Windows CI run on the branch/PR.
2. Add runtime text facade.
3. Implement dedicated game scene and flow controller.
4. Implement original board/card/piece assets and runtime layout.
5. Implement deck setup and local-player privacy handoff.
6. Implement board input, battle/scout presentation, and center reorder UI.
7. Implement heuristic match controller and two-action reevaluation.
8. Implement `LLAMAService` facade with timeout, cancel, generation ID, reset, and fallback.
9. Implement public-reasoning HUD.
10. Add audio/effects and critical defeat presentation.
11. Run full Debug/Release builds and repeated retry/teardown validation.

## Known blockers and risks

- Dynamic Japanese UI cannot meet completion requirements until runtime text support exists.
- The current LLM API is agent-oriented rather than request-oriented; stale output must be isolated explicitly.
- Prefab instantiation uses immediate structure mutation; runtime-spawn patterns must be reconciled with scheduled Queue rules.
- The initial belief model is not yet multi-hypothesis sampling. It is safe against hidden-information leakage but strategically simple.
