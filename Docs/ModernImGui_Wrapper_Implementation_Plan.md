# Modern ImGui Wrapper Implementation Plan

## Objective

Retain Dear ImGui as the editor's immediate-mode UI kernel while removing the visible stock-ImGui appearance through a wrapper-only customization layer.

The following remain unchanged:

- Dear ImGui source and DX11/Win32 backends
- Docking and multi-viewport support
- ImGuizmo and ImNodes integration
- Existing editor state and command/undo architecture

## Architectural Boundary

```text
Editor screens
    -> MImGui wrapper controls
        -> Dear ImGui public API
            -> existing Win32/DX11 backend
```

`imgui.cpp`, `imgui_widgets.cpp`, backend files and docking internals are not patched in this workstream.

## Completed: Step 1 — Foundation

- Added `ModernImGui/ModernImGui.h` as a header-only wrapper foundation.
- Replaced the previous scattered style function with `MImGui::ApplyTheme()`.
- Removed the initialization-time `WindowRounding = 0` override.
- Added semantic theme tokens for surfaces, text, selection, accent and destructive actions.
- Added persistent spring state keyed by `ImGuiID`.
- Added periodic cleanup so hidden controls do not leak animation state indefinitely.
- Added custom-drawn wrapper controls:
  - `Button`
  - `PrimaryButton`
  - `DangerButton`
  - `Toggle`
  - `SearchField`
  - `SectionHeader`
  - `TreeRow`

## Completed: Step 2 — Inspector Pilot

- Replaced entity deletion with `DangerButton`.
- Replaced entity-state checkboxes with two-column `Toggle` controls.
- Replaced component tree headers with `SectionHeader`.
- Changed component removal to a low-emphasis ghost action.
- Replaced `+ Add Component` with a full-width wrapper button.
- Wrapped component search while preserving the existing popup and command flow.
- Kept component inspection and command-based add/remove behavior unchanged.

Runtime validation still required:

- verify narrow Inspector widths
- verify component section state persistence
- verify add/remove undo and redo
- verify Japanese text input in the entity name field

## Completed: Step 3 — Hierarchy Pilot

- Replaced the add button and search field with wrapper controls.
- Added a custom `TreeRow` with animated hover, selection and disclosure state.
- Removed the permanently visible numeric entity ID from each row.
- Moved entity IDs and Prefab source paths into delayed tooltips.
- Rendered Prefab state as a compact badge.
- Preserved recursive filtering, context menus, inline rename and parenting commands.
- Registered each entity row as one ImGui item so selection, context menus and drag/drop share the same hit target.

Runtime validation still required:

- verify drag source and drop target behavior
- verify inline rename focus and submission
- verify context-menu deletion during recursive drawing
- verify multiple active scenes with overlapping entity IDs
- verify scene and entity expansion persistence

## Next: Step 4 — Shared Input Controls

- Add wrapped scalar and vector property fields.
- Integrate with the existing undo-aware functions in `ImGuiFunc`.
- Add segmented controls and compact icon buttons.
- Centralize disabled, focused and validation-error states.
- Move the Inspector name field and common component properties away from stock frame visuals.

## Step 5 — Assets, Viewport and B.R.A.I.N.

Migrate in this order:

1. Assets Browser
2. Editor/Player View overlays
3. B.R.A.I.N. header, timeline and composer
4. Settings and performance/debug surfaces

Debug-only tools may continue to use stock ImGui controls where visual consistency is not material.

## Validation

Each step must preserve:

- docking and undocking
- multi-viewport windows
- keyboard focus and text input
- drag and drop
- context menus
- command-based undo/redo
- 1280x720 minimum editor usability
- 100%, 125%, 150% and 200% DPI usability

The UI wrapper must not grow motion state without bounds and should keep additional editor CPU cost below 0.3 ms per frame in normal layouts.
