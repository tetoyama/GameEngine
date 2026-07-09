# Step H4 Query Structure Version Guard Completion

## Scope

Query / View iteration previously relied on a contract that Entity / Component structure must not be modified while a query is being iterated.
That contract was documented but not enforced.

This note records the current debug guard implementation.

## Changes

- Added `EntityRegistry` structure version tracking.
  - `Create`
  - `CreateID`
  - `Destroy`
- Exposed `EntityRegistry::GetStructureVersion()` and `GetStructureVersionPointer()`.
- Added `ComponentRegistry` structure version tracking for component structure mutations.
  - successful `AddComponent`
  - successful `RemoveComponent`
  - successful `RemoveComponentByID`
  - `OnEntityDestroyed` when the entity had a component mask
- Added `ComponentRegistry::GetRegistryStructureVersion()`.
- Extended `ComponentQueryView` to capture expected Entity / Component structure versions at query creation.
- Added debug assertions in `ComponentQueryView::Iterator` to detect structure changes during iteration.
- Extended `ComponentRegistryGrowthPolicySmokeTest` to cover version increments and stable query iteration.

## Contract

`ComponentQueryView` is still a lightweight non-owning view.
It does not snapshot alive entities or component masks.
Instead, it asserts in debug builds if either source registry changes after the view is created.

This preserves the zero-allocation query path while making accidental immediate structure mutation visible during development.
