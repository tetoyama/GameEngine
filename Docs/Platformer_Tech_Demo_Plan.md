# Platformer Tech Demo Plan

## Branch basis

- Work branch: `game/platformer-tech-demo`
- Base: PR #45 HEAD `7ec9cc546e8f817c55e4d4a07c9cc0176751eec8`
- Base branch: `refactor/ecs-scheduler-foundation`
- Rule: game code must not access RenderWorld, RenderPacket, RHI, D3D11 resources, render passes, static-batch internals, or culling internals.

## Goal

Deliver one short, linear 3D platformer stage that can be played from start to clear. The critical path is movement, jump feel, triple jump, wall kick, coin guidance, stomp combat, fall recovery, a three-hit mini boss, and a clear result.

## Existing engine boundaries reused

- `CustomScriptComponent`: lifecycle, input helpers, collision and trigger callbacks.
- `ComponentRef<T>` / `EntityRef`: persistent safe references. Raw pointers are used only within the current call.
- `EntityCommandBuffer`: `QueueCreateEntity`, `QueueDestroyEntity`, `QueueDestroySelf`, `QueueAddComponent`, and `QueueRemoveComponent` during scheduled work.
- `PhysicSystem`: PhysX rigid bodies, colliders, triggers, layer filtering, raycasts, collision dispatch.
- `TransformComponent`, `CameraComponent`, `ModelRendererComponent`, `MaterialComponent`, `ParticleComponent`, `AudioComponent`, and Prefab/Scene serialization.
- Existing character and camera scripts are reference implementations only. The platformer controller remains game-specific.

## Important API findings

1. `HitInfo` exposes the other `EntityRef` and collision layer, but not contact position or normal.
2. Ground and wall normals therefore come from `PhysicSystem::RaycastWithMask`.
3. `ColliderComponent::pRigidbodyDynamic` is a non-owning PhysX runtime alias. The platformer controller disables PhysX gravity and owns vertical acceleration, matching the existing `CharacterController` pattern.
4. Structural mutation from Update, FixedUpdate, collision, or trigger callbacks must be queued.
5. Runtime-created game entities should prefer Prefab instantiation. Initial gray-box content may be authored as a dedicated Scene.

## New game classes

- `PlatformerSceneAccess`: centralized scene/service lookup so registry access is not scattered.
- `PlatformerMovementSettings`: controller tuning data.
- `PlatformerCharacterController`: camera-relative locomotion, variable jump, coyote time, jump buffer, asymmetric gravity, apex adjustment, ground snap, slope projection, triple jump, wall kick, stomp bounce, damage, knockback, invulnerability, control lock, checkpoint recovery, and clear state.
- `PlatformerCameraController`: constrained follow camera, zone framing, collision shortening, boss framing, and clear framing.
- `PlatformerAnimationController`: animation-state selection with transform fallback when clips are unavailable.
- `PlatformerGameManager`: run state, coins, boss state, checkpoints, clear state, and restart-facing data.
- `PlatformerCoin`: spin/bob, trigger collection, duplicate prevention, feedback event, and delayed destroy.
- `PlatformerCheckpoint`: trigger activation and player respawn target update.
- `PlatformerEnemy`: simple patrol, stomp defeat, contact damage, duplicate defeat prevention.
- `PlatformerMovingPlatform`: deterministic ping-pong platform motion.
- `PlatformerBoss`: telegraph, chase, charge, wall impact, stunned weak point, stomp damage, phase escalation, defeat, and clear request.
- `PlatformerCameraZone`: trigger-driven camera profile changes.
- `PlatformerHud`: coin count, health, boss health, and clear result.

## Engine changes allowed

- Register game components in `componentList.h`.
- Add only a thin game-facing helper if an existing public boundary is missing.
- Do not modify renderer/RHI internals for game behavior.

## Stage plan

1. Start/tutorial platform and low-risk jumps.
2. Long runway and three-height platforms for triple-jump teaching; lower recovery route remains usable.
3. Safe single-wall kick, alternating-wall shaft, then short exposed wall-kick test.
4. Moving platform and one stompable patrol enemy.
5. Boss gate/checkpoint and compact arena.
6. Boss defeat presentation and clear result.

Target coin count: 40. Coins are guidance and feedback, never a progression lock.

## Risks

- No contact normal in collision callbacks: use short multi-direction wall probes with grace time.
- PhysX runtime actor may be created after script start: reacquire the collider each FixedUpdate and disable native gravity when available.
- Scene and prefab YAML are hand-authored initially: validate component names and serialized fields against existing scenes.
- Asset availability is uncertain: gameplay must remain complete with primitive models, transform animation, material changes, particles, and existing audio assets.
- Full Windows build cannot be run in the current connector environment; branch CI and repository checks are used for compile validation.

## Completion gates

- Dedicated branch based on the recorded PR #45 HEAD.
- Dedicated game code and asset directories.
- Start-to-clear path with no mandatory coin collection.
- Required locomotion and recovery features.
- Stompable enemy and three-hit boss.
- Camera profiles for course, wall section, boss, and clear.
- No long-lived raw component pointers.
- Queued structural changes in scheduled callbacks.
- No direct renderer/RHI/D3D11 dependency.
- Compile checks green and plan progress updated.

## Progress

- [x] Create `game/platformer-tech-demo` from PR #45 HEAD.
- [x] Inspect script lifecycle, safe references, command buffer, character movement, camera, collider, PhysX, collision dispatch, Prefab, Scene YAML, audio, particle, and UI-related component boundaries.
- [ ] Add game project structure and component registration.
- [ ] Complete Phase 1 movement test implementation.
- [ ] Complete Phase 2 triple jump and wall kick.
- [ ] Complete Phase 3 coin, enemy, damage, HUD, and checkpoint systems.
- [ ] Complete Phase 4 full gray-box stage.
- [ ] Complete Phase 5 boss encounter.
- [ ] Complete Phase 6 presentation.
- [ ] Complete Phase 7 stability and compile validation.
