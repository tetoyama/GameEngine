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

## Step 2 — Inspector Pilot

Use the Inspector as the first production migration target.

- Replace entity deletion with `DangerButton`.
- Replace entity state checkboxes with `Toggle` controls.
- Replace component headers with `SectionHeader`.
- Move component removal into a low-emphasis or contextual action.
- Replace `+ Add Component` with a full-width wrapper button.
- Preserve component inspection, undo/redo and popup behavior.

Success conditions:

- No stock button or checkbox visuals remain in the Inspector header.
- Component editing behavior is unchanged.
- Narrow Inspector widths remain usable.

## Step 3 — Hierarchy Pilot

- Replace the add button and search field with wrapper controls.
- Introduce a custom `TreeRow` wrapper using the existing tree state and drag/drop behavior.
- Remove the permanently visible numeric entity ID from each row.
- Render Prefab state as a badge rather than `(Prefab)` text.
- Preserve selection, rename, parenting and context menus.

Success conditions:

- Selection and drag targets remain unambiguous.
- Inline rename and recursive filtering continue to work.
- Row density remains suitable for large scenes.

## Step 4 — Shared Input Controls

- Add wrapped scalar and vector property fields.
- Integrate with the existing undo-aware functions in `ImGuiFunc`.
- Add segmented controls and compact icon buttons.
- Centralize disabled, focused and validation-error states.

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
