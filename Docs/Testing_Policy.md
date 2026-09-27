# Testing and Branch Policy

## Purpose

This repository is a personal game-engine project. Tests exist to catch expensive regressions, not to maximize test count or coverage.

The default rule is:

> Keep a test when a failure would be hard to diagnose by running the editor/game normally.

Tests that only freeze implementation spelling, temporary migration structure, telemetry fields, or source-code text should not become permanent gates.

## Branch model

- `master`: backup/stable snapshot. Do not use for daily development.
- `develop`: current integrated development state.
- short-lived feature/refactor branches: branch from `develop`, merge back to `develop`, then delete after validation.
- temporary integration branches: allowed when several old branches must be reconciled. Delete after the resulting `develop` state has been built and checked.

Do not stack new long-lived work on old feature branches after its parent change has reached `develop`.

## Automatic CI gates

Only four workflow groups are automatic.

### 1. Windows Build

Build the complete Debug x64 solution on changes targeting `develop`.

Release x64 is a manual check before an important snapshot/release.

### 2. Engine Core Smoke

Small, deterministic contracts for failure modes that are difficult to notice or diagnose manually:

- JobSystem execution
- dependency scheduler ordering
- SystemAccess conflict rules
- DirectPaged component storage
- transform rotation math
- point-shadow face layout
- local-light shadow projection
- packed-light traversal
- shadow participation
- RHI null/backend contracts
- player-view refresh policy

A new test belongs here only when it protects a stable engine invariant.

### 3. AgentOS Core

Automatic AgentOS CI runs a representative core subset. The full historical AgentOS suite remains available with manual workflow dispatch or `make full`.

The automatic set should cover orchestration boundaries rather than every helper class.

### 4. Game Rules

Game-specific automatic tests are limited to deterministic rules that are cheap and portable:

- ElemenTactics rules
- MiniGameCollection rules

Presentation, layout, scene composition, and feel are validated by running the game/editor.

## Manual/diagnostic tests

Keep useful focused tests in `Tests/` even when they are not automatic CI. Examples:

- WARP / real D3D11 rendering checks
- detailed RHI / RenderGraph checks
- StaticBatch internals
- GPU timing and performance diagnostics
- detailed culling and procedural-mesh checks
- full AgentOS regression suite

Run these when changing the subsystem they cover or before a larger engine snapshot.

A test source file does not need its own GitHub Actions workflow.

## Tests that should not be permanent

Delete or rewrite tests whose main assertion is one of the following:

- source text contains a class/function/token
- source text does not contain a native API name
- a migration-specific private implementation still has an exact shape
- a telemetry/debug struct stores fields that were just assigned
- an enum/flag has the same numeric value as its declaration
- a temporary probe prints internal debugging state

Source-text checks may be used temporarily during a migration, but should be removed when the migration is complete.

## Adding new tests

Before adding a test, answer all three:

1. What realistic regression does this catch?
2. Would normal editor/game execution reveal it quickly?
3. Is this the smallest stable public/behavioral contract that detects it?

If #1 is unclear, do not add the test.
If #2 is yes, prefer manual validation unless the failure is destructive or nondeterministic.
If #3 depends on private spelling/source layout, test a higher-level behavior instead.

## CI workflow rule

Do not create one workflow per test or per migration step.

Add new automatic tests to one of the existing consolidated workflows. A new workflow is justified only for a genuinely different platform/runtime environment or permission model.

## Current optimization decision

The 2026-09-27 cleanup intentionally keeps broad historical test sources for manual use while removing low-signal source-inspection/probe tests and collapsing the workflow surface to four groups.

This is deliberately conservative: prune CI aggressively first, then delete additional manual tests only when the subsystem is touched and their value can be judged with current implementation context.
