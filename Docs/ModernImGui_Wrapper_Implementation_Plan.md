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
- Removed the bordered Inspector child surface and separated components with spacing instead of another outer box.
- Removed the hard separator immediately before `Add Component`.

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

## Completed: Step 4 — Width-safe Property Controls

- Converted `TextureComponent` fixed cursor offsets into table-based width contracts.
- Reserved explicit label, stretch field and clear-action columns for texture paths.
- Made texture previews stack vertically at narrow widths.
- Removed inner vertical borders from Transform and Collider property tables.
- Moved Transform uniform-scale lock into a dedicated wrapped toggle row.
- Removed the Parent input's stock `-` and `+` steppers.
- Replaced Collider general and trigger checkboxes with wrapped toggles.
- Preserved Collider toggle undo by emitting `PropertyChangeCommand<bool>` entries.
- Replaced rotation lock checkboxes with compact selected axis buttons.
- Replaced Collider add/remove controls with full-width and contextual wrapped actions.

Remaining shared-input work:

- wrap scalar, vector and slider presentation while retaining existing drag tracking and undo
- centralize disabled, validation-error and read-only states
- migrate remaining path fields and suffix buttons
- replace high-frequency stock combos where selection presentation remains visually dominant

## Completed: Step 5a — Assets Browser

- Wrapped refresh, search and rename actions.
- Wrapped asset tiles with pointer-down feedback.
- Reduced paths to supporting metadata.
- Added full-name tooltips for truncated files.
- Removed unnecessary horizontal scrolling from browser panes.

Remaining Assets work:

- custom directory rows
- breadcrumb navigation
- asset selection state and context actions

## Completed: Step 5b — Viewport Toolbar

- Replaced stock image buttons with a dedicated toolbar material surface.
- Converted Stop, Play/Pause and Step into consistent wrapped actions.
- Made Play/Pause the current primary action.
- Replaced the long render-layer preview string with a concise visible-count summary.
- Replaced the selected-blue multi-select combo with an anchored popup of wrapped layer toggles.
- Added explicit Show All and Hide All actions.
- Reduced camera speed to supporting toolbar metadata.

Runtime validation still required:

- verify toolbar fit at narrow Editor View widths
- verify layer popup position in docked and floating viewports
- verify render-layer toggles remain open for repeated changes
- verify Play/Pause/Stop/Step state transitions and focus behavior

## Next: Step 6 — Remaining High-visibility Surfaces

Migrate in this order:

1. remaining stock Combo/Selectable and nested component TreeNode presentation
2. B.R.A.I.N. header, timeline, phase rail and composer
3. Performance Monitor and Debug Log controls
4. Settings surfaces and save feedback
5. Dock-tab appearance achievable through public style configuration

Docking internals remain outside the wrapper-only boundary. Debug-only tools may continue to use stock ImGui controls where visual consistency is not material.

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
