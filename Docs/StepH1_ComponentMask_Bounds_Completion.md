# Step H1 ComponentMask Bounds Completion

## Scope

`ComponentMask` uses `std::bitset<MAX_COMPONENTS>` and component type IDs are assigned as `ComponentTypeID`.
Previous contract text called out that direct `set` / `test` / `reset` calls can throw when a component type ID reaches the mask capacity.

This note records the current mitigation for that boundary.

## Changes

- Increased `MAX_COMPONENTS` from 64 to 256.
- Added central helper functions in `IComponentStorage.h`:
  - `IsComponentMaskIndexValid`
  - `TrySetComponentMaskBit`
  - `TryResetComponentMaskBit`
  - `TestComponentMaskBit`
- Replaced direct `ComponentMask::set` / `test` / `reset` calls in `ComponentRegistry` with the helper functions.
- Added debug assertions at registration, add, query, and mask mutation points.
- Kept release behavior non-throwing for out-of-range component IDs where the operation can be rejected safely.
- Extended `ComponentRegistryGrowthPolicySmokeTest` to cover helper boundary behavior:
  - index `0`
  - index `MAX_COMPONENTS - 1`
  - out-of-range index `MAX_COMPONENTS`

## Notes

This does not convert the registry to an unbounded dynamic component mask.
It raises the practical limit and prevents accidental `std::bitset` range exceptions from direct calls.
A future archetype or dynamic mask implementation can replace this fixed mask without changing the high-level registry call sites again.
