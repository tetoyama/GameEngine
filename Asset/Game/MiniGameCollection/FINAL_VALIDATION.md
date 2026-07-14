# MiniGame Collection Final Validation Handoff

Status: implementation complete / manual play verification pending

## Completed implementation

### Shared briefing

- Full and Compact briefing modes
- Step-gated interactive guidance
- Per-step input restrictions and scene update suspension
- Enter hold for 1.0 second to skip
- Release-to-arm protection against carried Selection input
- Briefing cleanup before normal Ready and Countdown flow
- Per-game session completion tracking

### Shared telegraphs

- Warning -> Armed -> Resolving -> Aftermath lifecycle
- Major limit 1 / Minor limit 2
- SceneToken cleanup
- World marker, range, path, and countdown presentation
- Major warning suppression of ordinary Score and NearMiss presentation

### Color Territory

- Full interactive briefing
- Item Rush warning starts 3 seconds before activation
- Bomb and Star landing forecast
- Bomb 3x3 danger range display through fuse
- Extended 2.8 second Bomb landing and 2.4 second Star landing warning
- Existing Bomb and Star effects preserved

### Sheep Roundup

- Full interactive briefing
- FLOCK RUSH warning starts 4 seconds before activation
- Golden sheep position and 3-point value forecast 2.2 seconds before spawn
- Normal flock replenishment grouped into low-priority area warnings
- Existing 24-slot endless flock and scoring preserved

### Backshot

- Route topology with directional connection masks
- Straight, corner, T-junction, cross, dead-end, empty, and block cells
- Deterministic Layout A / B / C rotation
- Corner auto-turn and junction stop behavior
- Reserved route and player collision safety
- Timed 5.0 second, 1.4x Boost with fixed two-slot pool
- 2.8 second Boost landing warning
- Temporary blocker with 4.0 second warning, 6.0 second closure, and 1.0 second reopen warning
- Occupied and reserved cells excluded
- Connectivity safety check before closure
- Full interactive briefing for slide, corner, junction, front Guard, rear elimination, Boost, and blocker avoidance

### Stability

- Result menu remains arrow keys plus Space
- Escape remains application exit only
- Player-facing UI remains Direct2D, not ImGui
- Scene loads remain deferred outside ECS schedule execution
- SceneToken cleanup covers briefing, telegraph, presentation, Boost, blocker, and transition state
- Route Backshot uses explicit facing compatibility state while continuous movement keeps forward and facing synchronized

## Automated validation gates

The final HEAD must pass all jobs in `MiniGame Collection Validation`:

- Runtime UI boundary
- Result menu controls
- Interactive briefing flow
- Shared telegraph architecture
- Color and Sheep predictive warnings
- Route-based Backshot contract
- PlayerPass runtime UI composition
- Scene-driven entry flow
- Tactile presentation stack
- Deferred additive loading
- Portable mini-game model contracts
- Telegraph lifecycle and priority contracts
- Exact forecast timing contracts
- Presentation retry contract
- Mini-game rule flow
- Legacy Backshot slide contracts
- Route topology, Boost, and blocker contracts
- Required scene asset validation
- Windows Debug x64 full solution build

## Manual validation sequence

Run the following after pulling the final HEAD:

```text
Entry
 -> Color full briefing
 -> Color game
 -> Result / Retry compact briefing, then Enter-hold skip
 -> Next
 -> Sheep full briefing, then mid-briefing Enter-hold skip
 -> Sheep game
 -> Result
 -> Next
 -> Backshot full briefing
 -> Backshot game
 -> Result
 -> Return to selection
```

Repeat the sequence three times and verify:

- Selection Enter does not accidentally skip Briefing
- Enter must be released once before hold progress begins
- Short Enter tap resets progress
- Skip still passes through Ready and the 3-second Countdown
- Gameplay timer, score, and CPU do not advance during Briefing
- Old Briefing and Telegraph UI never remains after transition
- Major warnings are not replaced by ordinary score messages
- Boost and blocker state do not survive retry or scene change
- Old SceneToken resolve events do not fire
- Color Bomb, Star, and Item Rush are understood before activation
- Sheep FLOCK RUSH and golden value are understood before activation
- Backshot route turns, junction stops, Guard, rear elimination, Boost, and blocker are understood before activation
