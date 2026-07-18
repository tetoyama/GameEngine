# Project Vision: Robocraft-Centered Extensible Simulation GameEngine Roadmap

## 状態

**ビジョン定義・基盤フェーズ進行中（2026-07-18更新）**

この文書は、GameEngineの長期的な設計目標、設計判断の優先順位、実装フェーズ、各フェーズの完了条件を定義する。

本エンジンは、単一作品専用のコードベースにも、UnityやUnreal Engineの機能数を追う汎用エンジンにもならない。

> **Robocraftの「設計した機体を自分で操縦し、破損や性能不足の原因を理解して設計を改善する」体験を中心に置き、Minecraft、Create、Valkyrien Skies 2、Noitaに見られる編集可能世界、機械ネットワーク、可動ローカル世界、物質反応を段階的に統合できる、拡張可能なシミュレーションゲーム基盤を目指す。**

オンライン対戦は、この基盤を利用する実行形態の一つであり、最終目的ではない。シングルプレイ、協力、対戦、サンドボックス、技術デモ、AI実験へ展開可能な構造を維持する。

関連資料:

- `Docs/ECS_Scheduler_Migration_Plan.md`
- `Docs/GameEngine_Game_Development_Usability_Assessment.md`

---

# 0. エグゼクティブサマリー

## 0.1 最終的に作りたいもの

プレイヤーが部品、ブロック、機械、資源を組み合わせて構造物を設計し、その構造が物理・接続・動力・電力・熱・物質・戦闘ルールによって実際に機能するゲームを作る。

中心となる反復は次の通り。

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

この反復が成立していない段階で、Voxel World、Factory、Material Simulation、Online PvPを広げない。

## 0.2 作品要素は同列ではない

参照作品の機能を均等に混ぜるのではなく、役割を明確に分ける。

| 参照軸 | 本構想での役割 |
|---|---|
| Robocraft | プレイヤー体験、ゲームループ、設計と戦闘の因果関係を定義する中心軸 |
| Valkyrien Skies 2 | ブロック集合を独立した可動構造として扱う空間・物理基盤 |
| Minecraft | 編集可能で永続化される大規模なブロック世界と拡張性 |
| Create | 接続関係から機械、物流、加工、制御が生まれる機能ネットワーク |
| Noita | 熱、火、液体、電気などが局所反応し、予測外の連鎖を生む物質シミュレーション |

Robocraftが中心体験を決め、他の要素はその体験を拡張する。

## 0.3 最初の大目標

> **20〜100個の部品から機体を組み、走らせ、撃ち、部品が壊れ、複数のConstructへ分裂し、残った構造に応じて操作性と戦闘能力が変わり、Blueprintとして保存・復元できる。**

このVertical Sliceが完成するまでは、大規模Voxel World、全面的な物質シミュレーション、オンライン対戦を主開発目標にしない。

---

# 1. プレイヤー体験の設計原則

## 1.1 設計が実際の挙動を決める

ゲーム側が完成品の乗り物や正解を与えるのではなく、プレイヤーが作った構造をSimulationが解釈する。

設計結果として変化する対象:

- 重量
- 重心
- 慣性
- 推力中心
- 接地性
- 旋回性
- 安定性
- 武器配置
- 射界
- 反動
- 装甲
- 冗長性
- 動力経路
- 電力供給
- 熱と冷却
- 破損後の残存能力

見た目だけ異なる部品配置ではなく、構造そのものが挙動を決める必要がある。

## 1.2 破壊は後付けしない

部位破壊は演出ではなく、構造設計とSimulation Pipelineの前提とする。

初期段階から考慮するもの:

- Part単位の識別と耐久
- Structural Connectivity
- 接続切断
- Connected Components
- Construct Split
- 質量、重心、慣性の再計算
- Weapon、Wheel、Thruster、Seatなどの機能喪失
- 分裂後のPhysicsとRender再生成
- Damage結果の保存、Replay、Network表現

「完成した機体に後から部位破壊を追加する」順序は採用しない。破壊を前提にしないデータ構造は、後で作り直す可能性が高い。

## 1.3 壊れてもすぐ全損にしない

重要なのはHPを削って機体全体を消すことではない。

- 武器を失う
- 片側の車輪を失う
- 推力が偏る
- 電力経路が切れる
- 操縦席は残るが移動不能になる
- 分離した残骸が物理的に残る
- 不完全な状態でも帰還や反撃が可能になる

この部分的失敗が、設計改善の理由になる。

## 1.4 原因を理解できることを重視する

複雑なSimulationでも、プレイヤーが原因を理解できなければ設計ゲームとして成立しない。

必要な可視化:

- 重心と推力中心
- 接続強度
- Damage経路
- 電力供給
- 回転速度とトルク
- 流量と圧力
- 温度と冷却
- 操縦入力の伝達先
- 機能停止理由
- 分裂予測

Build Mode、HUD、Damage Feedback、Debug Viewは補助機能ではなく、中心体験の一部として設計する。

## 1.5 設計から試験までを速くする

Robocraft型の面白さは、設計と実戦の反復速度に強く依存する。

目指す導線:

```text
Build
  ↓
即時Validation
  ↓
Test Drive / Test Fire
  ↓
Damage Test
  ↓
原因表示
  ↓
Buildへ復帰
```

Scene YAMLやコードを手編集しなければ試せない状態を、完成したBuild Modeとは扱わない。

---

# 2. エンジンとしての成功条件

最終ゲームだけが動いても、他ジャンルを扱えなくなった場合は成功とはしない。

成功条件:

1. Robocraft型の構造Simulationを無理なく実装できる。
2. Platformer、TPS、STG、ボードゲーム、ミニゲームがConstruct Moduleなしで動作できる。
3. Worldごとに必要なServiceとDomain Moduleだけを選択できる。
4. ゲーム固有Adapterを追加してもEngine Coreを汚染しない。
5. 複数のゲームやSceneで実績を得たAdapterだけを汎用Serviceへ昇格できる。
6. Authoritative State、Physics、Render、Editor、Save、Networkの責務が混線しない。
7. プレイヤー生成構造を破壊、保存、複製、検証、再生できる。
8. 大量Partを1 Part = 1 Entityへ固定せず、用途に応じたStorageで扱える。

## 2.1 自作エンジンを選ぶ理由

- 完全に把握可能なC++コード
- ゲーム固有Adapterを自由に追加可能
- Engine内部まで制約なく変更可能
- ECS Storage、System Phase、Scheduler、Render Pipeline、Physics連携をゲーム要件に合わせられる
- 状態の正本、更新順序、保存形式、Network同期単位をゲームの中心概念に合わせて定義できる
- 失敗した抽象化をCompatibility Layerごと整理し直せる

> **自作エンジンの価値は、内部コードへアクセスできることではなく、ゲームの中心概念をEngine Architectureの中心へ置けることにある。**

## 2.2 Unity / Unreal Engineを機能数で追わない

優先しないもの:

- 映画制作向け機能の網羅
- Hair、Clothなどの全面対応
- 全Platformへの早期対応
- 巨大なAsset Store相当
- 汎用Visual Scriptingの完全再現
- 全ジャンル向け既製Gameplay Framework

優先するもの:

- プレイヤー生成構造
- 動的な分離と結合
- 大量部品向けStorage
- 可動ローカルWorld
- 構造、機械、電力、流体、熱の連携
- 部品破壊から機能喪失までの一貫したPipeline
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
├─ Navigation
├─ UI / Localization
├─ Feedback
├─ Save / Replay
└─ Network Transport

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

すべてのゲームへConstruct、Voxel、Material Simulationを強制しない。

## 3.2 Service Scope

| Scope | 例 | 原則 |
|---|---|---|
| Engine | Resource、Graphics Device、Job System | Process全体で共有 |
| Session | Network Session、Profile、Replay | Game Session単位 |
| World | Physics Scene、ECS World、Construct Registry | 独立Simulation単位 |
| Scene | Encounter、Level Script、Scene UI | Sceneロードと連動 |
| Entity | Character、Weapon、高機能Part | ECS Component / Systemで管理 |

Global Singletonを増やさず、`EngineContext`、`WorldContext`、Service Registryから明示的に解決する。

## 3.3 Adapterの役割

Adapterは、Engine不足を恒久的に隠す迂回路ではなく、必要条件を実制作で検証するための境界とする。

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

- 複数のGameまたはSceneで同じ要求が発生した
- 状態所有者と更新Phaseを定義できる
- ゲーム固有名称を除去できる
- Serialization、Debug、Testを含む契約を作れる
- 外部Libraryの生ポインタをGameplayへ漏らさない

Game側へ残すもの:

- ルール固有の勝敗判定
- 作品固有の操作感
- 特定の演出タイミング
- 一つの作品だけで使う特殊データ
- 汎用化すると契約が複雑になるだけの機能

---

# 4. 状態所有権とSimulation Pipeline

## 4.1 Construct Stateを正本にする

将来のConstructでは、ECS Entity、PhysX Actor、Render Objectを状態の正本にしない。

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

Physics Aggregate
ECS Runtime Proxy
Render Batch
Editor Handle
Network Snapshot
```

PhysicsやRender表現を破棄しても、Construct Stateから再生成できることを不変条件とする。

## 4.2 単一書き込み所有者

同じ状態を複数Systemが直接変更しない。

- Player位置と速度はCharacter Motorだけが最終書き込みする
- ConstructのPart追加・削除はConstruct Command経由だけで行う
- Structural GraphはStructural Systemだけが確定する
- Physics ActorはPhysics Representation Systemだけが再構築する
- Render BatchはRender Representation Systemだけが更新する
- Network受信データは検証済Domain Commandへ変換する

ECSのRead / Write Access宣言だけでは、複数Scriptによる同一PhysX Actorの直接操作を防げない。外部Library操作も所有Systemへ封じる。

## 4.3 標準破壊Pipeline

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

## 4.4 RevisionとCommand

Construct変更は、直接配列を書き換えるのではなくOperationとして表現する。

```cpp
struct ConstructOperation {
    ConstructID construct;
    ConstructRevision expectedRevision;
    OperationType type;
    OperationPayload payload;
};
```

対象:

- Part配置
- Part削除
- 回転
- Damage
- Repair
- Paint
- Docking
- Split / Merge
- Blueprint適用

CommandとRevisionを共通単位にすることで、Undo、Redo、Save Diff、Replay、Network同期、AI操作を同じ境界へ揃える。

---

# 5. Constructを第一級概念にする

## 5.1 ConstructはOptional Domain Module

Constructは最終構想の中心だが、すべてのGameへ強制するEngine Core Objectにはしない。

PlatformerやBoard Gameは通常のECS Worldだけで動作する。Vehicle、Ship、Factory、Robocraft型ゲームだけがConstruct Moduleを登録する。

## 5.2 基本データ案

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

Construct Stateに含めるもの:

- Part Type / Stable ID
- GridまたはSocket上の配置
- Local Transform
- 接続Port
- HP / Damage / Temperature
- 所有者 / Team
- Structural Connectivity
- Mass / Center of Mass / Inertia
- Mechanical / Electrical / Fluid / Logic / Thermal Network
- Revision

派生表現へ置くもの:

- PhysX Shape / Actor
- Render Instance / Mesh Batch
- Particle / Audio
- Editor Selection Handle
- Replication Snapshot

## 5.3 ECSとの関係

`1 Part = 1 Entity`を固定ルールにしない。

- 数十個の高機能PartはECS Entity化できる
- 数百〜数千個の単純BlockはConstruct専用SoA Storageへ格納する
- Weapon、Seat、Engine、Sensorなど高機能PartだけEntity Proxyを持てる
- RenderはConstruct単位のBatch / Instanceへ集約する
- 初期Physicsは`1 Construct = 1 RigidBody`を基本とする
- 分裂時にConnected Componentごとに新しいConstructを生成する

ECSは万能データ形式ではなく、System間連携とRuntime Proxy管理に使用する。

## 5.4 構造Graphと機能Graphを分ける

一つのGraphへすべての接続を詰め込まない。

```text
Structural Graph
Mechanical Graph
Electrical Graph
Fluid Graph
Logic Graph
Control Graph
Thermal Graph
```

Structural Graphは物理的な接続と分裂を決める。Functional GraphはPartのPort種別と接続状態から供給・伝播・停止を計算する。

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

## 6.4 実制作から確認できたこと

Platformer、Minigame、Board Game、STGなど、異なるGameをGame固有ComponentとAdapterとして追加できている。

確認できた強み:

- Game側からRenderWorld、RHI、D3D11 Resourceへ直接依存せず描画できる
- Game固有AdapterでPhysics QueryやFeedbackの仮説検証ができる
- Scene / Component駆動でGameを構成できる
- Engine内部を必要に応じて修正できる

確認できた不足:

- Physics QueryのFilter契約が不明瞭
- Character移動の汎用基盤がない
- Collision Contact情報が不足
- Gameplay Debug DrawとScene Validationが弱い
- UI Text、Layout、Localizationが不足
- Feedback、Particle、Time Domainが分散
- Scene / Prefabを調整するEditor導線が不足
- 複数Systemが外部Physics状態を書き換えやすい

これらは最終構想とは別の寄り道ではない。Construct、Dynamic Local World、Damage Simulationを安全に実装するための前提である。

---

# 7. 最終構想に必要なDomain Module

## 7.1 Construct Kernel

規模: **XL / 最重要**

- Grid / Socket配置
- Part追加、削除、回転
- Part Definition
- Part Storage
- Structural Graph
- Connected Components
- Split / Merge
- Revision / Command
- Mass / Center of Mass / Inertia
- Blueprint
- Undo / Redo Operation
- Runtime Representation生成

## 7.2 Build Mode

規模: **XL / 中心体験**

- Part Palette
- Ghost Preview
- Placement Validation
- Grid / Socket Snap
- Symmetry / Mirror
- Box Select
- Copy / Paste
- Paint
- Undo / Redo
- Blueprint Library
- 重量、重心、推力中心、Network可視化
- Test Drive / Test Fireへの即時遷移

Build Mode UXはEditor補助ではなく、プレイヤー体験そのものである。

## 7.3 Vehicle / Control

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
- Control Authority

## 7.4 Structural Damage

規模: **XL / 中心体験**

- Part HP
- Armor / Damage Type
- Penetration
- Explosion
- Connection Strength
- Part Detach
- Construct Split
- Debris Policy
- Mass Property再計算
- Weapon / Movement / Control喪失
- Damage原因表示

## 7.5 Dynamic Local World

規模: **XL**

- Construct Local / World変換
- Construct上のEntity
- 動く構造上を歩くCharacter
- Local Velocity継承
- Construct上のRay / Shape Query
- Docking / Undocking
- Joint
- Construct間接触
- Split / Merge時のEntity移行
- Large World Precision

## 7.6 Functional Network

規模: **XL**

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

## 7.7 Voxel / Chunk World

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
- World BlockとConstruct Partの変換境界

既存Terrain Systemとは別のDomain Moduleとして設計する。

## 7.8 Material Simulation

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

## 7.9 Save / Replay / Diagnostics

- Blueprint
- Construct Operation Log
- Revision Snapshot
- Damage Timeline
- Replay
- Deterministic Seed
- Runtime Audit
- Failure Reason
- Performance Counter

設計改善を支えるため、結果だけでなく原因を記録できる構造にする。

## 7.10 Network / Replication

規模: **XL / 最難関の一つ**

- Authoritative Server
- Entity Replication
- Construct Operation同期
- Revision / Diff同期
- Physics Snapshot
- Prediction / Reconciliation
- Interest Management
- Chunk同期
- Damage / Split同期
- Join-in-progress
- Dedicated Server
- Mod Compatibility

完全決定論だけへ依存せず、Domain Command、Revision、Snapshot、補正を組み合わせる。

---

# 8. フェーズ別ロードマップ

各Phaseは巨大実装として進めず、必ず遊べるか検証できるVertical Sliceを持つ。

## Phase R0: Foundation Stabilization

目的: 通常Game制作と将来のSimulationで共通して必要な基盤を安定させる。

- [~] ECS / Scheduler / Job System移行
- [ ] Component外部状態の所有権規則
- [ ] Physics Query Filter
- [ ] Raycast / SphereCast / CapsuleCast / Overlap
- [ ] Collision Contact情報
- [ ] Character Motor
- [ ] Gameplay Debug Draw
- [ ] Scene Validation
- [ ] UI Text / Layout基礎
- [ ] Feedback Service / Time Domain
- [ ] Device Lost / Release Build / Asset Packaging

完了条件:

- Platformer、STG、Minigameで同じPhysics Query、Feedback、UI基盤を再利用できる
- Gameplay ScriptがPhysX ActorやRender内部を無秩序に直接変更しない
- Windows Debug / ReleaseのClean Buildが安定する

## Phase R1: Service / World Composition

目的: 異なるGameとSimulationを同一Engineで安全に共存させる。

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

目的: 最終構想の正本データを確立する。

- [ ] Part Definition
- [ ] Grid / Socket
- [ ] Part Storage
- [ ] Structural Graph
- [ ] Connected Components
- [ ] Revision / Command
- [ ] Mass / Center of Mass / Inertia
- [ ] `1 Construct = 1 PhysX RigidBody`
- [ ] Render Instance / Batch生成
- [ ] Blueprint Save / Load

Vertical Slice:

> 20〜100個のBlockを配置し、1台の剛体として走らせ、Blueprintとして保存・復元する。

## Phase R3: Build Mode

- [ ] 配置 / 削除 / 回転
- [ ] Ghost Preview / Validation
- [ ] Symmetry / Mirror
- [ ] Selection / Copy / Paste
- [ ] Undo / Redo
- [ ] 性能、質量、重心表示
- [ ] Blueprint Library
- [ ] Test Driveへの即時遷移

完了条件:

- CodeやYAMLを手編集せず機体を完成できる
- 不正接続や浮遊Partを配置時に理解できる
- BuildとTestを短い導線で反復できる

## Phase R4: Small Robocraft Vertical Slice

目的: 中心体験を最小構成で成立させる。

- [ ] Wheel / Thruster / Hoverのうち最低2種類
- [ ] Seat / Input Mapping
- [ ] Weapon / Projectile / Hitscan
- [ ] Part HP / Damage
- [ ] Structural Graph再計算
- [ ] Construct Split
- [ ] 分裂後のPhysics再生成
- [ ] 推進、武器、操縦能力の喪失
- [ ] Damage原因表示
- [ ] Feedback / Camera / Audio連携
- [ ] Buildへ戻り設計を修正する導線

Vertical Slice:

> 機体を組み、走らせ、撃ち、部品が壊れ、複数のConstructへ分裂し、残存部品に応じて操作性が変わり、原因を確認して設計を改善する。

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

> GeneratorからCableを通してMotorとWeaponへ電力を供給し、途中のPart破壊で機能が停止する。

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

# 9. フェーズゲート

機能数ではなく、次のGateを通過したかで進捗を判断する。

## Gate A: Constructを保存できる

- 20〜100 Partを配置できる
- Stable IDとRevisionを持つ
- Blueprintへ保存・復元できる
- PhysicsとRenderを正本から再生成できる

## Gate B: プレイヤーがコードなしで設計できる

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

## Gate E: 可動ローカル世界が成立する

- Construct上をCharacterが歩ける
- Local / World Queryが一貫する
- Docking、Split、MergeでEntityが正しく移行する

## Gate F: 接続から機能が生まれる

- Port接続から電力、機械、論理Networkが構築される
- 破壊でNetworkが再計算される
- 停止理由を可視化できる

## Gate G: Worldと物質が統合される

- Voxel Worldを編集、保存、Streamingできる
- World BlockとConstruct Partを移動できる
- Active Region内で物質反応が動く
- Material結果がConstruct Damageへ接続される

---

# 10. 設計上の不変条件

1. **Robocraft型の設計・操作・破壊・再設計ループを最上位の判断基準にする。**
2. **破壊を後付け機能として扱わない。**
3. **Game固有機能を理由なくEngine Coreへ入れない。**
4. **Domain ModuleはOptionalであり、未使用Gameへ依存を強制しない。**
5. **状態の正本を一つにし、PhysicsとRenderを派生表現として扱う。**
6. **外部Libraryの生ポインタ操作を所有Systemへ封じる。**
7. **構造変更はCommand / Revisionを経由する。**
8. **Adapterは検証用境界であり、恒久的な重複実装にしない。**
9. **汎用化は利用実績と失敗条件を得てから行う。**
10. **ECS Entity数を増やすこと自体を設計目的にしない。**
11. **保存、Debug、Editor、TestをRuntime機能の後付けにしない。**
12. **複雑さを増やす機能は、中心ループを強化する場合だけ導入する。**
13. **Online、Voxel、Material Simulationを中心ループ成立前の逃げ道にしない。**
14. **Unity / Unrealの機能数ではなく、このゲーム群への適合性で優先順位を決める。**

Construct Module固有の不変条件:

- Construct StateがPart状態の唯一の正本
- Part追加・削除はConstruct Command経由のみ
- Physics Aggregateは再生成可能な派生キャッシュ
- Render Representationは再生成可能な派生キャッシュ
- Split / MergeはStructural Phaseだけで確定
- Functional NetworkはStructural Revisionへ追従
- NetworkはDomain CommandとRevisionを検証して適用
- Build Mode、Save、Undo、Replay、AIが同じOperation境界を利用する

---

# 11. 当面の優先順位

## P0: 一般ゲーム制作基盤

1. Physics Query Filter / Shape Cast
2. Collision Contact情報
3. Character Motorと外部状態の所有権
4. Gameplay Debug Draw / Scene Validation
5. UI Text / Layout
6. Feedback Service / Time Domain
7. Release Build / Packaging

## P1: Engine構成能力

1. Service Scope
2. World単位Service Registry
3. Optional Domain Module
4. Simulation Profile
5. 複数World実行
6. Adapter昇格ルール

## P2: Construct縦切り

1. Part Definition
2. Part Storage
3. Grid / Socket
4. Structural Graph
5. Revision / Command
6. Mass Property / Compound Physics
7. Blueprint Save / Load
8. 最小Build Mode

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

> **Construct Kernelと最小Build Modeを作り、20〜100 Partの機体を保存・操作・破壊できるSmall Robocraft Vertical Sliceへ到達する。**

---

# 12. 方針判断の基準

新機能を追加する前に、次を確認する。

1. この機能はRobocraft型の中心ループをどう強化するか。
2. Engine Core、Shared Service、Domain Module、Game Adapterのどこに属するか。
3. 正本となる状態は何か。
4. 誰が書き込みを所有するか。
5. どのSystem Phaseで確定するか。
6. 保存、Undo、Replay、Network、Debugでどう表現するか。
7. 使わないGameへ依存を強制しないか。
8. 一般Engineの模倣ではなく、実際のGame要件から必要になったか。
9. 小さなVertical Sliceで価値と失敗条件を検証できるか。
10. 破壊、分離、再構築を妨げるデータ構造になっていないか。
11. プレイヤーが結果の原因を理解できるか。
12. 設計と試験の反復速度を悪化させないか。

---

# 13. 他ブランチ・他担当への引き継ぎ規則

この文書を読む担当者やAIエージェントは、次を前提とする。

- 最終目的を「Robocraft風オンライン対戦ゲーム」と短縮しない
- Minecraft、Create、VS2、Noitaの機能を同時に実装し始めない
- まずRobocraft型の設計・操作・破壊・再設計ループを成立させる
- Engine Core変更とGame Prototypeを同じ責務で混在させない
- Game Adapterで失敗条件を取得してから汎用化する
- Construct State以外を正本にしない
- Physics ActorやRender Objectへの直接書き込みを増やさない
- 新しいModuleにはSerialization、Debug、Test、Editor導線を同時に設計する
- 各作業は現在のPhaseとGateのどこを進めるか明記する
- 遊べるVertical Sliceへ接続しない巨大基盤実装を続けない

作業報告には最低限、以下を含める。

1. 対象Phase / Gate
2. 変更した正本データ
3. 書き込み所有System
4. Serializationへの影響
5. Debug / Validation方法
6. 既存Gameへの互換性
7. 次のPlayable Verification

---

# 14. 進捗評価指標

単純な機能数ではなく、以下を観測する。

## 14.1 制作反復

- BuildからTestへ移るまでの手順
- Test結果から原因を特定できるか
- 設計修正を再試験するまでの手順
- YAMLやコードの手編集が必要か

## 14.2 Simulation品質

- Part配置が質量、重心、推進、射界へ反映されるか
- 部分破壊が段階的な能力喪失を生むか
- Split後も残存Constructが正しく動くか
- 同じBlueprintとSeedで再現できるか

## 14.3 Architecture品質

- 正本と派生表現が分離されているか
- 単一書き込み所有者が守られているか
- 未使用GameへModule依存を強制していないか
- Debug、Save、Replay、Networkへ同じCommandを利用できるか

## 14.4 Scalability

- Part数に対するCPU、Physics、Render、Memoryの増加
- Structural Graph再計算範囲
- Functional NetworkのDirty Rebuild範囲
- Active Simulation Regionの更新量
- Network Interest範囲

---

# 15. 現時点の結論

現在のGameEngineは、描画、ECS、PhysX、Scene、Prefab、Editor、Audioを持ち、複数ジャンルのゲームを実装できる段階にある。一方で、ゲームを作りやすくする共通基盤と、最終構想の中心であるConstruct系Moduleはまだ不足している。

したがって、当面は次の二本を並行して進める。

1. Platformerなどの実制作で露出したPhysics Query、Character Motor、Contact、Debug、UI、Feedback、Editorの不足を共通基盤として解消する。
2. 全機能を先に作ろうとせず、Construct KernelからSmall Robocraft Vertical Sliceまでを段階的に完成させる。

> **このエンジンの差別化は、何でも作れることだけではない。プレイヤーが作った構造を、操作し、壊し、理解し、改善できることにある。**

この文書は固定仕様ではない。各Vertical Sliceと実制作から得た失敗条件を反映し、設計判断と優先順位を更新し続ける。
