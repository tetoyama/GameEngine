# Project Vision: Extensible Simulation GameEngine Roadmap

## 状態

**ビジョン定義・基盤フェーズ進行中（2026-07-18更新）**

この文書は、GameEngineの長期的な設計目標と、その目標へ向けて何を汎用基盤として実装し、何をゲーム固有モジュールとして分離するかを定義する。

本エンジンの目的は、単一のRobocraft風ゲーム専用エンジンになることではない。

> **汎用性と拡張性を維持しながら、Service・ECS・Adapter・Domain Moduleを適切に組み合わせることで、3Dアクション、シューティング、ボードゲーム、ミニゲーム、ボクセルゲーム、車両戦闘、構造シミュレーションなど、性質の異なる複数のゲームを同じ基盤上で扱えるエンジンを目指す。**

その上で、最も難しく、エンジン全体の設計品質を検証できる統合目標として、以下の体験を置く。

> **Robocraftの「設計した機体を自分で操縦し、戦闘結果から設計を改善する」体験を軸に、Minecraft、Create、Valkyrien Skies 2、Noitaに見られる編集可能世界、機械ネットワーク、可動ローカル世界、物質反応を段階的に統合する。**

オンライン対戦は、この構想を動かす一つの実行形態であり、エンジンやゲームの最終目的そのものではない。シングルプレイ、協力、対戦、サンドボックス、技術デモ、AI実験のいずれにも展開可能な構造を維持する。

関連資料:

- `Docs/ECS_Scheduler_Migration_Plan.md`
- `Docs/GameEngine_Game_Development_Usability_Assessment.md`

---

# 1. 最終ビジョン

## 1.1 プレイヤー体験の中心

最終的に実現したい基本ループは次の通り。

```text
資源・部品・環境を理解する
        ↓
機体・基地・工場・装置を設計する
        ↓
設計した構造物を自分で操作する
        ↓
探索・輸送・製造・戦闘を行う
        ↓
破損・事故・性能不足が構造へ反映される
        ↓
原因を分析して設計を改善する
```

ゲーム側が完成品の乗り物や解法を与えるのではなく、プレイヤーが構造を作り、その構造をSimulationが解釈して機能させる。

## 1.2 参照作品から取り込む核

| 参照軸 | 取り込む体験・技術 | そのまま模倣しない点 |
|---|---|---|
| Robocraft | 機体設計、操縦、部位破壊、性能喪失、再設計 | オンライン対戦だけを最終ゴールにしない |
| Minecraft | 編集可能で永続化されるブロック世界、Chunk、隣接更新、拡張性 | 全要素を1m立方体へ限定しない |
| Create | 接続関係から生まれる動力・加工・物流・論理、視覚的に理解できる機構 | 固定された機械ブロック群だけに限定しない |
| Valkyrien Skies 2 | ブロック集合を可動構造として扱うローカル座標系、船上のEntity、結合と分離 | Minecraftの内部仕様へ依存しない |
| Noita | 物質、熱、液体、火、電気などの局所反応と創発的連鎖 | 全Worldを常時ピクセル単位で更新する前提にしない |

## 1.3 エンジンとしての成功条件

最終ゲームだけが動いても、他ジャンルを扱えなくなった場合は成功とはしない。

成功条件は次の両立である。

1. Robocraft型の複雑な構造Simulationを無理なく実装できる。
2. Platformer、TPS、STG、ボードゲーム、ミニゲームなどが不要なConstruct機能へ依存せず動く。
3. 複数のGame World、Scene、Simulation Profileを同一Runtimeで共存させられる。
4. ゲーム固有機能をAdapterやModuleとして追加してもEngine Coreが汚染されない。
5. 実績の取れたAdapterだけを汎用Serviceへ昇格できる。

---

# 2. 自作エンジンを選ぶ理由と差別化

## 2.1 現時点での明確な強み

- **完全に把握可能なC++コード**
- **ゲーム固有Adapterを自由に追加可能**
- **Engine内部まで制約なく変更可能**
- ECS Storage、System Phase、Scheduler、Render Pipeline、Physics連携をゲーム要件に合わせて変更可能
- 外部EngineのActor、GameObject、Package、Plugin境界へ設計を合わせる必要がない
- 失敗した抽象化を互換性レイヤーごと整理し直せる

この強みは、単にソースコードへアクセスできるという意味ではない。

> **ゲームの中心概念に合わせて、状態の正本、更新順序、データ配置、Physics表現、保存形式、Network同期単位をEngine側から定義できる。**

## 2.2 Unity / Unreal Engineを追わない

UnityやUnreal Engineが持つ一般機能を、数や規模で再現することは目的としない。

優先しない例:

- 映画制作向けSequencer全般
- Hair / Clothの網羅
- 全Platformへの即時対応
- 汎用Visual Scriptingの完全再現
- 巨大なAsset Store相当の機能
- あらゆるジャンル向けの既製Gameplay Framework

代わりに、次の領域では汎用Engineより明快で、速く、壊れにくい構造を目指す。

- プレイヤー生成構造
- 動的な構造分離・結合
- 大量部品の専用Storage
- 可動ローカルWorld
- 構造・電力・機械・流体・熱の統合Simulation
- 部品破壊から機能喪失までの一貫した更新Pipeline
- Construct差分を前提とした保存・Undo・Network同期

## 2.3 現在の完成度の捉え方

2026-07時点の概算:

- 一般的なゲーム制作基盤: **約60〜70%**
- 最終統合目標に対する実装工数ベース: **約20〜25%**
- 中心値: **約22%**

描画、ECS、PhysX、Scene、Prefab、Editor、Audioなどの共通基盤は存在する。一方で、Construct、構造分離、可動ローカルWorld、機械Network、Voxel Chunk、物質Simulation、Replicationなど、最重量の差別化領域はこれからである。

この割合は進捗管理上の目安であり、厳密な工数見積もりではない。

---

# 3. 基本アーキテクチャ方針

## 3.1 小さなCoreと選択可能なModule

すべてのゲームへConstruct、Voxel、Network、Material Simulationを強制しない。

```text
Engine Core
├─ Memory / Job / Time / Logging
├─ ECS / World / Scheduler
├─ Resource / Serialization
├─ Render / Physics / Audio / Input
└─ Service Registry / Module Lifecycle

Optional Shared Services
├─ Character Motor
├─ Navigation
├─ UI / Localization
├─ Feedback
├─ Save / Replay
└─ Network

Domain Modules
├─ Construct
├─ Voxel World
├─ Functional Network
├─ Material Simulation
├─ Vehicle
└─ Encounter / Board Game / Minigameなど

Game Layer
├─ Game-specific Component / System
├─ Adapter
├─ Rule Set
└─ Presentation
```

ゲームは必要なServiceとModuleだけをWorldへ登録する。

## 3.2 Service Scope

Serviceの寿命と所有範囲を明示する。

| Scope | 例 | 原則 |
|---|---|---|
| Engine | Resource、Graphics Device、Job System | Process全体で共有 |
| Session | Network Session、Profile、Replay | Game Session単位 |
| World | Physics Scene、ECS World、Construct Registry | 独立Simulation単位 |
| Scene | Encounter、Level Script、Scene UI | Sceneロードと連動 |
| Entity | Character、Weapon、Part | ECS Component / Systemで管理 |

Global Singletonを増やすのではなく、`EngineContext`、`WorldContext`、Service Registryから明示的に解決する。

## 3.3 複数ゲームを同時に扱う意味

「どんなゲームも同時に扱える」とは、すべてのSystemを常時実行することではない。

- Worldごとに利用するComponent、System、Serviceを選択できる
- Platformer WorldとVoxel Simulation Worldを別Worldとして同時実行できる
- Editor Preview、Player View、Dedicated Simulation、AI Testを独立Worldとして動かせる
- 同一Process内で複数SceneやMinigameを切り替えてもService状態が混線しない
- 共通Systemは再利用し、ゲーム固有RuleはModuleへ隔離する

必要なのは巨大な万能Worldではなく、**構成可能な複数World**である。

## 3.4 Adapterの役割

AdapterはEngineの不足を隠す恒久的な迂回路ではなく、ゲーム側で必要条件を検証するための実験境界とする。

```text
ゲーム側Adapterで仮説検証
        ↓
実機で失敗条件を収集
        ↓
複数ゲームへ共通する契約を抽出
        ↓
Engine Service / Domain Moduleへ昇格
        ↓
旧AdapterをCompatibility Layerへ縮退・削除
```

### Engineへ昇格する条件

- 複数のゲームまたはSceneで同じ要求が発生した
- 状態所有者と更新Phaseを明確に定義できる
- ゲーム固有名称を除去できる
- Serialization、Debug、Testを含む契約を作れる
- 外部Libraryの生ポインタをGameplayへ漏らさず実装できる

### ゲーム側へ残す条件

- ルール固有の勝敗判定
- 作品固有の操作感
- 特定の演出タイミング
- 一つのゲームでのみ必要な特殊データ
- 汎用化すると契約が複雑化するだけの機能

---

# 4. 状態所有権とSimulation Pipeline

## 4.1 正本と派生表現を分ける

将来のConstructでは、部品Entity、PhysX Actor、Render Objectを状態の正本にしない。

```text
Authoritative Domain State
        ↓
Physics Representation
        ↓
Render Representation
        ↓
Audio / VFX / UI / Network Snapshot
```

Constructの場合:

```text
Construct State
├─ Parts
├─ Structural Graph
├─ Damage State
├─ Mass Properties
├─ Functional Networks
└─ Local Coordinate Space

        ↓ 派生

Physics Aggregate / ECS Runtime Entities / Render Batch / Network Snapshot
```

Physicsや描画表現を破棄・再生成しても、正本から復元できることを不変条件とする。

## 4.2 単一書き込み所有者

同じ状態を複数Systemが直接変更しない。

例:

- Player位置と速度はCharacter Motorだけが最終書き込みする
- Constructの部品追加・削除はConstruct Command経由だけで行う
- Physics ActorはPhysics Representation Systemだけが再構築する
- Render BatchはRender Representation Systemだけが更新する
- Network受信はDomain Stateへ直接書かず、検証済Commandへ変換する

ECSのRead / Write Access宣言だけでは、複数Scriptが同じPhysX Actorや外部状態を変更する競合を防げない。外部Library操作も所有Systemへ封じる。

## 4.3 被弾から破壊までの標準Phase例

```text
Input / Network Command
        ↓
Weapon Simulation
        ↓
Hit Collection
        ↓
Damage Apply
        ↓
Part Destruction
        ↓
Structural Graph Rebuild
        ↓
Construct Split / Merge
        ↓
Mass / Inertia Recalculation
        ↓
Functional Network Rebuild
        ↓
Physics Representation Update
        ↓
Render / Feedback / Replication
```

この順序をSystem PhaseとCommand Bufferで保証する。

---

# 5. Constructを第一級概念にする

## 5.1 ConstructはOptional Domain Module

Constructは最終構想の中心だが、すべてのゲームへ強制するEngine Core Objectにはしない。

PlatformerやBoard Gameは通常のECS Worldだけで動作できる。Robocraft型、Vehicle、Ship、Factoryなど必要なゲームだけがConstruct Moduleを登録する。

## 5.2 基本データ案

```cpp
struct Construct {
    ConstructID id;
    ConstructRevision revision;

    LocalCoordinateSpace localSpace;
    PartStorage parts;
    StructuralGraph structuralGraph;
    PhysicsAggregateState physicsState;
    FunctionalNetworkSet networks;
    ConstructDamageState damageState;
};
```

### Construct Stateに含めるもの

- Part Type / ID
- GridまたはSocket上の配置
- Local Transform
- 接続Port
- HP / Damage / Temperature
- 所有者・Team
- Structural Connectivity
- Mass / Center of Mass / Inertia
- Mechanical / Electrical / Fluid / Logic / Thermal Network
- Revision

### 派生表現へ置くもの

- PhysX Shape / Actor
- Render Instance / Mesh Batch
- Particle / Audio
- Editor Selection Handle
- Replication Snapshot

## 5.3 ECSとの関係

1 Part = 1 Entityを固定ルールにしない。

用途に応じて選択する。

- 数十個の高機能Part: ECS Entity化可能
- 数百〜数千個の単純Block: Construct専用SoA Storage
- Weapon、Seat、Engineなど高機能PartだけEntity Proxyを持つ
- 描画はConstruct単位のBatch / Instanceへ集約
- Physicsは初期段階では1 Construct = 1 RigidBodyを基本とする

ECSは万能データ形式ではなく、System間連携とRuntime Object管理の基盤として使う。

---

# 6. 現在ある基盤

## 6.1 Runtime / ECS

- 世代付きEntity
- `ComponentRef<T>` / `EntityRef`
- Dense / Sparse / DirectPaged / Archetype系Storage
- System Phase / Priority
- Read / Write Access宣言
- Command BufferによるStructural Change
- Job System / Parallel Schedule Executor
- `CustomScriptComponent`
- `EngineContext`によるService管理

## 6.2 Rendering

- DirectX 11 / HLSL
- Deferred Rendering
- Shadow / CSM / Point Shadow
- Mesh / Model / Sprite / Billboard / Terrain
- Material / Texture / Normal Map / Environment Map
- Particle / Effect
- Post Effect / Bloom / Blur / BrightPass
- Culling / Static Batching
- ImGui Editor / Player View

## 6.3 Physics / Scene / Asset

- PhysX
- Box / Sphere / Capsule / Mesh Collider
- Static / Dynamic Actor
- Trigger / Collision Layer
- Raycast
- Scene / Prefab / YAML Serialization
- Reflection / Inspector
- Resource管理
- Audio

## 6.4 実制作から確認できた強み

Platformer、Minigame、Board Game、STGなど異なるゲームを、ゲーム固有ComponentとAdapterとして追加できている。

特に有効だった点:

- Game側からRenderWorld、RHI、D3D11 Resourceへ直接依存せず描画できる
- ゲーム固有AdapterでPhysics QueryやFeedbackの仮説検証ができる
- Scene / Component駆動でゲームを構成できる
- Engine内部を必要に応じて修正できる

---

# 7. 現在不足している共通ゲーム制作基盤

最終構想へ進む前に、通常ゲーム制作でも繰り返し問題になる基盤を整える。

## 7.1 Physics Query API

必要:

- Include / Exclude Layerを明示したFilter
- `ignoreEntity` / `ignoreActor`
- Static / Dynamic / Trigger選択
- Raycast / SphereCast / CapsuleCast / Overlap
- Query Debug Draw

現在のMask意味が不明瞭なAPIは、Compatibility Wrapperを残しつつ明示名へ移行する。

## 7.2 Character Motor

必要:

- Capsule Sweep
- Ground Contact
- Step Offset
- Slope Limit
- Ground Snap
- Ledge Detach
- Depenetration
- Moving Platform
- External Impulse
- Teleport

Character MotorだけがPlayer位置と速度の最終書き込みを行う。

## 7.3 Collision Contact

`HitInfo`へ追加:

- Contact Point
- Contact Normal
- Relative Velocity
- Impulse
- Self / Other Collider Handle
- Contact Count

## 7.4 Debug / Editor

- Ray / Shape Cast表示
- Collider / Trigger / Zone Gizmo
- Scene Validation
- Multi Select
- Duplicate
- Align / Distribute
- Grid / Surface Snap
- Prefab Override確認
- Missing Asset検出

## 7.5 UI / Feedback / Time

- Text Renderer / Font Asset
- Layout / Reference Resolution / Safe Area
- Localization / Input Glyph
- Feedback Preset
- Particle Emitter / Pool
- Gameplay / UI / Camera / ParticleのTime Domain分離

---

# 8. 最終構想に必要なDomain Module

## 8.1 Construct Kernel

規模: **XL / 最重要**

- Grid / Socket配置
- Part追加・削除・回転
- Connectivity Graph
- Connected Components
- Split / Merge
- Revision
- Mass / Center of Mass / Inertia
- Blueprint
- Undo / Redo Operation
- Runtime Representation生成

## 8.2 Build Mode

規模: **XL**

- Part Palette
- Ghost Preview
- Placement Validation
- Grid / Socket Snap
- Symmetry / Mirror
- Box Select
- Copy / Paste
- Paint
- Blueprint Library
- 性能・重心・推力中心・Network可視化

Build Mode UXは補助機能ではなく、Robocraft軸の体験そのものである。

## 8.3 Structural Damage

規模: **XL**

- Part HP
- Armor / Damage Type
- Penetration
- Explosion
- Connection Strength
- Construct Split
- Debris Policy
- Mass Property再計算
- Weapon / Movement / Control喪失

## 8.4 Vehicle / Control

規模: **L〜XL**

- Wheel
- Thruster
- Hover
- Leg
- Wing / Control Surface
- Seat / Controller
- Input Mapping
- Stabilization
- Weapon Mount
- Recoil

## 8.5 Dynamic Local World

規模: **XL**

- Construct Local / World変換
- Construct上のEntity
- 動く構造上を歩くCharacter
- Local Velocity継承
- Construct上のRay / Query
- Docking / Undocking
- Joint
- Construct間接触
- Large World Precision

## 8.6 Functional Network

規模: **XL**

```text
Structural Graph
Mechanical Graph
Electrical Graph
Fluid Graph
Logic Graph
Control Graph
Thermal Graph
```

必要:

- Typed Port
- Graph Rebuild
- Dirty Propagation
- Supply / Demand
- RPM / Torque
- Voltage / Current
- Pressure / Flow
- Signal
- Heat / Cooling
- Failure / Overload
- Debug Visualization

## 8.7 Voxel / Chunk World

規模: **XL**

- Chunk Storage
- Block State
- Palette Compression
- Neighbor Update
- Runtime Editing
- Meshing
- Collision Rebuild
- Streaming
- Save Diff
- Lighting
- Replication

Terrain Systemとは別のDomain Moduleとして設計する。

## 8.8 Material Simulation

規模: **XL**

- Solid / Powder / Liquid / Gas
- Temperature / Pressure
- Combustion
- Electricity
- Corrosion
- Phase Change
- Explosion / Chain Reaction
- Active Simulation Region

全Worldを常時高解像度で更新せず、機体内部、破損地点、火災、液体領域など必要な範囲だけをActive Regionとして処理する。

## 8.9 Network / Replication

規模: **XL / 最難関の一つ**

- Authoritative Server
- Entity Replication
- Construct Operation同期
- Revision / Diff同期
- Physics Snapshot
- Prediction / Reconciliation
- Interest Management
- Chunk同期
- Damage同期
- Join-in-progress
- Dedicated Server
- Mod Compatibility

完全決定論だけへ依存せず、Domain Command、Revision、Snapshot、補正を組み合わせる。

---

# 9. フェーズ別ロードマップ

各Phaseは、単独の巨大実装ではなく、必ず小さなVertical Sliceで検証する。

## Phase R0: Foundation Stabilization

目的: あらゆるゲームで必要になる共通基盤を安定させる。

- [~] ECS / Scheduler / Job System移行
- [ ] Component外部状態の所有権規則
- [ ] Physics Query Filter / Shape Cast
- [ ] Collision Contact情報
- [ ] Character Motor
- [ ] Gameplay Debug Draw
- [ ] Scene Validation
- [ ] UI Text / Layout基礎
- [ ] Time Domain / Feedback Service
- [ ] Device Lost / Release Build / Asset Packaging

完了条件:

- Platformer、STG、Minigameで同じPhysics Query / Feedback / UI基盤を再利用できる
- Gameplay ScriptがPhysX ActorやRender内部を無秩序に直接変更しない
- Windows Debug / ReleaseのClean Buildが安定する

## Phase R1: Service / World Composition

目的: 異なるゲームを同一Engineで安全に共存させる。

- [ ] Engine / Session / World / Scene Service Scope
- [ ] WorldごとのService Registry
- [ ] Optional Module Lifecycle
- [ ] System Set / Simulation Profile
- [ ] 複数World同時実行
- [ ] Editor Preview / Player / Automated Test World分離
- [ ] Adapter昇格・廃止規則

完了条件:

- Platformer Worldと別ジャンルのWorldを同一Processで独立実行できる
- 不要なDomain Moduleを登録せずGameを起動できる

## Phase R2: Construct Kernel Prototype

目的: 最終構想の中心データを確立する。

- [ ] Part Definition
- [ ] Grid / Socket
- [ ] Part Storage
- [ ] Structural Graph
- [ ] Connected Components
- [ ] Revision / Command
- [ ] Mass / Center of Mass / Inertia
- [ ] 1 Construct = 1 PhysX RigidBody
- [ ] Render Instance / Batch生成
- [ ] Blueprint Save / Load

最初のVertical Slice:

> 20〜100個のBlockを配置し、1台の剛体として走らせ、Blueprintとして保存・復元する。

## Phase R3: Build Mode

- [ ] 配置 / 削除 / 回転
- [ ] Ghost Preview / Validation
- [ ] Symmetry / Mirror
- [ ] Selection / Copy / Paste
- [ ] Undo / Redo
- [ ] 性能・質量・重心表示
- [ ] Blueprint Library

完了条件:

- CodeやYAMLを手編集せず機体を完成できる
- 不正接続や浮遊Partを配置時に理解できる

## Phase R4: Vehicle / Weapon / Structural Damage

- [ ] Wheel / Thruster / Hoverのうち最低2種類
- [ ] Seat / Input Mapping
- [ ] Weapon / Projectile / Hitscan
- [ ] Part HP / Damage
- [ ] Structural Graph再計算
- [ ] Construct Split
- [ ] 分裂後のPhysics再生成
- [ ] 推進・武器・操縦能力の喪失
- [ ] Feedback / Camera / Audio連携

Vertical Slice:

> 機体を組み、走らせ、撃ち、部品が壊れ、二つのConstructへ分裂し、残存部品に応じて操作性が変わる。

この時点を、Robocraft軸の最初の成立点とする。

## Phase R5: Dynamic Local World / Contraption

- [ ] Construct Local Space
- [ ] Construct上のEntity
- [ ] Moving Construct上のCharacter
- [ ] Local Query
- [ ] Docking / Joint
- [ ] Split / Merge時のEntity移行
- [ ] 回転・往復するContraption

Vertical Slice:

> 移動する船上を歩き、別Constructへ乗り移り、Dockingして一つの構造として動かす。

## Phase R6: Functional Network

- [ ] Typed Port
- [ ] Electrical Graph
- [ ] Mechanical Graph
- [ ] Logic / Control Graph
- [ ] Dirty Rebuild
- [ ] Debug Visualization
- [ ] Overload / Failure

Vertical Slice:

> Engine → Generator → Cable → Motor / Weaponが接続され、途中のPart破壊で機能が停止する。

## Phase R7: Voxel / Chunk World

- [ ] Chunk Storage / Streaming
- [ ] Block Placement / Mining
- [ ] Meshing / Collision
- [ ] Neighbor Update
- [ ] Save Diff
- [ ] Construct化 / World化の境界

Vertical Slice:

> WorldからBlockを採掘し、機体へ搭載し、移動後もWorldとConstructの状態を保存できる。

## Phase R8: Material Simulation

- [ ] Material Cell
- [ ] Active Region
- [ ] Heat / Fire
- [ ] Liquid / Gas
- [ ] Electricity
- [ ] Corrosion / Explosion
- [ ] Construct Damage連携

Vertical Slice:

> 冷却系が破損し、液体漏れ、過熱、引火、電力喪失がSimulationから連鎖する。

## Phase R9: Network / Replication

- [ ] Session / Room
- [ ] Authoritative Command
- [ ] Construct Revision同期
- [ ] Physics Snapshot / Prediction
- [ ] Damage / Split同期
- [ ] Interest Management
- [ ] Chunk / Material Region同期
- [ ] Join-in-progress

オンライン対戦だけでなく、協力、Dedicated Simulation、AI Clientにも利用できるNetwork基盤とする。

## Phase R10: Integrated Game Loop

- [ ] 探索 / 資源
- [ ] 製造 / Factory
- [ ] Build / Repair
- [ ] Vehicle / Ship運用
- [ ] Combat / Encounter
- [ ] Progression
- [ ] Single / Co-op / Versus Rule Set
- [ ] Mod / Content Extension

---

# 10. 設計上の不変条件

今後の実装で守る規則。

1. **Game固有機能を理由なくEngine Coreへ入れない。**
2. **Domain ModuleはOptionalであり、未使用Gameへ依存を強制しない。**
3. **状態の正本を一つにし、PhysicsとRenderを派生表現として扱う。**
4. **外部Libraryの生ポインタ操作を所有Systemへ封じる。**
5. **構造変更はCommand / Revisionを経由する。**
6. **Adapterは検証用境界であり、恒久的な重複実装にしない。**
7. **汎用化は利用実績と失敗条件を得てから行う。**
8. **ECS Entity数を増やすこと自体を設計目的にしない。**
9. **保存、Debug、Editor、TestをRuntime機能の後付けにしない。**
10. **Unity / Unrealの機能数ではなく、このゲーム群に対する適合性で優先順位を決める。**

Construct Module固有の不変条件:

- Construct StateがPart状態の唯一の正本
- Part追加・削除はConstruct Command経由のみ
- Physics Aggregateは再生成可能な派生キャッシュ
- Render Representationは再生成可能な派生キャッシュ
- Split / MergeはStructural Phaseだけで確定
- Functional NetworkはStructural Revisionへ追従
- NetworkはDomain CommandとRevisionを検証して適用

---

# 11. 当面の優先順位

## P0: 一般ゲーム制作基盤

1. Physics Query Filter / Shape Cast
2. Collision Contact情報
3. Character Motorと状態所有権
4. Gameplay Debug Draw / Scene Validation
5. UI Text / Layout
6. Feedback Service / Time Domain
7. Release Build / Packaging

## P1: エンジン構成能力

1. Service Scope
2. World単位Service Registry
3. Optional Domain Module
4. Simulation Profile
5. 複数World実行
6. Adapter昇格ルール

## P2: 最終構想の最初の縦切り

1. Construct Kernel
2. Build Mode
3. Mass Property / Compound Physics
4. Wheel / Thruster / Weapon
5. Part Damage
6. Construct Split
7. Blueprint Save / Load

最初の大目標:

> **20〜100個の部品から機体を組み、走らせ、撃ち、部品が壊れ、二つのConstructへ分裂し、残った構造に応じて性能が変わり、Blueprintとして保存できる。**

ここまで到達した時点で、自作エンジンを選んだ理由が、Engine内部の技術だけでなくプレイヤー体験として現れ始める。

---

# 12. 方針判断の基準

新機能を追加する前に、次を確認する。

1. この機能はEngine Core、Shared Service、Domain Module、Game Adapterのどこに属するか。
2. 正本となる状態は何か。
3. 誰が書き込みを所有するか。
4. どのSystem Phaseで確定するか。
5. 保存・Undo・Network・Debugでどう表現するか。
6. 使わないゲームへ依存を強制しないか。
7. 一般Engineの模倣ではなく、実際のゲーム要件から必要になったか。
8. 小さなVertical Sliceで価値と失敗条件を検証できるか。

この文書は固定された仕様書ではない。各PhaseのPrototypeと実制作から得た失敗条件を反映し、設計判断と優先順位を更新し続ける。