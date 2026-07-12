# MiniGame Collection Implementation Plan

## Branch contract

- Base pull request: `tetoyama/GameEngine#45`
- Base branch: `refactor/ecs-scheduler-foundation`
- Base commit: `355b9ac450d2461bac3fffb49820d73d8b2e8f2e`
- Game branch: `game/minigame-collection`
- Created: 2026-07-12

This branch must remain separable from the ongoing ECS / Scheduler / RenderWorld / Static Batch / RHI migration. Game code must not depend directly on RenderWorld, RenderPacket, RenderPass, StaticBatch, Camera PostEffect GPU runtime, D3D11 resources, RHI internals, or raw PhysX actors.

## Phase 0 findings

### Scene and Multi-Scene

Current `SceneManager` owns an `unordered_map<string, shared_ptr<Scene>>` of active scenes and updates, fixed-updates, draws, and system-processes all active scenes. A scene is removed when `Scene::isDestroy` becomes true. `DeferredLoadScene` exists, but the current public header does not expose a named unload API, a persistent-scene marker, or a transition transaction.

Required game-facing additions:

1. A thin `MiniGameSceneTransition` facade that only uses public `SceneManager` / `Scene` operations.
2. A safe named-scene unload request or a documented helper that sets only the selected mini-game scene to `isDestroy`.
3. Transition ordering that blocks input, finishes presentation, cancels scene-owned effects/audio/tweens, destroys scene-owned entities, then unloads the scene.
4. Persistent services must live in a dedicated scene that is never marked for destruction by mini-game transitions.

### Custom scripts and ECS references

Available:

- `CustomScriptComponent`
- `EntityRef`
- `ComponentRef<T>` with entity and component-generation validation
- `QueueCreateEntity`
- `QueueDestroyEntity`
- `QueueDestroySelf`
- `QueueAddComponent`
- `QueueRemoveComponent`
- `QueueEntitySetup`
- Collision and Trigger callbacks

Rules for this branch:

- Long-lived references use `EntityRef` or `ComponentRef<T>`.
- Pointers returned by `TryGet()` / `GetComponent()` are limited to the current operation.
- Update, FixedUpdate, collision, trigger, and scheduled-task structural changes use Queue APIs.
- Direct `sceneContext->manager/component/system` access is confined to narrow adapter classes where no public facade exists.

### Prefab

`SceneContext` exposes `PrefabSystem* prefab`. Phase 1 must verify the exact public instantiate API and whether prefab spawn can be queued. Runtime entities should be prefab-backed where practical.

### Input

`CustomScriptComponent` currently exposes key polling through `InputService`. Phase 2 must inspect gamepad axis/button APIs and provide one shared movement/action sample structure for human and CPU controllers.

### Collider / Trigger / Physics

`ColliderComponent`, collision callbacks, and trigger callbacks are available. Gameplay results must use deterministic rule logic; physics is limited to contact, trigger, and controlled knockback. No game result may depend on unconstrained rigid-body simulation.

### Audio

Current `AudioComponent::Play` destroys an existing source voice before creating a replacement. One component therefore cannot overlap repeated one-shot sounds. A pooled playback service is required.

Required boundary:

```text
MiniGameAudioService
- Preload(soundId, path)
- PlayOneShot(soundId, volume, pitch)
- StopOwnedByScene(sceneToken)
- StopAll()
- Update(unscaledDeltaTime)
```

Initial implementation may use a fixed voice pool. Game scripts must not create/destroy `AudioComponent` for each sound event.

### Effect

`EffectComponent` supports Play, Stop, Loop, TimeScale, MaxPlayTime, and EffectSystem updates, but refuses `Play` while already playing. Repeated one-shot effects therefore require a pool of effect entities/components.

Required boundary:

```text
MiniGameEffectPool
- Prewarm(effectId, prefab, capacity)
- PlayOneShot(effectId, transform, intensity, sceneToken)
- CancelOwnedByScene(sceneToken)
- Update(unscaledDeltaTime)
```

### Presentation gaps

The following common boundaries are missing or not yet verified:

- One-shot effect pool
- Overlapping audio voice pool
- Pitch control
- UI tween runner
- Camera shake mixer
- Screen flash overlay
- Countdown presenter
- Result presenter
- Scaled vs unscaled time split for hit stop / slow motion
- Scene-token cleanup

No presentation service may access RenderPass, D3D11, SRV/RTV, or RHI internals. Post-effect requests, if needed, go through a narrow game-facing facade.

## Target architecture

```text
Persistent Scene
- MiniGameCollectionManager
- MiniGameSession
- MiniGameSceneTransition
- MiniGamePresentationService
- MiniGameAudioService
- MiniGameInputRouter
- Countdown / Result / HUD presenters
- Persistent camera manager

MiniGame Scene
- IMiniGameRules implementation
- stage entities
- player / CPU spawn points
- game-specific scoring and gimmicks
- game-specific camera settings
```

### Shared state

```cpp
enum class MiniGameState {
    Loading,
    Introduction,
    Countdown,
    Playing,
    Finishing,
    Result,
    Transition
};
```

`MiniGameSession` owns the legal state transitions, active game id, elapsed/remaining time, input lock, scene token, and final result. Individual rules do not perform scene transitions directly.

### Rule boundary

```cpp
class IMiniGameRules {
public:
    virtual ~IMiniGameRules() = default;
    virtual void Prepare() = 0;
    virtual void StartGame() = 0;
    virtual void Tick(float deltaTime) = 0;
    virtual bool IsFinished() const = 0;
    virtual MiniGameResult BuildResult() const = 0;
    virtual void Shutdown() = 0;
};
```

Only behavior genuinely shared by all three games belongs in Core.

## Implementation order

### Phase 1: Presentation Spike

- Create deterministic presentation timeline.
- Implement countdown and GO events.
- Implement scene-owned effect and audio pools.
- Implement UI tween, camera shake, and screen flash adapters.
- Implement success, near-miss, failure, result, retry, and back-to-selection flow.
- Run at least ten retry cycles and assert zero retained scene-token resources.

### Phase 2: Persistent flow

- Persistent scene and manager.
- Mini-game selection.
- Common player input/movement.
- CPU difficulty profile and decision clocks.
- Result and retry flow.
- Explicit transition cleanup.

### Phase 3: Color Territory

- Deterministic tile board and ownership changes.
- Score aggregation and tie handling.
- Shared player movement/contact.
- CPU local candidate evaluation with decision intervals.
- Late-game aggression and leader targeting.
- Endgame presentation.

### Phase 4: Sheep Roundup

- Controlled sheep steering: flee, combined threat, cohesion, wall avoidance, turn smoothing.
- Pen trigger and scoring.
- CPU intercept/guard/steal intentions with hold times.
- Anti-oscillation and wall-stick diagnostics.

### Phase 5: Backshot

- Forward shot with cooldown.
- Dot-product rear-hit resolver with adjustable threshold.
- Debug visualization data for rear cone and shot line.
- Elimination and tie handling.
- CPU threat, cover, wall, target, and cooldown evaluation with reaction delay.

### Phase 6: Presentation polish

- Shared countdown, warnings, finish, winner, result, retry.
- Game-specific score, reversal, hit, capture, and elimination feedback.

### Phase 7: Stability

- Repeated sequence: selection -> game -> result -> retry -> selection -> next game.
- Verify no previous scene timer, CPU, event, audio, effect, UI tween, or entity survives.
- Validate application shutdown.

## Difficulty contract

Difficulty changes information radius, decision interval, candidate quality, prediction quality, and controlled mistake probability. It does not change input latency below humanly readable limits, movement physics, shot cooldown, or hidden information access.

## Risks

1. **Scene unload API gap**: active scenes can be destroyed, but named transition ownership needs a safe facade.
2. **Audio overlap gap**: current component replaces its voice on replay.
3. **Effect overlap gap**: current component rejects replay while active.
4. **Time-domain gap**: presentation and transition must continue during hit stop.
5. **Input API uncertainty**: gamepad axes/buttons must be verified before common player integration.
6. **Prefab queue uncertainty**: runtime prefab spawning must be inspected for scheduler-safe use.
7. **Project integration**: new source files must be included without destabilizing the large `GameEngine.vcxproj`; prefer `Directory.Build.targets` for narrowly scoped additions.
8. **No local Windows build in the current execution environment**: compilation must be validated by the repository Windows workflow after a draft PR is opened.

## Completion gates

- [x] Branch created from PR #45 latest HEAD.
- [x] PR head, branch, and active migration scope recorded.
- [x] Scene, script, reference, command-buffer, audio, and effect boundaries inspected.
- [ ] Presentation Spike complete and retried ten times.
- [ ] Persistent multi-scene flow complete.
- [ ] Color Territory playable with CPU and result.
- [ ] Sheep Roundup playable with CPU and result.
- [ ] Backshot playable with CPU and result.
- [ ] Shared presentation polish complete.
- [ ] Three-game repeated-cycle stability pass complete.
- [ ] Windows compile and smoke tests green.

## Progress log

### 2026-07-12

- Created `game/minigame-collection` from `355b9ac450d2461bac3fffb49820d73d8b2e8f2e`.
- Confirmed PR #45 is in Step 19-A GPU pixel cost / shadow correctness work, with RenderWorld, Static Batch, RenderPacket, RHI, and scheduler migration still active.
- Confirmed game code must remain outside those internals.
- Identified scene-transition, one-shot audio, one-shot effect, UI tween, camera shake, screen flash, and unscaled-time boundaries as the first implementation risks.
- Next: add compile-isolated Core state/result/session contracts and deterministic rule smoke tests before wiring engine-facing presentation adapters.
