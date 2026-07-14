# GameEngine ゲーム制作時の使用感・不足機能・改善ロードマップ

## 0. この文書の目的

この文書は、`game/platformer-tech-demo` ブランチの会話や作業履歴を知らない開発者・別ブランチ担当・AIエージェントでも、以下を理解できるようにするための独立した引き継ぎ資料である。

- 現在の GameEngine で、どの程度ゲームを完成させられるか
- 実際に3Dプラットフォーマーを制作した際、どこで詰まったか
- 問題がゲーム固有実装にあるのか、エンジン機能の不足にあるのか
- 次にどの汎用機能を、どの順番で実装すべきか
- 他のゲームブランチで同じ問題を繰り返さないための設計規則

本資料の観察元は主に PR #47 `game/platformer-tech-demo` だが、結論はPlatformer固有ではなく、今後の3Dアクション、TPS、ミニゲーム、AIゲーム、レベル制作全般へ適用することを想定している。

### 観察基準

- Repository: `tetoyama/GameEngine`
- 主な観察ブランチ: `game/platformer-tech-demo`
- PR: `#47`
- 文書作成時の基準HEAD: `d3829f241196b270021eacee92842cef1de81e2d`
- Engine: C++ / DirectX 11 / PhysX / ECS / ImGui / YAML
- 対象ゲーム: 5〜8分程度の直線型3Dプラットフォーマー技術デモ

### 検証上の注意

GitHub上のコード調査とWindows CIによるコンパイル確認は可能だが、作業担当AIはDirectX Editorを直接操作できない。実際の手触り、不自然さ、判定不良はユーザーによる実機プレイ報告を重要な根拠としている。

---

# 1. 総合評価

## 1.1 結論

現在のGameEngineは、

> **ゲームを作れるエンジンではあるが、ゲームを作りやすいエンジンにはまだなっていない。**

3Dプラットフォーマーでは、開始、移動、ジャンプ、敵、ボス、チェックポイント、HUD、演出、クリア、リスタートまで実装できた。したがって、基盤能力や拡張性が不足して何も完成できない状態ではない。

一方で、一般的なゲームエンジンなら標準機能として期待される処理までゲーム固有コードで構築する必要があり、ゲーム内容を作る時間よりも、物理判定・状態同期・エディタ不足・デバッグ不足へ対処する時間が長くなった。

## 1.2 用途別評価

| 用途 | 評価 | 理由 |
|---|---:|---|
| エンジン技術デモ | 高い | 描画・物理・ECS・Editor・独自機能を見せやすい |
| 個人による小規模ゲーム | 可能だが高コスト | C++で必要機能を追加できるが、標準ゲーム機能が少ない |
| 複数ジャンルの高速試作 | 低〜中 | 毎回Character、UI、Feedback、Queryを再構築しやすい |
| 非エンジニアを含むチーム制作 | 低い | Editor配置、UI、Gizmo、データ検証、調整導線が不足 |
| 長期運用ゲーム | 現状は危険 | 状態所有権、Scene互換性、デバッグ・テスト・配布工程が弱い |

---

# 2. 現在の強み

## 2.1 ゲーム固有機能を分離して追加できる

`CustomScriptComponent`、`ComponentRef<T>`、`EntityRef`、ECS Component、Scene/Prefabシリアライズがあるため、ゲーム機能を専用クラスとして分離できる。

Platformerでは以下を独立したゲームクラスとして実装できた。

- `PlatformerCharacterController`
- `PlatformerCameraController`
- `PlatformerEnemy`
- `PlatformerBoss`
- `PlatformerMovingPlatform`
- `PlatformerCheckpoint`
- `PlatformerCoin`
- `PlatformerHud`
- `PlatformerPlayerFeedback`
- `PlatformerClearFeedback`

この構造により、ゲーム側からRenderWorld、RHI、D3D11 Resource、RenderPass内部へ直接アクセスせずに制作できた。

## 2.2 描画・演出の基礎能力は高い

既存機能を組み合わせることで、専用Renderer改造なしに次を実現できた。

- Lit Material
- Metallic / Roughness
- Emissive
- Directional Light / Shadow
- Environment Map
- Bloom / BrightPass / Blur
- Particle
- Sprite HUD
- Outline
- Camera Impulse
- Material Flash
- BGM / SFX

自作エンジンとして、ゲームロジックから描画基盤が適切に分離されている点は強い。

## 2.3 問題をゲーム側Adapterで実験できる

Engine全体のAPIを即座に壊さず、ゲーム側で暫定Adapterを置ける。

例:

- `PlatformerSceneAccess`
- `PlatformerPhysicsProbe`
- `PlatformerFeedback`

これは、新機能をゲームブランチで検証し、実績が取れた後にEngineへ昇格させる流れを作りやすい。

## 2.4 ECS移行後の構造的な安全性は向上している

- 長期参照に`ComponentRef<T>`を使用できる
- Structural ChangeをCommand Bufferへ寄せられる
- SystemのRead/Write Accessを明示できる
- 描画を除く並列化へ発展できる

ただし、ゲームコードがPhysX ActorやTransformを直接操作する限り、ECS Access宣言だけでは防げない競合が残る。

---

# 3. 実制作で発生した代表的な問題

以下は単なる理論上の不足ではなく、Platformer制作中に実際に不具合として現れたものを整理したものだ。

## 3.1 接地したまま崖から落下できない

### 症状

- 崖外へ進んでもGroundedが解除されない
- 空中で落下状態へ移行しない
- CameraZone内で接地しているように見える

### 直接原因

- Ground RayがPlayer自身のColliderへ命中
- 古いSceneの`SelfLayerBit`と実際のCollider Layerが不一致
- Trigger ShapeがScene Query対象のままで、CameraZoneやCheckpointを床として取得
- 周辺Probeが広すぎ、崖端の1点だけでGroundedを維持

### 根本的なエンジン不足

- Query FilterがLayer Mask中心で、`ignoreEntity`や`ignoreActor`を指定できない
- Trigger / Static / Dynamicを明示して選べない
- API名からMaskがIncludeかExcludeか判断しにくい
- Groundingをゲームごとに実装している

## 3.2 着地が不自然、床に触れる前に接地する

### 症状

- 床へ到達する前に落下速度が消える
- 高い位置から吸着するように着地する
- 坂や段差付近でGroundedが不安定

### 直接原因

- Ground Probeの許容距離が長すぎた
- Ground Snapと斜面速度射影の意味が混在
- Uphill時の正のY速度をジャンプと誤認しやすい

### 根本的なエンジン不足

- Character Motor標準実装がない
- Ground Contact、Snap Distance、Slope Tangentが統一管理されていない
- Ray 1本または複数RayでCapsule接地を近似している

## 3.3 小さな段差へ引っ掛かる

### 症状

- 10〜30cm程度の段差で水平移動が止まる
- 階段や結合Meshの境界を滑らかに登れない
- Step Assistを追加してもほとんど作動しない

### 直接原因

- PhysX Dynamic Capsuleの丸みに任せておりStep Offsetがない
- Step AssistをCharacter ControllerではなくFeedback Scriptへ後付けした
- Playerと段差が同じDefault Layerを使い、自己除外Maskで段差まで除外した
- Transform / PhysX Actorを持ち上げても、別の移動処理が同じFixedUpdateで上書きし得る

### 根本的なエンジン不足

- Capsule SweepとStep Solverがない
- Characterの位置・速度の単一所有者が定義されていない
- QueryがEntity単位の無視に対応していない

## 3.4 敵やボスを踏めない

### 症状

- 見た目では上から接触しているのに踏み判定にならない
- ボスの高さを変更した後、踏み条件が物理上面より上へずれた
- Collider側面が先に接触してPlayerが押し返される

### 直接原因

- Collision Eventに接触点・法線がない
- Player原点YとBoss原点Yの差だけで「上から踏んだ」を判定
- 見た目Transform、Collider Offset、弱点高さが別々に調整された

### 根本的なエンジン不足

`HitInfo`に以下がない。

- Contact Point
- Contact Normal
- Relative Velocity
- Impulse
- Self Shape / Other Shape
- Contact Count

そのため、踏み判定、壁衝突、着地衝撃などを追加Rayや座標差で推測している。

## 3.5 CameraZoneに吸われるような感覚

### 症状

- Zone境界で移動感覚が変化する
- Zone内部で接地状態が変になる

### 直接原因

- Trigger ColliderがGround Queryに命中
- カメラ補間中も、毎FixedUpdateで移動入力を現在カメラ基準へ再変換する

### 根本的なエンジン不足

- Trigger Query制御不足
- Camera Relative Inputの基準をいつ固定するかというMovement Policyが未定義
- Camera Zone / Gameplay Zoneの可視化とデバッグ表示がない

## 3.6 ボス戦の開始境界と安置

### 症状

- 最後の階段を降り切る前にボス戦が開始
- 階段へ戻ることで突進を安全に処理できる
- 死に戻り時にボス戦外へ戻れる可能性

### 直接原因

- Bossとの距離だけで開始判定
- Arena Gateが物理的／論理的に存在しない
- CheckpointとEncounter Stateが独立していた

### 根本的なエンジン不足

- Encounter Volume
- Arena Boundary
- Encounter Checkpoint Override
- Encounter Resume / Reset Policy

といったゲーム進行用の汎用機能がない。

## 3.7 ヒットストップや演出が各所へ散らばる

### 症状

- Boss、GameManager、PlayerFeedbackがそれぞれCamera ShakeやParticleを呼ぶ
- Hit Stopのために全Dynamic Actorを列挙し停止・復帰
- Actor停止前の速度を復元すると、別処理が変更した速度を上書きする危険

### 根本的なエンジン不足

- Time Domain / Gameplay Pause Layerがない
- Feedback Serviceがない
- Camera Shake、Hit Stop、Audio、Particle、FlashをPresetとして束ねられない

## 3.8 Particle表現の制約

### 症状

- Burstを呼ぶたび以前の粒子を全消去し、多段爆発が上書きされる
- 全Particleが同一色・同一サイズ
- Boss Transformが縮小すると、同じEntity上のParticleも縮小

### 改修例

Platformerブランチでは、粒子ごとの`Color`と`SizeScale`を追加し、爆発と花吹雪を作った。

### 根本的なエンジン不足

- Particle Pool / Emitter分離
- Burst Append
- Per Particle Color / Size / Rotation
- World Space / Local Space選択
- Particle Preset Asset
- Multiple Emitters

## 3.9 UI制作が低水準APIに寄りすぎる

### 症状

- 数字をTexture Sliceで表示
- 文章による操作説明を簡単に追加できない
- 解像度・Safe Area・自動レイアウトの確認が難しい

### 根本的なエンジン不足

- Text Renderer
- Font Asset
- Anchor / PivotはあるがLayout Systemがない
- Horizontal / Vertical / Grid Layout
- Content Size Fitter
- Safe Area
- Localization Key
- Input Glyph
- UI Tween / Transition

## 3.10 Scene / Prefab制作の負担

### 症状

- 大規模なScene YAMLを手動または外部処理で生成・調整
- Entity数、Prefab配置、Collider、Trigger、Zoneの整合をコードで検査
- 見た目と物理寸法がずれやすい

### 根本的なエンジン不足

- 複数選択・整列・複製
- Grid / Surface Snap
- Prefab Override表示
- Collider Gizmo
- Trigger / Zone Gizmo
- Scene Validation
- Missing Asset検出
- Runtime PreviewとEditor値の差分表示

---

# 4. 最重要の設計問題: 状態所有権

機能不足以上に重要なのが、**誰がPlayerの位置と速度を最終決定するかが曖昧になりやすいこと**である。

Platformerでは以下がPlayerを直接変更した。

- `PlatformerCharacterController`
  - 通常速度
  - 重力
  - 接地
  - ジャンプ
- `PlatformerPlayerFeedback`
  - 走行補助
  - ジャンプ加速
  - Step Assist
- `PlatformerGameManager`
  - Arena Clamp
  - Respawn補正
  - Hit Stop
- `PlatformerMovingPlatform`
  - Rider Transport
- `PhysicSystem`
  - Transform Upload / Physics Download

この状態では、同じFixedUpdate内で処理順により結果が変わる。

## 4.1 今後の必須規則

> **Character MotorだけがPlayerの位置と速度を書き込む。**

他のシステムは直接TransformやPxRigidDynamicを操作せず、MotorへCommandを送る。

例:

```cpp
class CharacterMotor {
public:
    void SetMoveInput(const Vector2& input);
    void RequestJump();
    void AddImpulse(const Vector3& impulse);
    void SetExternalVelocity(const Vector3& velocity);
    void Teleport(const Vector3& position, TeleportReason reason);
    void SetMovementConstraint(const MovementConstraint& constraint);
    void SetPaused(bool paused);
};
```

## 4.2 ECS Access宣言だけでは不十分

ECS SchedulerでComponentのRead/Write Conflictを防いでも、複数Scriptが同じ`PxRigidDynamic*`を直接操作すれば競合は防げない。

したがって、以下のどちらかが必要。

1. PhysX Actor操作を専用System APIへ封じる
2. Gameplay ScriptはMotor Commandのみ発行し、Physics前の単一Systemで適用する

---

# 5. 優先して汎用化すべき機能

## P0: 次のゲーム制作前に必要

### 5.1 汎用Kinematic Character Motor

必要機能:

- Capsule Sweep
- Ground Probe / Ground Contact
- Step Offset
- Slope Limit
- Ground Snap
- Ledge Detach
- Depenetration
- Moving Platform Support
- External Impulse
- Teleport
- Trigger Ignore
- Dynamic Actor Push Policy

### 推奨方針

Dynamic Rigidbodyへ速度を設定する方式ではなく、Character専用のSweepベースMotorを検討する。

最低限のAPI例:

```cpp
struct CharacterMotorSettings {
    float radius;
    float height;
    float skinWidth;
    float stepHeight;
    float slopeLimitDegrees;
    float groundSnapDistance;
};

struct CharacterGroundState {
    bool grounded;
    Vector3 point;
    Vector3 normal;
    EntityRef surface;
    Vector3 surfaceVelocity;
};
```

### 完了条件

- 5〜35cmの段差を止まらず登れる
- 高い壁は登れない
- 崖で即座にGrounded解除
- 坂を上下してもY速度が破綻しない
- 移動床へ乗り降りできる
- Triggerを床として扱わない

## 5.2 Physics Query API刷新

現在の`RaycastWithMask`はMaskが除外指定であり、用途が読み取りにくい。

推奨API:

```cpp
struct PhysicsQueryFilter {
    uint32_t includeLayers = 0xFFFFFFFFu;
    uint32_t excludeLayers = 0;
    EntityRef ignoreEntity;
    const physx::PxActor* ignoreActor = nullptr;
    bool includeStatic = true;
    bool includeDynamic = true;
    bool includeTriggers = false;
};

bool Raycast(
    const Ray& ray,
    float maxDistance,
    const PhysicsQueryFilter& filter,
    RayHit& result);

bool SphereCast(...);
bool CapsuleCast(...);
bool OverlapCapsule(...);
```

### 互換性

既存の`RaycastWithMask`はすぐ削除せず、`RaycastExcludeMask`へ明示改名したCompatibility Wrapperを残す。

## 5.3 Collision Contact情報

`HitInfo`を拡張する。

```cpp
struct ContactPoint {
    Vector3 position;
    Vector3 normal;
    float separation;
    float impulse;
};

struct HitInfo {
    EntityRef other;
    uint32_t otherLayer;
    Vector3 relativeVelocity;
    ColliderHandle selfCollider;
    ColliderHandle otherCollider;
    std::span<const ContactPoint> contacts;
};
```

### 利用先

- Stomp判定
- Landing Impact
- Wall Collision
- Damage Direction
- Bounce
- Footstep Material

## 5.4 Gameplay Debug Draw / Gizmo

最低限表示するもの:

- Ray / SphereCast / CapsuleCast
- Hit Point / Normal
- Character Capsule
- Grounded状態
- Step下段・上段・上面Probe
- Trigger Bounds
- CameraZone
- Checkpoint
- Boss Activation Radius
- Boss Weak Point
- Arena Boundary

### 要件

- Editorのみ／Game View表示を切替可能
- Duration指定
- Category Filter
- Entity選択時のみ表示
- Releaseでは無効化可能

## 5.5 Player状態所有権の整理

- `PlatformerPlayerFeedback`のMovement AssistとStep AssistをCharacter Motorへ移す
- `GameManager`の位置補正を`MovementConstraint`へ変換
- `MovingPlatform`は直接Playerを移動せず、Platform VelocityをMotorへ渡す
- Hit StopはActor操作ではなくTime Domainを止める

---

## P1: 制作速度を大きく改善する

### 5.6 Editorレベル制作機能

- Multi Select
- Duplicate
- Align / Distribute
- Grid Snap
- Surface Snap
- Local / World切替
- Collider Auto Fit
- Trigger / Zone Preset
- Prefab Override確認
- Scene Validation

### Scene Validation例

- Collider Layerが未設定
- TriggerがScene Query対象
- Dynamic ColliderにTriangle Mesh
- Missing Asset
- Duplicate Name
- Required Component不足
- Player Self Layer不一致
- CameraZone重複

### 5.7 UIシステム

最低限:

- TextRenderer
- Font Asset
- Localization Key
- Anchor / Pivot
- Horizontal / Vertical / Grid Layout
- Safe Area
- Reference Resolution
- Scale Mode
- Button / Focus / Navigation
- Tween / State Transition

### 5.8 Feedback Service

```cpp
struct FeedbackRequest {
    Vector3 position;
    FeedbackPresetID preset;
    EntityRef source;
    EntityRef target;
    float scale = 1.0f;
};

FeedbackService::Play(request);
```

Presetに含めるもの:

- Particle
- Audio
- Camera Shake
- FOV Kick
- Hit Stop
- Material Flash
- Screen Flash
- Controller Vibration

### 5.9 Time Domain

例:

- Real Time
- Gameplay Time
- UI Time
- Particle Time
- Camera Time

Hit StopではGameplayだけ止め、CameraとUIは継続できるようにする。

---

## P2: 完成・提出・運用を支える

### 5.10 Input Action Map

- Action Name
- Keyboard / Mouse / Gamepad Binding
- Rebind
- Pressed / Released / Held
- Buffer
- Context切替
- Input Glyph

### 5.11 Release Build / Packaging

- Windows Release x64 CI
- Clean Checkout Build
- Asset Packaging
- Missing Asset Audit
- Writable Directory依存の排除
- Crash Log
- Version表示

### 5.12 自動Playtest・Runtime Audit

- Start-to-Clear Smoke Mode
- Scene Load Test
- Respawn Test
- Boss State Transition Test
- Navigation Failure Log
- Runtime Error / Warning収集
- Seed付き再現

---

# 6. 推奨ロードマップ

## Phase A: Physics Query基盤

1. `PhysicsQueryFilter`追加
2. `Raycast`のInclude / Exclude意味を明確化
3. `ignoreEntity` / `ignoreActor`対応
4. Trigger / Static / Dynamic Filter
5. SphereCast / CapsuleCast追加
6. Debug Draw連携

## Phase B: Character Motor

1. Sweep移動
2. Ground State
3. Slope
4. Step
5. Snap / Ledge Detach
6. Depenetration
7. Moving Platform
8. External Command API
9. Platformerを新Motorへ移行

## Phase C: Collision Event拡張

1. PhysX Contact抽出
2. Contact Lifetime設計
3. Script Eventへ安全な値コピー
4. Stomp / Landing / Wall処理移行
5. 旧`HitInfo`互換維持

## Phase D: Editor / Debug

1. Query Gizmo
2. Collider / Trigger表示
3. Zone表示
4. Scene Validator
5. Multi Select / Snap / Align

## Phase E: UI / Feedback

1. Text
2. Reference Resolution
3. Layout
4. Feedback Preset
5. Time Domain

---

# 7. 他ブランチ担当への実装指針

## 7.1 Platformer固有コードをそのまま他ゲームへコピーしない

次のコードは実験実績として有用だが、最終的な汎用APIではない。

- `PlatformerPhysicsProbe`
- `PlatformerPlayerFeedback::ApplyStepAssist`
- `PlatformerGameManager`のHit Stop
- Bossの原点差によるStomp判定

これらは、必要性と失敗条件を示すReference Implementationとして扱う。

## 7.2 汎用化はFoundation側で行う

推奨:

- Engine API変更は`refactor/ecs-scheduler-foundation`系統、または専用基盤ブランチ
- Game BranchではCompatibility Adapterを使用
- 基盤完成後、Platformer / Minigame / US3Sを段階的に移行

## 7.3 互換性を維持する

SceneやPrefabには古いLayer値・Field名が残る可能性がある。

- Field削除よりMigration
- API削除よりDeprecated Wrapper
- Scene Decode時の補正を明文化
- 新旧挙動を同時に長期間維持しすぎない

## 7.4 調整値と不具合回避を混同しない

例:

- Probe Radiusを増やして接地を安定化すると、崖から落ちなくなる
- Ground Distanceを増やすと、着地前に吸着する
- Step Heightを上げると、壁を登る
- Stomp Marginを下げると、側面接触でも踏める

数値調整で症状を隠す前に、Query対象、Contact情報、State Ownershipを確認する。

## 7.5 実機検証を必須にする

最低検証項目:

- 平地開始
- 坂上り／下り
- 段差正面／斜め
- 崖離脱
- Trigger通過
- 移動床
- 通常敵踏み
- Boss踏み
- Boss中死亡
- Clear演出
- Restart

---

# 8. 現在のPlatformerブランチで残っている技術的負債

## 8.1 Movement AssistとStep AssistがFeedbackに存在

`PlatformerPlayerFeedback`は本来Presentation担当だが、速度や位置にも介入している。Character Motorへ移行するまでの暫定実装と扱う。

## 8.2 Hit Stopが全Dynamic Actor停止方式

短期デモとしては動作可能だが、以下の危険がある。

- Stop中にActorが無効化・削除される
- 復帰時に古い速度を上書き
- UI / Camera / Particleとの時間関係が曖昧
- 将来のMultithreadingと競合

## 8.3 Groundingが複数Ray近似

Capsuleの実接触ではないため、Triangle Mesh Seam、端、急斜面、薄い床で限界がある。

## 8.4 Scene上の物理Layerが一貫していない

Playerと一部EnvironmentがDefault Layerを共有した事例がある。EditorのScene Validatorで検出すべき。

## 8.5 Player用ParticleをClear演出でも共有

クリア後なので致命的ではないが、Boss Explosion / Confetti専用Emitterへ分離すべき。

---

# 9. 成功と判断できる状態

GameEngineを「ゲームを作りやすいエンジン」と評価できる最低条件は以下。

- 新規3Dキャラクターが1日以内に坂・段差・移動床対応で動く
- Raycast Filterの意味をコードを読まず理解できる
- StompやWall HitをContact情報だけで実装できる
- Debug Drawで判定不良を即座に特定できる
- Level DesignerがYAMLを直接編集せずコースを作れる
- 操作説明を画像作成なしにText UIで追加できる
- Hit FeedbackをPreset 1つで再利用できる
- Release BuildとAsset PackagingがCIで通る
- Game ScriptがPhysX Actorを直接操作しなくてよい

---

# 10. 次の担当者が最初に行うべきこと

1. この文書と`Docs/Platformer_Tech_Demo_Plan.md`を読む
2. `PlatformerCharacterController.h`、`PlatformerPlayerFeedback.h`、`PlatformerSceneAccess.h`を比較する
3. Player位置・速度へ書き込む全箇所を列挙する
4. `PhysicsQueryFilter`の設計を先に確定する
5. Character Motorの責務と外部Command APIを設計する
6. Debug Drawを同時に用意する
7. PlatformerのStep AssistとGroundingを新Motorの受入テストに使用する
8. Foundationへ入れる前に、Minigame / US3S側の利用可能性も確認する

---

# 11. 最終所見

このGameEngineの強みは、問題が起きたときにEngine内部まで降りて解決できること、描画・ECS・物理・Editorを自分の方針で発展させられることにある。

弱みは、一般的なゲーム制作で繰り返し必要になる機能がまだ抽象化されておらず、各ゲームブランチで局所解を作りやすいことにある。

次に優先すべきなのは新しい描画表現ではない。

> **Physics Query、Character Motor、Collision Contact、Debug Draw、State Ownershipを整え、ゲーム固有コードが「遊びの実装」に集中できる状態へ変えること。**

この5点が整えば、現在のGameEngineは「技術力を見せる自作エンジン」から、「複数のゲームを効率よく完成させられる制作基盤」へ進める。
