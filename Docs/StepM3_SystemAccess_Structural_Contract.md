# Step M3 SystemAccess Structural Contract

## Scope

The migration plan calls out that immediate Entity / Component structure changes from parallel tasks are unsafe unless structural access is declared correctly.
This step adds a small shared contract layer for structural access intent.

## Changes

- Added free helper functions in `SystemAccess.h`:
  - `IsStructuralAccessDeclared`
  - `IsImmediateStructuralWriteAllowed`
  - `IsStructuralCommandEmissionAllowed`
  - `RequiresStructuralIsolation`
- Added member helpers on `SystemAccess`:
  - `CanEmitStructuralCommands`
  - `CanWriteWorldStructureImmediately`
  - `RequiresStructuralIsolation`
- Updated `SystemAccess::ConflictsWith` to use the shared isolation predicate instead of checking enum values inline.
- Added `Tests/SystemAccessContractSmokeTest.cpp` to lock the contract.
- Added `.github/workflows/system-access-contract.yml` for a narrow Windows smoke check when this contract changes.

## Contract

- `StructuralAccess::None`
  - cannot emit structural commands
  - cannot perform immediate structural writes
  - does not force isolation by itself
- `StructuralAccess::EmitCommands`
  - may emit deferred structural commands
  - may not mutate registries immediately
  - does not force full isolation by itself
- `StructuralAccess::ExclusiveWorldWrite`
  - may perform immediate structural writes
  - may emit structural commands
  - forces structural isolation in scheduling conflict checks

This is the foundation for later debug guards that reject immediate registry mutation from tasks without `ExclusiveWorldWrite`.
