# MiniGame Collection Phase 1: Common Briefing Foundation

Status: `[完了]` 2026-07-14

Canonical record:

```text
Docs/MiniGame_Collection_Phase1_Briefing.md
```

## Implemented

- Portable `MiniGameBriefingModel`
- Full / Compact mode
- Step order, minimum display, success, reset, Ready, Complete
- Enter 1.0 second hold skip
- Release-to-arm protection against carried Selection Enter input
- Skip progress reset on early release
- Skip transitions to Ready, not directly to gameplay
- Direct2D `MiniGameBriefingPresenter`
- Session-level per-game briefing completion state
- Portable C++20 tests
- Windows Debug x64 presenter compile validation

## Integration boundary

Game-specific Briefing steps are intentionally not connected yet.

- Color Territory integration: next game-specific phase
- Sheep Roundup integration: next game-specific phase
- BackShot integration: after route topology, Boost, and temporary blocker specifications settle

Until those integrations are added, the live game flow remains unchanged and no Briefing screen is shown during normal play.
