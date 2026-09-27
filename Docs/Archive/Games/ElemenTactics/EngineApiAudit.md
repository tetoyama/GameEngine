# ElemenTactics Engine API Audit

Baseline: PR #45 HEAD `3dffea8e7e001413fa3ff71a854a1c54e05ba0f2`.

## Summary

| Area | Existing API | Decision for ElemenTactics | Gap / risk |
|---|---|---|---|
| Scene / Multi-Scene | `SceneManager::AddScene`, `LoadScene`, `DeferredLoadScene`, `LoadFromFilePath`, active-scene map | Use a dedicated game scene; keep initial screen flow inside a game flow controller | Frequent scene replacement can leave async callbacks or subscriptions alive unless cancellation ownership is explicit |
| Input | `InputService` keyboard, mouse position/buttons, wheel, and gamepad | Add a game-local input facade for pointer hit tests and modal blocking | `CustomScriptComponent` wraps keyboard only; mouse access needs the service context or a small public wrapper |
| Runtime sprite | `SpriteRendererComponent`, `TextureComponent`, render layers | Use for cards, icons, HUD panels, and transitions | Layout and interaction need game presenters |
| Runtime text | No general text component in `COMPONENT_LIST` | Add a thin engine-owned runtime text/glyph facade | Blocking gap for Japanese rules, history, result text, and LLM public reasoning |
| Audio | `AudioComponent` + `AudioSystem` | Drive through `ComponentRef<AudioComponent>` and public methods | Game code must never retain or manipulate the internal XAudio2 voice |
| Effect | `EffectComponent` + `EffectSystem` | Trigger through public playback and stop on teardown | Game code must not depend on the internal Effekseer handle |
| Particle | `ParticleComponent` + `ParticleSystem` | Optional lightweight board feedback | Rules must not depend on presentation completion |
| Camera | `CameraComponent` and CameraSystem | Fixed 2.5D board camera, configured outside rules | Do not access Camera PostEffect GPU runtime or preview handles |
| Prefab | `PrefabSystem::InstantiatePrefab` and hierarchy serialization | Use for reusable board cell, piece, card, and presentation entities | Current instantiate path mutates structure immediately; scheduled runtime spawning should use queued setup or pools |
| ECS references | `EntityRef`, `ComponentRef<T>` | Required for all long-lived game-to-engine references | Never cache raw component pointers across frames or structural changes |
| Structural changes | `EntityCommandBuffer`; queue wrappers on `CustomScriptComponent` | Required during Update/Fixed/Draw | Queue setup callbacks are engine-module-only; avoid callbacks crossing Script DLL boundaries |
| Script lifecycle | `OnInitialize`, `OnStart`, `OnUpdate`, `OnStop` | Match controller owns subscriptions/tasks and releases them in `OnStop` | Every event token, effect, audio source, and async request needs idempotent teardown |
| LLM service | `LLAMAService::CreateAgent`, `CreateAgentAsync`, `DestroyAgent` | Access through an ElemenTactics facade | No request-level completion callback/future/timeout API is exposed |
| LLM agent | `RunAsync`, `GetOutput`, `GetState`, `Stop`, `ResetContext` | Poll behind a generation ID; cancel on match exit | Stale output can target a new match unless request ownership is explicit; `Stop` is agent-wide |
| LLM history | Agent owns history/token state; `ResetContext` resets at a worker-safe point | Reset for every new match and after cancellation where the agent is reused | Never access llama context, history, or token members directly |
| RHI / rendering internals | RenderWorld, RenderPacket, RenderPass, RHI layers | No direct dependency from game code | Hard prohibition on D3D11 device/context/SRV/RTV and internal packet/pass ownership |

## Safe-reference contract

`CustomScriptComponent` exposes:

```text
GetEntityRef
GetComponentRef<T>
GetEntityRefFor
GetComponentRefFor<T>
QueueCreateEntity
QueueDestroyEntity
QueueDestroySelf
QueueAddComponent
QueueRemoveComponent
```

These APIs are the allowed runtime ownership boundary. A pointer obtained for a short immediate operation must not be retained.

## Scene strategy

The engine supports multiple active scenes, but ElemenTactics initially uses one dedicated runtime scene plus an internal screen state because:

- title, setup, battle, and result share resources
- retries should not require complete scene reconstruction
- one controller can own the LLM request lifetime
- local-player hidden handoff is naturally modal

If later memory measurements justify partitioning, transitions must cancel LLM work and invalidate pending callbacks before `DeferredLoadScene`.

## Runtime UI gap

The registered component set contains Sprite, Texture, RenderLayer, Audio, Effect, Particle, and Camera components, but no general runtime text component. Existing score display uses digit textures, which is insufficient for dynamic Japanese rules, battle history, result text, and LLM reasoning.

Required thin text API characteristics:

- UTF-8 input
- runtime font or glyph-atlas resource
- anchor, pivot, bounds, alignment, size, and layer
- clipping and wrapping
- no ImGui dependency
- no native D3D11 types exposed to game code
- ordinary ECS teardown

Icon-only placeholders do not satisfy player-facing completion.

## LLM lifecycle contract

```text
Create or acquire agent
ResetContext for new match
Build prompt from PublicGameView and exact legal action list
RunAsync
Poll state/output behind a request generation ID
Timeout, scene exit, or retry => invalidate generation, Stop, then reset or destroy
Parse structured response
Require an exact match in the legal action list
Rules Engine revalidates against the current state
Apply only if still legal; otherwise use heuristic fallback
```

The facade must never serialize the authoritative opponent deck. The only opponent data source is `PublicGameView`.

## Presentation ownership contract

Rules produce immutable action, battle, scout, reorder, and public-event results. Presentation may block input while showing those results, but it must not mutate rule state directly.

Audio and Effect teardown is mandatory on retry, return to title, scene stop, and application shutdown.

## Expected engine-facing additions

1. Runtime text/glyph rendering facade.
2. Optional mouse convenience wrappers or a game-local input facade.
3. ElemenTactics LLM request facade with timeout, cancel, and generation ownership.
4. Project item registration when runtime integration begins.

No modification to RenderWorld, RenderPacket, RenderPass, StaticBatch, camera GPU runtime, RHI internals, or llama.cpp context internals is required or permitted for the game implementation.
