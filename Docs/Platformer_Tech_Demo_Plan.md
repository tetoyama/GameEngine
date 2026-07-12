# Platformer Tech Demo Plan

## Branch basis

- Work branch: `game/platformer-tech-demo`
- Original base: PR #45 HEAD `7ec9cc546e8f817c55e4d4a07c9cc0176751eec8`
- Base branch: `refactor/ecs-scheduler-foundation`
- Latest foundation sync: PR #49 merge commit `48f987ca587d4794d4110a0470777f28cb225e4b`
- Review branch: draft PR #47
- Rule: game code must not access RenderWorld, RenderPacket, RHI, D3D11 resources, render passes, static-batch internals, or culling internals.

## Goal

Deliver one short, linear 3D platformer stage that can be played from start to clear. The critical path is movement, jump feel, triple jump, wall kick, coin guidance, stomp combat, fall recovery, a three-hit mini boss, and a clear result.

## Existing engine boundaries reused

- `CustomScriptComponent`: lifecycle, input helpers, collision and trigger callbacks.
- `ComponentRef<T>` / `EntityRef`: persistent safe references. Raw pointers are used only within the current call.
- `EntityCommandBuffer`: queued entity/component changes plus queued Prefab instantiation during the exclusive Commit playback interval.
- `PhysicSystem`: PhysX rigid bodies, colliders, triggers, layer filtering, raycasts, collision dispatch, and Transform-to-actor upload.
- `TransformComponent`, `CameraComponent`, `ModelRendererComponent`, `MaterialComponent`, `ParticleComponent`, `AudioComponent`, and Prefab/Scene serialization.
- Existing character and camera scripts are reference implementations only. The platformer controller remains game-specific.

## Important API findings

1. `HitInfo` exposes the other `EntityRef` and collision layer, but not contact position or normal.
2. Ground and wall normals therefore come from `PhysicSystem::RaycastWithMask`.
3. `RaycastWithMask` takes an exclusion mask. Player layer 1 and gameplay-trigger layer 3 are excluded with mask `10`.
4. `ColliderComponent::pRigidbodyDynamic` is a non-owning PhysX runtime alias. The platformer controller disables PhysX gravity and owns vertical acceleration, matching the existing `CharacterController` pattern.
5. Structural mutation from Update, FixedUpdate, collision, or trigger callbacks must be queued.
6. Runtime-created game entities use queued Prefab instantiation; the dedicated Scene stores only the core game, player, camera, light, and sky entities.
7. Runtime Prefab scripts initialize safely: FixedUpdate may initialize their object state first, then the next frame performs `Start()` and `OnStart()` once.

## New game classes

- `PlatformerSceneAccess`: centralized scene/service lookup so registry access is not scattered.
- `PlatformerMovementSettings`: controller tuning data.
- `PlatformerCharacterController`: camera-relative locomotion, variable jump, coyote time, jump buffer, asymmetric gravity, apex adjustment, ground snap, slope projection, triple jump, wall kick, stomp bounce, damage, knockback, invulnerability, control lock, checkpoint recovery, and clear state.
- `PlatformerCameraController`: constrained follow camera, zone framing, collision shortening, boss framing, and clear framing.
- `PlatformerAnimationController`: animation-state selection with transform fallback when clips are unavailable.
- `PlatformerGameManager`: run state, coins, boss state, clear state, result lock, and scene restart.
- `PlatformerCoin`: spin/bob, trigger collection, duplicate prevention, feedback, and delayed queued destruction.
- `PlatformerCheckpoint`: trigger activation and player respawn target update.
- `PlatformerEnemy`: simple patrol, stomp defeat, contact damage, duplicate defeat prevention.
- `PlatformerMovingPlatform`: deterministic ping-pong platform motion and rider transport.
- `PlatformerBoss`: telegraph, chase, charge, wall impact, stunned weak point, stomp damage, phase escalation, defeat, and clear request.
- `PlatformerCameraZone`: trigger-driven camera profile changes.
- `PlatformerHud`: coin count, health, boss health, controls, clear result, and restart prompt.
- `PlatformerStageBuilder`: queued Prefab construction of the complete course.
- `PlatformerPlayerFeedback` / `PlatformerClearFeedback`: event-revision-driven particles and audio.
- `PlatformerSoundLibrary`: deterministic generation of small PCM WAV effects when dedicated sound files are absent.

## Engine changes

- Register game components in `componentList.h`.
- Add queued `EntityCommandBuffer::InstantiatePrefab` as the missing safe creation boundary.
- Change the game branch's `DEFAULT_SCENE` to the dedicated platformer Scene.
- Do not modify renderer, RenderWorld, Static Batch, Culling, RHI, D3D11, or RenderPass internals for game behavior.

## Stage plan implemented

1. Start/tutorial platform and low-risk jumps.
2. Long runway and three-height platforms for triple-jump teaching; lower recovery route remains usable.
3. Safe shelf, alternating-wall shaft, short exposed wall-kick exit, and lower recovery floor.
4. Mid-course checkpoint, tilted vertically moving platform, and one stompable patrol enemy.
5. Boss-front checkpoint and open-entry charge arena with side walls, far impact wall, and optional wall-jump aids.
6. Boss defeat presentation, clear result, and `R` restart.

Target coin count: 30. Coins are guidance and feedback, never a progression lock.

## Presentation implemented

- Existing animated player model with Idle, Run, Jump Up, and Fall clips.
- Transform squash/stretch, triple-jump spin, wall-kick spin, damage response, and landing pulse fallbacks.
- Course-section materials, directional light/shadow, sky environment, Bloom post effect, particles, BGM, generated action effects, boss material telegraph, boss defeat collapse, HUD, boss bar, clear jingle, and result screen.
- Static course blocks carry `StaticEntityComponent`, while rendering/culling remain fully owned by the Engine.

## Risks and validation limits

- No contact normal in collision callbacks: wall contact uses short multi-direction ray probes with grace time.
- Scene and Prefab YAML are hand-authored and require a real Editor/runtime load to validate visual scale and authored balance.
- The connected GitHub environment cannot launch the DirectX application or perform a human start-to-clear playthrough.
- Windows Debug x64 compilation is delegated to `.github/workflows/platformer-tech-demo-build.yml`.
- Runtime-generated WAV files require the project Asset directory to be writable. Failure to generate sound does not block gameplay.

## Completion gates

- [x] Dedicated branch based on PR #45 and later synchronized with its latest working branch.
- [x] Dedicated game code and asset directories.
- [x] Start-to-clear code path with no mandatory coin collection.
- [x] Required locomotion and recovery features.
- [x] Stompable enemy and three-hit boss.
- [x] Camera profiles for course, triple-jump, wall section, boss, and clear.
- [x] No long-lived raw component pointers in game scripts.
- [x] Queued structural changes in scheduled callbacks.
- [x] No direct renderer/RHI/D3D11 dependency.
- [ ] Windows Debug x64 compile check green.
- [ ] Real runtime start-to-clear playthrough verified.

## Progress

- [x] Phase 0: inspect current Engine APIs and add this plan.
- [x] Phase 1: ground/air movement, variable jump, coyote time, jump buffer, slope handling, fall recovery, and constrained camera.
- [x] Phase 2: triple jump, wall probes, wall kick, animation state, and action feedback.
- [x] Phase 3: coin, HUD, particles, audio, enemy stomp, damage, knockback, invulnerability, and checkpoints.
- [x] Phase 4: dedicated full gray-box course with recovery routes and 30 guidance coins.
- [x] Phase 5: three-hit boss state machine, encounter framing, defeat, and clear request.
- [x] Phase 6: models, materials, lighting, shadow, particles, BGM/SFX, post effect, camera presentation, boss presentation, result, and restart.
- [ ] Phase 7: Windows compile result, runtime playthrough, tuning, and final defect stabilization.

## Current next action

1. Resolve any Windows CI compile errors without weakening Engine boundaries.
2. Load `Asset/Game/Platformer/Scene/PlatformerTechDemo.scene` in the Editor and perform a full start-to-clear run.
3. Tune movement, camera distances, platform dimensions, boss timing, and sound levels from observed runtime behavior.
