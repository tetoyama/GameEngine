# Step M4 Component Replace API Completion

## Scope

`AddComponent` keeps the existing compatibility behavior: adding a component that already exists succeeds and returns the existing component, but constructor arguments are not applied to the existing value.

That behavior is retained for old call sites, while explicit replacement now has a separate API.

## Changes

- Added `ComponentRegistry::ReplaceComponent<T>(Entity, Args&&...)`.
  - Rejects dead entities.
  - Removes the existing component when present.
  - Adds the new component with the supplied constructor arguments.
- Added `ComponentRegistry::SetComponent<T>(Entity, Args&&...)` as an alias for replacement semantics.
- Kept `AddComponent` compatibility semantics unchanged.
- Extended `ComponentRegistryGrowthPolicySmokeTest` to cover:
  - re-`AddComponent` keeps the old value and discards new constructor arguments.
  - `ReplaceComponent` applies the new value.
  - `SetComponent` applies the new value.
  - registry structure version changes are explicit and observable.

## Contract

Use `AddComponent` only for insertion or compatibility paths.
Use `ReplaceComponent` / `SetComponent` when the caller intends to overwrite existing component data.

This prevents silent overwrite assumptions while avoiding a breaking change to old `AddComponent` call sites.
