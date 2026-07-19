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

## Interaction Reference

The wrapper follows the transferable parts of Emil Kowalski's `apple-design` skill:

- response starts on pointer-down, while action commit remains on release
- interaction motion is spring-based, critically damped by default and interruptible
- presentation values and velocity are preserved when a target changes
- hierarchy is communicated through material weight, luminance and spacing rather than heavy borders
- destructive and secondary actions stay contextual instead of permanently competing with primary content
- reversible controls follow symmetric visual paths
- reduced-motion, reduced-transparency and increased-contrast behavior are component-level concerns
- feedback must remain useful and restrained rather than decorative

Web-only implementation details such as CSS transitions, Pointer Events and `backdrop-filter` are not copied literally. Their behavioral intent is translated to ImGui IDs, frame state and `ImDrawList` drawing.

Reference: `https://github.com/emilkowalski/skills/tree/main/skills/apple-design`

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
  - `TextField`
  - `SearchField`
  - `Badge`
  - `SectionHeader`
  - `TreeRow`

### Apple interaction pass

- Press visuals now change on the pointer-down frame instead of waiting for the spring to advance from zero.
- Release and reversal still use the existing spring velocity, avoiding fixed-duration transitions.
- Default springs are critically damped; no decorative overshoot is used for ordinary editor controls.
- Buttons, toggles, fields and sections now expose keyboard focus rings.
- Search fields include a stable visual origin and a focus transition.
- Buttons and sections use a subtle top material edge and shallow depth shadow.
- Hierarchy selection uses a translucent selected surface plus a directional accent strip.
- Toggle off-state receives an explicit outline instead of disappearing into the panel.
- Added wrapper-level preferences for reduced motion, reduced transparency and increased contrast.
- Hidden controls continue to clean stale motion state periodically.

## Completed: Step 2 — Inspector Pilot

- Replaced entity-state checkboxes with two-column `Toggle` controls.
- Replaced component tree headers with `SectionHeader`.
- Replaced `+ Add Component` with a full-width wrapper button.
- Wrapped component search while preserving the existing popup and command flow.
- Moved the entity name to `TextField` and reduced ID/Prefab to supporting metadata.
- Replaced the permanently visible destructive entity button with a contextual action menu.
- Replaced permanently visible component remove labels with consistent `...` action menus.
- Kept component inspection and command-based add/remove behavior unchanged.
- Removed the Inspector's always-on horizontal scrollbar; individual properties must fit or clip within the available width.

Runtime validation still required:

- verify narrow Inspector widths
- verify component section state persistence
- verify add/remove undo and redo
- verify Japanese text input in the entity name field
- verify action popup positioning in floating and docked windows
- verify no component inspector still creates horizontal overflow

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
- Centralize disabled, validation-error and read-only states.
- Fix width contracts for path fields, suffix buttons and multi-column component inputs.
- Preserve 1:1 pointer tracking for drags while animating only non-positional presentation feedback.

## Step 5 — Assets, Viewport and B.R.A.I.N.

Migrate in this order:

1. Assets Browser toolbar, search, directory rows and asset cards
2. Editor/Player View overlays and transport controls
3. B.R.A.I.N. header, timeline, phase rail and composer
4. Settings and performance/debug surfaces

Debug-only tools may continue to use stock ImGui controls where visual consistency is not material.

## Accessibility Contract

- `reducedMotion`: snap presentation values while preserving color/visibility feedback.
- `reducedTransparency`: use solid popup and overlay surfaces.
- `increasedContrast`: strengthen borders, separators, scrollbars and focus rings.
- Focus must not depend only on color.
- Destructive actions must remain textually explicit inside their context menus.
- Minimum control height is maintained independently from visual scale animation.

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
