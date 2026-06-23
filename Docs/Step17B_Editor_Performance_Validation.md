# Step 17-B.2 Editor Performance Validation

## 対象

- AssetsBrowser filesystem metadata cache
- Hierarchy parent / children adjacency table
- DebugLogWindow revision cache and visible-row clipping
- redundant Resize suppression
- Frame Spike Diagnostics

## Build validation

This document commit intentionally starts the standard Windows compile checks after all code and cache-stability fixes have been applied.

- [ ] Engine Debug x64
- [ ] Engine Release x64
- [ ] Script Debug x64
- [ ] Script Release x64

## Runtime validation

### Editor layout

Use the same Scene and panel layout as the 2026-06-23 baseline. Wait at least 60 frames, then record:

- Editor UI Build CPU average
- AssetsBrowser average
- Hierarchy average
- DebugLogWindow average
- GPU Frame Time average

Expected behavior:

- AssetsBrowser no longer performs filesystem enumeration every frame
- Hierarchy no longer performs repeated full-Entity child searches
- DebugLogWindow cost scales primarily with visible rows and log changes

### Asset refresh

1. Create or add an Asset outside the editor
2. Press `Refresh` in Assets Browser
3. Confirm the new item appears
4. Create and delete a folder through the context menu
5. Confirm no crash or invalid tree traversal occurs

### Resize

1. Resize the main window manually
2. Confirm rendering remains valid after `WM_EXITSIZEMOVE`
3. Repeat with the same final dimensions
4. Confirm redundant resource recreation is suppressed
5. Check the recorded Resize CPU time

### Frame spikes

1. Keep `Frame Spike Diagnostics` threshold at 20ms
2. Start the engine and resize once
3. Confirm startup records contain `[Startup]`
4. Confirm resize-related records contain `[Resize]`
5. When an irregular spike occurs, preserve the newest record showing:
   - dominant section
   - Update / Draw / GPU / Render / Editor / Present values
   - dominant Editor panel
   - Resize CPU time when applicable

## Remaining work

- [ ] runtime comparison after editor panel optimization
- [ ] startup and resize spike history confirmation
- [ ] irregular spike capture
- [ ] GPU pass-level timestamps for Shadow / GBuffer / Lighting / PostEffect / ImGui
