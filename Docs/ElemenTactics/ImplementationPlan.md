# ElemenTactics Implementation Plan

## Baseline

- Repository: `tetoyama/GameEngine`
- Source pull request: `#45` (`refactor/ecs-scheduler-foundation`)
- Baseline HEAD: `3dffea8e7e001413fa3ff71a854a1c54e05ba0f2`
- Game branch: `game/elementactics-boardgame`
- Rule authority: the confirmed ElemenTactics specification supplied with this task
- Scratch `.sb3`: reference-only when supplied; extracted artwork must not be reused

The game branch is based directly on the latest PR #45 HEAD. Game-specific code must not be committed to `refactor/ecs-scheduler-foundation`.

## Architecture boundaries

```text
Pure rules and public-information model
        ↓
Legal action generation and validation
        ↓
Heuristic evaluation / LLM selection adapter
        ↓
Rules revalidation and state mutation
        ↓
Runtime controller and presentation
```

The `Core`, `Rules`, and deterministic parts of `AI` must remain independent of Entity/Component storage, render internals, runtime UI, llama.cpp contexts, and presentation handles.

Runtime integration may retain only `EntityRef` and `ComponentRef<T>` for long-lived engine references. Structural changes during scheduled execution must use the Queue APIs.

## Fixed rules represented by the rules engine

- Radius-2 hex board with 19 cells and one center cell
- One to three pieces per player, determined by non-empty deck allocation
- Exactly eight cards per player: Fire ×2, Water ×2, Wood ×2, Dark ×1, Light ×1
- Arbitrary distribution and ordering, including `8-0-0`
- Complete 25-entry element matchup table
- Exactly two successful actions per turn; no pass or early end
- Unlimited-range move or battle target selection
- Winner rotation, loser loss, and draw rotation
- Piece removal at zero cards
- Attacker advance only after an attacker victory that defeats the defender
- Scout reveal and rotation for both selected pieces
- Optional no-cost center reorder on a new arrival
- Reorder reuse only after exit, a later re-entry, and skipping the immediately following owner turn
- Immediate game set when Light or Dark is lost
- Public history and viewer-filtered state that never exposes an opponent's hidden order

## Configurable unspecified values

| Setting | Initial value | Reason |
|---|---:|---|
| First player | Player 1 | Deterministic test and local-match default; mode setup may randomize later |
| Board camera | Fixed 2.5D perspective | Keeps all 19 cells and both HUD columns readable at 1280×720 |
| Normal presentation step | 0.25 s | Preserves cause and effect without making repeated actions slow |
| Critical-card defeat presentation | 1.2 s | Light/Dark loss must read more strongly than normal loss |
| AI near-tie variation | Small deterministic noise | Avoids exact repetition without making random play the primary policy |
| LLM response timeout | 8 s target | Timeout always falls back to heuristic AI |

All values must be exposed as game configuration rather than scattered presentation constants.

## Implementation phases

### Phase 0 — Baseline and API audit

Status: **complete for initial foundation**

- Confirm PR head branch and SHA
- Create dedicated game branch
- Audit Scene, Input, Sprite, runtime text, Audio, Effect, Particle, Camera, Prefab, ECS, safe references, command buffer, and LLM lifecycle
- Record missing APIs and risks

### Phase 1 — Pure rules engine

Status: **implemented**

Files:

- `Core/ElemenTacticsCore.h`
- `Rules/ElemenTacticsRules.h`
- `Rules/ElemenTacticsRules.cpp`

Rules are pure C++ and are not coupled to Engine entities, rendering, UI, or LLM services.

### Phase 2 — Rules tests

Status: **implemented as standalone smoke test**

- All 25 matchup combinations
- Valid and invalid deck allocation
- `8-0-0`, `7-1-0`, `6-1-1`, `4-4-0`, `3-3-2`
- Battle movement and loss rules
- Immediate Light/Dark game set
- Two-action turn behavior
- Repeated scout
- Center reorder and cooldown
- Hidden-information boundary
- Legal heuristic and LLM-adapter decisions

### Phase 3 — Runtime game flow

Status: **not started**

Implement a dedicated ElemenTactics runtime scene and explicit flow state:

```text
Title → Mode Select → Rules → Deck Setup → Match Introduction
      → Battle Board / Center Reorder → Result → Retry or Title
```

A single dedicated match scene with internal screen state is preferred initially. It reduces async LLM and event-subscription leakage across frequent screen transitions.

### Phase 4 — Board and player input

Status: **not started**

- Generate 19 board cells from the pure board definition
- Build board-to-screen projection and hit testing behind a game-local input facade
- Select piece, destination, battle target, and scout target
- Block board input while center reorder or transition presentation is active
- Keep the authoritative state only in the rules controller; presenters mirror it

### Phase 5 — Runtime UI and deck setup

Status: **blocked by runtime text API gap**

The current engine has Sprite rendering but no general runtime text component in the registered component list. Add a thin engine-owned `RuntimeTextComponent`/renderer facade, or equivalent glyph-sprite facade, before implementing Japanese player-facing text. It must not expose D3D11 objects to game code.

Deck setup must support moving all cards among three columns, reordering each column, empty columns, exact multiset validation, and a hidden handoff screen for local two-player mode.

### Phase 6 — Heuristic AI

Status: **foundation implemented**

- Uses viewer-filtered public state and own visible deck only
- Scores only generated legal actions
- Values Light/Dark survival, critical captures, scouting entropy, and center reorder
- Reevaluates after the first action through a fresh public view
- Applies limited deterministic near-tie variation

The initial belief model is intentionally conservative. Later work should add sampled hidden-deck hypotheses without reading the actual opponent state.

### Phase 7 — LLM integration

Status: **adapter implemented; service facade not started**

- Prompt contains only own full deck, opponent public state, and enumerated legal actions
- Response is compact structured data
- Selected action must exactly match the enumerated legal list
- Public text is length-limited and private chain-of-thought is not requested

Add an `ElemenTacticsLlmFacade` around `LLAMAService`/`LLAMAAgent` with per-request generation ID, timeout, cancel, scene-lifetime cancellation, safe output polling, teardown reset, and heuristic fallback.

### Phase 8 — Presentation and original assets

Status: **not started**

Create original symbols and materials for Fire, Water, Wood, Dark, and Light. Do not use Scratch-extracted assets. Every element must differ by shape/symbol as well as color.

Presentation must be event-driven from applied rule results. Critical Light/Dark loss must have a distinct, stronger sequence than normal card loss.

### Phase 9 — Stabilization

Status: **not started**

- Full Debug and Release x64 builds
- Standalone rules smoke test
- Human vs heuristic completion
- Human vs LLM completion and timeout fallback
- Local two-player privacy handoff
- Ten or more retries without state, entity, audio, effect, event, or LLM leakage
- 1280×720 overlap check
- Application shutdown during active LLM generation

## Completion gate

The game is not complete until all player-facing flow, runtime UI, original assets, presentation, heuristic fallback, LLM lifecycle handling, retry/reset behavior, and full-engine builds pass. Passing the pure-rules tests is a foundation milestone, not the final completion state.
