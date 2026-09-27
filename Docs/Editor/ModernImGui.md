# Modern ImGui Editor

Status: **Current editor UI direction — 2026-09-27**

## Goal

ImGuiをそのまま並べるのではなく、GameEngine Editorとして一貫したinteraction hierarchyを作る。

## Boundary

Modern ImGui wrapperはEngine UIの共通interaction / visual helper。

Game runtime UIのためのframeworkではない。

## Implemented areas

現在までに主に次を整理している。

- common wrapper foundation
- Inspector controls
- Hierarchy
- Assets Browser
- viewport context toolbar
- global menu / transport
- panel shortcut
- icon handling
- inspector empty state
- Debug Log
- Performance Monitor

## Global roles

MenuBarは次の役割を分離する。

- primary transport: Stop / Play-Pause / Step
- navigation: Hierarchy / Assets / Inspector / Log / Profiler
- traditional menu: File / Edit / Window / Settings

Window幅が不足する場合はshortcutを無理に詰めず、Window menuへフォールバックする。

## EditorService

Editor panelは `EditorService` が所有し、Initialize / Draw / Finalizeを管理する。

Panel timingはPerformance Monitorへ渡す。

Icon resourceもEditorService lifetimeで管理する。

## AgentOS coexistence

B.R.A.I.N. / AgentOS PanelはModern Editorと同じWindow systemへ統合される。

UI統合のためにAgentOS側へModern ImGui内部依存を広げすぎない。

## Next work

今後のUI改善は独立した巨大Stepではなく、実際に触るhigh-visibility surfaceごとに行う。

優先候補:

- remaining settings surfaces
- complex inspector controls
- accessibility / keyboard flow
- narrow-window layout
- consistent empty/loading/error state

旧Implementation PlanはArchiveへ移動済み。
