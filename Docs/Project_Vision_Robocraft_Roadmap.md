# Project Vision: Robocraft-Centered Composable Multi-World GameEngine Roadmap

## 状態

**ビジョン定義・基盤フェーズ進行中（2026-07-18更新）**

この文書は、GameEngineの長期目標、アーキテクチャ境界、実装順序、各フェーズの完了条件を定義する。

本エンジンは、単一作品専用のコードベースにも、UnityやUnreal Engineの機能数を追う巨大な万能エンジンにもならない。

> **汎用性と拡張性を維持しながら、WorldごとにService、ECS Component / System、Domain Module、Adapterを構成し、3Dアクション、STG、ボードゲーム、ミニゲーム、ボクセル世界、車両戦闘、構造シミュレーションなど、性質の異なるゲームを同じRuntime上で安全に扱えるエンジンを目指す。**

その設計品質を最も厳しく検証する統合目標として、次の体験を置く。

> **Robocraftの「設計した機体を自分で操縦し、破損や性能不足の原因を理解して設計を改善する」体験を中心に、Minecraft、Create、Valkyrien Skies 2、Noitaに見られる編集可能世界、機械ネットワーク、可動ローカル世界、物質反応を段階的に統合する。**

Robocraft型ゲームは唯一の用途ではない。最も複雑なVertical Sliceとして、Core、Service、ECS、Physics、保存、Editor、Networkの設計を検証する役割を持つ。

関連資料:

- `Docs/ECS_Scheduler_Migration_Plan.md`
- `Docs/GameEngine_Game_Development_Usability_Assessment.md`

---

# 0. エグゼクティブサマリー

## 0.1 最終的なプレイヤー体験

```text
設計する
    ↓
自分で操作する
    ↓
探索・輸送・製造・戦闘で試す
    ↓
破損・機能停止・性能不足が構造へ現れる
    ↓
原因を診断する
    ↓
設計を改善する
```

参照作品の役割は同列ではない。

| 参照軸 | 本構想での役割 |
|---|---|
| Robocraft | プレイヤー体験、ゲームループ、設計と戦闘の因果関係を定義する中心軸 |
| Valkyrien Skies 2 | ブロック集合を独立した可動構造として扱う空間・物理基盤 |
| Minecraft | 編集可能で永続化される大規模なブロック世界と拡張性 |
| Create | 接続関係から機械、物流、加工、制御が生まれる機能ネットワーク |
| Noita | 熱、火、液体、電気などが局所反応し、予測外の連鎖を生む物質シミュレーション |

Robocraftが中心体験を決め、他の要素はその体験を拡張する。

## 0.2 汎用性は「構成可能性」で得る

「どんなゲームも扱える」とは、すべての機能をCoreへ入れ、すべてのSystemを常時実行することではない。

```text
小さなEngine Core
      +
WorldごとのShared Service
      +
必要なECS System Set
      +
選択可能なDomain Module
      +
Game固有Rule / Adapter / Presentation
```

必要なのは一つの巨大な万能Worldではなく、**構成可能で相互に隔離された複数World**である。

## 0.3 最初の大目標

> **20〜100個の部品から機体を組み、走らせ、撃ち、部品が壊れ、複数のConstructへ分裂し、残った構造に応じて操作性と戦闘能力が変わり、Blueprintとして保存・復元できる。**

このVertical Sliceが完成するまでは、大規模Voxel World、全面的な物質シミュレーション、オンライン対戦を主開発目標にしない。

---

# 1. エンジンとしての理想像

## 1.1 同じCoreから異なるゲームを構成する

- Platformer: Character Motor、Camera、Animation、Checkpoint
- STG: Projectile、Encounter、Enemy Spawn、Deterministic Seed
- Board Game: Turn State、Rule Evaluation、Hidden Information、Replay
- Minigame: Session、Countdown、Result、Presentation
- Construct Sandbox: Part Storage、Physics Aggregate、Build Mode
- Voxel World: Chunk、Streaming、Block Update、Save Diff
- Factory: Functional Network、物流、加工、制御
- Material Simulation: Active Region、熱、火、液体、電気

これらを一つの継承階層や巨大な`GameManager`へ押し込まない。

## 1.2 「同時に扱える」の定義

- 同一Process内に複数のECS Worldを生成できる
- Worldごとに異なるServiceとSystem Setを登録できる
- Main Game、Build Preview、Test Drive、AI Simulationを別Worldで動かせる
- Dedicated SimulationはRender Serviceなしで動作できる
- UI PreviewはGameplay Physicsなしで動作できる
- Minigameを独立Sessionとして起動・破棄できる
- World間で共有するものと隔離するものを明示できる

World同士がComponent、Service内部状態、PhysX Actorの生ポインタを直接共有してはならない。連携にはCommand、Message、Snapshot、Stable ID、明示的なBridge Serviceを使う。

## 1.3 Coreへ入れるのは「機能」より「追加能力」

Construct、Voxel、Material Simulation、Board Game Rule、Platformer Movement RuleをCoreへ無条件に入れない。

Coreは、Moduleを安全に登録、実行、破棄、検証する能力を持つ。

---

# 2. エンジンとしての成功条件

1. Robocraft型の構造Simulationを無理なく実装できる。
2. Platformer、TPS、STG、Board Game、MinigameがConstruct Moduleなしで動作できる。
3. Worldごとに必要なService、ECS System Set、Domain Moduleだけを選択できる。
4. 同一Process内で異なるSimulation ProfileのWorldを同時実行できる。
5. ゲーム固有Adapterを追加してもEngine Coreを汚染しない。
6. 実績の取れたAdapterだけをShared Serviceへ昇格できる。
7. Authoritative State、Physics、Render、Editor、Save、Networkの責務が混線しない。
8. 大量Partを`1 Part = 1 Entity`へ固定せず、用途に応じたStorageで扱える。
9. Moduleを使用しないGameへ不要な初期化、Memory、依存を強制しない。

自作エンジンの価値は、内部コードへアクセスできることではない。

> **ゲームの中心概念と複数ゲームの構成方式を、Engine Architectureの中心へ置けることにある。**

Unity / Unreal Engineを機能数で追わず、次を優先する。

- World / Service / Moduleの構成能力
- プレイヤー生成構造
- 動的な分離と結合
- 大量部品向けStorage
- 可動ローカルWorld
- 構造、機械、電力、流体、熱の連携
- Construct差分を前提とした保存、Undo、Replay、Network同期
- Simulationの原因を理解できるEditorとDebug表示

---

# 3. アーキテクチャ境界

## 3.1 小さなCoreと選択可能なModule

```text
Engine Core
├─ Memory / Job / Time / Logging
├─ ECS / World / Scheduler
├─ Resource / Serialization
├─ Render / Physics / Audio / Input
└─ Service Registry / Module Lifecycle

Optional Shared Services
├─ Character Motor
├─ Physics Query
├─ Navigation
├─ UI / Localization
├─ Feedback
├─ Save / Replay
└─ Network

Domain Modules
├─ Construct
├─ Vehicle
├─ Dynamic Local World
├─ Functional Network
├─ Voxel World
├─ Material Simulation
└─ Encounter / Board Game / Minigameなど

Game Layer
├─ Game-specific Component / System
├─ Adapter
├─ Rule Set
└─ Presentation
```

## 3.2 ECS、Service、Domain Module、Adapterの役割

すべてをECSへ入れず、すべてをServiceにも入れない。

| 境界 | 適するもの | 適さないもの |
|---|---|---|
| ECS Component | Entityごとの小さな状態、並列反復したいデータ | Process全体の外部Resource、巨大Graphの正本 |
| ECS System | Component集合に対するPhase付き処理 | 所有範囲不明のGlobal処理 |
| Shared Service | World / Session共有状態、外部Library窓口、明確なLifecycleを持つ機能 | Entityごとの大量データ、Game固有Rule |
| Domain Module | 独立した正本データとRuleを持つOptionalな機能領域 | 全Game必須の低レベル基盤 |
| Game Adapter | 実制作で契約を検証する薄い統合層 | 長期的な重複実装 |
| Presentation | Audio、VFX、Camera、UI | Gameplayの正本状態 |

ECSへ置く条件:

- Entity単位で存在する
- Storageと並列反復の恩恵がある
- System Phaseで更新できる
- Component単位のSerialization契約を作れる

Serviceへ置く条件:

- 複数Entity / Systemが共有する
- Engine / Session / World / Scene Scopeがある
- 外部LibraryやOS Resourceの所有者になる
- CommandやQuery APIとして公開できる

Domain Moduleへ置く条件:

- 独自の正本Storage、Command、Revision、Validationを持つ
- 未使用Gameでは登録しなくてよい
- ECS EntityをRuntime Proxyとして利用できる

## 3.3 Service Scope

| Scope | 例 | 原則 |
|---|---|---|
| Engine | Resource、Graphics Device、Job System | Process全体で共有 |
| Session | Network Session、Profile、Replay | Game Session単位 |
| World | Physics Scene、ECS World、Construct Registry | 独立Simulation単位 |
| Scene | Encounter、Level Script、Scene UI | Sceneロードと連動 |
| Entity | Character、Weapon、高機能Part | ECS Component / Systemで管理 |

Global Singletonを増やさず、`EngineContext`、`SessionContext`、`WorldContext`、Service Registryから明示的に解決する。

## 3.4 World Composition Manifest

Worldの構成を暗黙的な`Get<T>()`呼び出しだけで決めない。

```cpp
struct WorldComposition {
    WorldProfileID profile;
    std::vector<ServiceTypeID> services;
    std::vector<SystemSetID> systemSets;
    std::vector<DomainModuleID> modules;
    std::vector<AdapterID> adapters;
    WorldCapabilitySet requiredCapabilities;
    WorldCapabilitySet providedCapabilities;
};
```

例:

```text
PlatformerWorld
├─ Physics World Service
├─ Character Motor Service
├─ Animation / Camera System Set
├─ UI / Feedback Service
└─ Platformer Rule Adapter

ConstructSandboxWorld
├─ Physics World Service
├─ Construct / Vehicle / Build Mode Module
├─ Construct Debug Service
└─ Sandbox Rule Adapter

DedicatedSimulationWorld
├─ Physics World Service
├─ Construct / Damage Module
├─ Network Replication Service
└─ Render / Editorなし
```

ManifestはDependency解決、起動時Validation、Debug表示、Automated Testに利用する。

## 3.5 Module Lifecycle

```text
DescribeCapabilities
        ↓
ValidateDependencies
        ↓
RegisterComponents / Storage
        ↓
RegisterServices / Systems
        ↓
InitializeWorldState
        ↓
Start
        ↓
Stop
        ↓
Unregister / Destroy
```

規則:

- Schedule開始後にComponent Typeを初回登録しない
- Module依存は宣言し、暗黙的Global探索に依存しない
- 循環依存を許可しない
- Optional依存はCapability Queryで確認する
- Module停止後にCallback、ComponentRef、外部Handleを残さない
- Serialization VersionとMigration方針をModule単位で持つ

## 3.6 World間Bridge

World間で共有Mutable Stateを直接参照しない。

- Immutable Snapshot
- Validated Command
- Event / Message Queue
- Stable Resource Handle
- Save / Blueprint Asset
- Explicit Bridge Service

Build WorldからTest Drive WorldへBlueprintを渡す場合も、ECS StorageやPhysX Actorを共有せず、BlueprintまたはSnapshotを渡す。

---

# 4. Adapterの役割とPlatformerから得た教訓

```text
Game Adapterで仮説検証
        ↓
実機で失敗条件を収集
        ↓
共通契約を抽出
        ↓
Shared Service / Domain Moduleへ昇格
        ↓
旧AdapterをCompatibility Layerへ縮退・削除
```

Engineへ昇格する条件:

- 複数GameまたはSceneで同じ要求が発生した
- 状態所有者と更新Phaseを定義できる
- ゲーム固有名称を除去できる
- Serialization、Debug、Testを含む契約を作れる
- World ScopeとLifecycleを定義できる
- 未使用Gameへの依存を発生させない

Platformer実装では次の問題が発生した。

- Physics Query Filterが弱く、自己Collider、Trigger、CameraZone、Environmentを誤検出した
- 複数ScriptがPlayer TransformとPhysX Actorへ書き、坂、落下、Step Assistが相互に回帰した
- Step AssistをPresentation Componentへ置き、Character Controllerの確定結果を後から上書きした
- Collider原点と見た目原点の差によりStomp判定が不安定になった
- Debug Draw不足によりRay、Normal、Trigger、Zoneの誤判定を発見しにくかった

この実績から、次をShared Serviceへ昇格する。

- Physics Query Service
- Character Motor
- Collision Contact
- Gameplay Debug Draw
- Feedback / Time Domain Service

Boss Arena Boundaryや作品固有のTriple Jump条件はGame Ruleへ残す。

---

# 5. 状態所有権とSimulation Pipeline

## 5.1 正本と派生表現

```text
Authoritative Domain State
        ↓
Physics Representation
        ↓
Render Representation
        ↓
Audio / VFX / UI / Network Snapshot
```

PhysicsやRender表現を破棄しても、正本から再生成できることを不変条件とする。

## 5.2 単一書き込み所有者

- Player位置と速度はCharacter Motorだけが最終書き込みする
- ConstructのPart追加・削除はConstruct Command経由だけで行う
- Structural GraphはStructural Systemだけが確定する
- Physics ActorはPhysics Representation Systemだけが再構築する
- Render BatchはRender Representation Systemだけが更新する
- Network受信データは検証済Domain Commandへ変換する

所有者以外は直接書かず、Requestを送る。

```cpp
motor.AddImpulse(...);
motor.Teleport(...);
motor.SetMovementConstraint(...);
construct.EnqueueOperation(...);
feedback.Play(...);
worldBridge.Send(...);
```

## 5.3 標準破壊Pipeline

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

# 6. Constructを第一級概念にする

Constructは最終構想の中心だが、全Gameへ強制するCore Objectにはしない。

```cpp
struct Construct {
    ConstructID id;
    ConstructRevision revision;
    LocalCoordinateSpace localSpace;
    PartStorage parts;
    StructuralGraph structuralGraph;
    MassProperties massProperties;
    FunctionalNetworkSet networks;
    ConstructDamageState damageState;
};
```

Construct Stateへ置くもの:

- Part Type / Stable ID
- Grid / Socket配置
- Local Transform
- 接続Port
- HP / Damage / Temperature
- Structural Connectivity
- Mass / Center of Mass / Inertia
- Functional Network
- Revision

派生表現へ置くもの:

- PhysX Shape / Actor
- Render Instance / Mesh Batch
- Particle / Audio
- Editor Handle
- Replication Snapshot

`1 Part = 1 Entity`を固定ルールにしない。

- 高機能PartはECS Entity化できる
- 数百〜数千の単純BlockはConstruct専用SoA Storageへ格納する
- ECSはSystem間連携とRuntime Proxy管理に利用する
- 初期Physicsは`1 Construct = 1 RigidBody`を基本とする

Structural GraphとMechanical / Electrical / Fluid / Logic / Control / Thermal Graphを分離する。

---

# 7. 現在ある基盤と不足

## 7.1 既存基盤

- 世代付きEntity、`ComponentRef<T>` / `EntityRef`
- Dense / Sparse / DirectPaged / Archetype系Storage
- System Phase、Read / Write Access、Command Buffer
- Job System / Parallel Scheduler
- DirectX 11 / HLSL / Deferred Rendering
- Shadow、Material、Particle、Post Effect、Culling、Static Batch
- PhysX、Collider、Trigger、Raycast
- Scene、Prefab、YAML、Reflection、Inspector
- Resource、Audio、ImGui Editor / Player View

## 7.2 実制作で確認できた強み

- Platformer、STG、Board Game、MinigameをGame固有Component / Adapterとして追加できる
- Game側からRenderWorld、RHI、D3D11 Resourceへ直接依存せず描画できる
- Game AdapterでPhysics QueryやFeedbackの仮説検証ができる
- Engine内部を必要に応じて修正できる

## 7.3 先に整えるShared Service

1. Physics Query Filter / Shape Cast
2. Collision Contact情報
3. Character Motor
4. Gameplay Debug Draw / Scene Validation
5. UI Text / Layout / Localization
6. Feedback Service / Time Domain
7. Release Build / Asset Packaging

これらはConstruct開発の寄り道ではなく、安全なDynamic Local WorldとDamage Simulationの前提である。

---

# 8. 最終構想のDomain Module

## Construct Kernel

- Part Definition / Storage
- Grid / Socket
- Structural Graph / Connected Components
- Split / Merge
- Revision / Command
- Mass Property
- Blueprint / Undo / Redo
- Runtime Representation生成

## Build Mode

- Placement / Validation / Snap
- Symmetry / Selection / Copy / Paste
- Paint / Undo / Redo
- Blueprint Library
- 重量、重心、推力中心、Network可視化
- Test Driveへの即時遷移

## Vehicle / Control

- Wheel / Thruster / Hover / Wing
- Seat / Input Mapping / Stabilization
- Weapon Mount / Recoil / Control Authority

## Structural Damage

- Part HP / Armor / Penetration / Explosion
- Connection Strength / Part Detach
- Construct Split / Debris
- Mass Property再計算
- Weapon / Movement / Control喪失
- Damage原因表示

## Dynamic Local World

- Construct Local / World変換
- Construct上のEntityとCharacter
- Local Velocity継承
- Local Query
- Docking / Joint
- Split / Merge時のEntity移行

## Functional Network

- Typed Port
- Mechanical / Electrical / Fluid / Logic / Control / Thermal Graph
- Dirty Rebuild
- Supply / Demand / Overload
- Debug Visualization

## Voxel / Chunk World

- Chunk Storage / Streaming
- Block State / Neighbor Update
- Runtime Editing / Meshing / Collision
- Save Diff / Replication
- World BlockとConstruct Partの変換境界

## Material Simulation

- Solid / Powder / Liquid / Gas
- Temperature / Pressure
- Combustion / Electricity / Corrosion
- Active Simulation Region
- Construct Damage連携

## Save / Replay / Network

- Blueprint / Operation Log / Revision Snapshot
- Damage Timeline / Replay / Runtime Audit
- Authoritative Command / Snapshot / Prediction
- Interest Management / Join-in-progress

---

# 9. フェーズ別ロードマップ

## Phase R0: Foundation Stabilization

- [~] ECS / Scheduler / Job System移行
- [ ] 外部状態の所有権規則
- [ ] Physics Query Filter / Shape Cast
- [ ] Collision Contact情報
- [ ] Character Motor
- [ ] Gameplay Debug Draw / Scene Validation
- [ ] UI Text / Layout
- [ ] Feedback Service / Time Domain
- [ ] Release Build / Asset Packaging

完了条件:

- Platformer、STG、Minigameで同じPhysics Query、Feedback、UI基盤を再利用できる
- Gameplay ScriptがPhysX ActorやRender内部を無秩序に直接変更しない
- Windows Debug / ReleaseのClean Buildが安定する

## Phase R1: Service / World Composition

- [ ] `WorldContext`とWorld単位Service Registry
- [ ] World Composition Manifest
- [ ] Capability宣言
- [ ] Optional Module Lifecycle
- [ ] Dependency Validation / 循環依存検出
- [ ] System Set / Simulation Profile
- [ ] 複数World同時実行
- [ ] Explicit World Bridge
- [ ] Preview / Player / Automated Test World分離
- [ ] Adapter昇格・廃止規則

完了条件:

- Platformer Worldと別ジャンルのWorldを同一Processで独立実行できる
- Main、Preview、Test Worldが状態を混線させない
- 不要なModuleを登録せずGameを起動できる
- World破棄後にService、Callback、外部Handleが残らない

Vertical Slice:

> Platformer World、Construct Preview World、AI Test Worldを同一Processで生成し、異なるService / System Setで動かし、SnapshotだけをBridgeで交換する。

## Phase R2: Construct Kernel Prototype

- [ ] Part Definition / Storage
- [ ] Grid / Socket
- [ ] Structural Graph / Connected Components
- [ ] Revision / Command
- [ ] Mass / Center of Mass / Inertia
- [ ] `1 Construct = 1 PhysX RigidBody`
- [ ] Render Batch生成
- [ ] Blueprint Save / Load

Vertical Slice:

> 20〜100個のBlockを配置し、1台の剛体として走らせ、Blueprintとして保存・復元する。

## Phase R3: Build Mode

- [ ] 配置 / 削除 / 回転
- [ ] Ghost Preview / Validation
- [ ] Symmetry / Selection / Copy / Paste
- [ ] Undo / Redo
- [ ] 性能、質量、重心表示
- [ ] Blueprint Library
- [ ] Test Driveへの即時遷移

## Phase R4: Small Robocraft Vertical Slice

- [ ] 移動方式を最低2種類
- [ ] Seat / Input Mapping
- [ ] Weapon
- [ ] Part Damage
- [ ] Structural Graph再計算
- [ ] Construct Split
- [ ] 分裂後のPhysics再生成
- [ ] 機能喪失
- [ ] Damage原因表示
- [ ] Buildへ戻る導線

Vertical Slice:

> 機体を組み、走らせ、撃ち、部品が壊れ、複数Constructへ分裂し、残存部品に応じて操作性が変わり、原因を確認して設計を改善する。

## Phase R5以降

- R5: Dynamic Local World / Contraption
- R6: Functional Network
- R7: Voxel / Chunk World
- R8: Material Simulation
- R9: Network / Replication
- R10: Integrated Game Loop

各Phaseは必ず遊べるVertical Sliceを持つ。

---

# 10. フェーズゲート

## Gate 0: 複数Gameを構成できる

- World Compositionを明示できる
- 不要なService / Moduleを外して起動できる
- 複数Worldで状態が混線しない
- Module LifecycleとDependencyをValidationできる

## Gate A: Constructを保存できる

- 20〜100 Partを配置できる
- Stable IDとRevisionを持つ
- Blueprintへ保存・復元できる
- PhysicsとRenderを正本から再生成できる

## Gate B: コードなしで設計できる

- Build Modeだけで機体を完成できる
- 配置不可能な理由を理解できる
- 重量と重心を確認できる
- Undo / Redoが成立する

## Gate C: 設計した機体を操作できる

- 複数の移動方式を組み合わせられる
- 部品配置が操作性へ反映される
- Weapon配置と射界が結果へ反映される

## Gate D: 破壊で挙動が変わる

- Partが個別に壊れる
- 接続切断でConstructが分裂する
- 残存部品に応じて能力が変化する
- Damage原因を確認できる

Gate Dまで通過して初めて、Robocraft軸の中心ループが成立したと判断する。

---

# 11. 設計上の不変条件

1. **汎用性は巨大な万能Coreではなく、Service、ECS System Set、Domain Moduleの構成可能性で得る。**
2. **Robocraft型の設計・操作・破壊・再設計ループを最上位の統合判断基準にする。**
3. **Robocraft固有機能を他Gameへ強制されるEngine Coreへ入れない。**
4. **Domain ModuleはOptionalとする。**
5. **WorldごとにService ScopeとSimulation Profileを明示する。**
6. **状態の正本を一つにし、PhysicsとRenderを派生表現として扱う。**
7. **外部Library操作を所有Service / Systemへ封じる。**
8. **構造変更はCommand / Revisionを経由する。**
9. **Adapterは検証用境界とし、恒久的な重複実装にしない。**
10. **汎用化は利用実績と失敗条件を得てから行う。**
11. **ECS化、Service化自体を設計目的にしない。**
12. **保存、Debug、Editor、Testを後付けにしない。**
13. **World間で共有Mutable Stateや外部Handleを直接共有しない。**
14. **Online、Voxel、Material Simulationを中心ループ成立前の逃げ道にしない。**
15. **Unity / Unrealの機能数ではなく、このゲーム群への適合性で優先順位を決める。**

---

# 12. 当面の優先順位

## P0: 一般ゲーム制作基盤

1. Physics Query Filter / Shape Cast
2. Collision Contact情報
3. Character Motorと外部状態の所有権
4. Gameplay Debug Draw / Scene Validation
5. UI Text / Layout
6. Feedback Service / Time Domain
7. Release Build / Packaging

## P1: Engine構成能力

1. `WorldContext`とWorld単位Service Registry
2. World Composition Manifest
3. Capability宣言
4. Optional Module Lifecycle
5. Simulation Profile
6. 複数World実行
7. Explicit World Bridge
8. Adapter昇格ルール

## P2: Construct縦切り

1. Part Definition / Storage
2. Grid / Socket
3. Structural Graph
4. Revision / Command
5. Mass Property / Compound Physics
6. Blueprint Save / Load
7. 最小Build Mode

## P3: Robocraft中心ループ

1. Wheel / Thruster / Hover
2. Seat / Control
3. Weapon
4. Part Damage
5. Construct Split
6. 機能喪失
7. Damage診断
8. Buildへ戻る反復導線

現在の最重要マイルストーン:

> **R0で通常ゲーム制作基盤を安定させ、R1で複数Worldの構成能力を確立した上で、Construct Kernelと最小Build Modeを作り、20〜100 Partの機体を保存・操作・破壊できるSmall Robocraft Vertical Sliceへ到達する。**

---

# 13. 方針判断と引き継ぎ

新機能を追加する前に確認する。

1. 複数Gameの構成能力またはRobocraft中心ループをどう強化するか。
2. Core、Shared Service、ECS、Domain Module、Game Adapterのどこに属するか。
3. ECS / Serviceへ置く理由は何か。
4. 正本と書き込み所有者は何か。
5. World ScopeとSystem Phaseは何か。
6. Required / Provided Capabilityは何か。
7. 保存、Undo、Replay、Network、Debugでどう表現するか。
8. 未使用Gameへ依存を強制しないか。
9. Vertical Sliceで価値と失敗条件を検証できるか。
10. 複数World実行時に状態が混線しないか。

他担当やAIエージェントは次を前提とする。

- 最終目的を「Robocraft風オンライン対戦ゲーム」と短縮しない
- 最終目的を「何でもCoreへ入れた万能エンジン」と解釈しない
- 汎用性はWorld CompositionとOptional Moduleで実現する
- Minecraft、Create、VS2、Noitaの機能を同時に実装し始めない
- Game Adapterで失敗条件を取得してから汎用化する
- Physics ActorやRender Objectへの直接書き込みを増やさない
- 新ModuleにはScope、Lifecycle、Dependency、Serialization、Debug、Testを設計する
- 各作業は対象Phase / Gateと次のPlayable Verificationを明記する

---

# 14. 進捗評価指標

## Game Composition品質

- 新GameをCore変更なしで構成できるか
- 未使用Moduleを外して起動できるか
- World起動時に依存不足を検出できるか
- 複数Worldで状態が混線しないか
- World破棄後にService、Callback、Resourceが残らないか

## 制作反復

- BuildからTestへ移る手順
- 結果から原因を特定できるか
- 再試験までの手順
- YAMLやコードの手編集が必要か
- Adapterの重複が減っているか

## Architecture品質

- 正本と派生表現が分離されているか
- 単一書き込み所有者が守られているか
- ECS / Service / Module境界が明示されているか
- Debug、Save、Replay、Networkへ同じCommandを利用できるか

## Scalability

- Entity / Part / World数に対するCPU、Physics、Render、Memoryの増加
- Structural GraphとFunctional Networkの再計算範囲
- Active Simulation Regionの更新量
- Network Interest範囲

---

# 15. 現時点の結論

現在のGameEngineは、描画、ECS、PhysX、Scene、Prefab、Editor、Audioを持ち、複数ジャンルのGameを実装できる段階にある。

Platformer、STG、Board Game、Minigameの実制作から、Game固有ComponentとAdapterを追加できる拡張性は確認できた。一方で、Physics Query、Character Motor、Collision Contact、Debug Draw、UI、Feedback、外部状態の所有権は、複数Gameへ共通するShared Serviceとして不足している。

次に必要なのは、個別Gameの機能をCoreへ増やすことではない。

> **Service、ECS System Set、Domain ModuleをWorldごとに安全に構成し、必要な機能だけを選択して複数のGameとSimulationを同じRuntimeで共存させる能力を確立すること。**

その上でConstructをOptional Domain Moduleとして追加し、Robocraft型の中心ループを最初の高難度Vertical Sliceとして成立させる。

この順序なら、最終構想へ進むほど他ジャンルを扱えなくなるのではなく、最終構想で鍛えたService、ECS、Physics、Editor、Save、Debugの基盤を他のGameへ再利用できる。

この文書は固定仕様ではない。各PhaseのPrototypeと実制作から得た失敗条件を反映し、設計判断と優先順位を更新し続ける。
