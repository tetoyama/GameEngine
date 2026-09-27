# M-1 / M-6 Pick Sentinel & PhysX Ownership Completion

## 状態

**コード実装完了・VSビルド確認待ち（2026-07-10）**

`Docs/ECS_Scheduler_Migration_Plan.md` §2.5で残っていた次の2件を実装した。

- M-1残: GBuffer Param(UINT4)の無効ID sentinel Clear化
- M-6残: PhysX `ActorEntityInfo`（`PxActor::userData`）の`unique_ptr`所有化

## M-1: Pick無効ID sentinel

### 旧実装の問題

Param(UINT4)ターゲットは他のGBufferと共通の`{0,0,0,0}`でClearされており、
背景（非描画）PixelのPickはSceneID=0 / ObjectID=0を返していた。
偶然「Entity index 0とSceneContext ID 0が予約済み」であるため実害は出ないが、
これは暗黙依存であり、将来index 0が使われた瞬間に背景クリックが誤選択になる。

### 実装内容

- `Source/Shader/commonDefine.h`（C++ / HLSL共有）へ
  `GBufferParam_InvalidID (0xFFFFFFFFu)`を新設
- `GBufferPass::Execute`でParamスロットだけ専用Clear値
  `{InvalidID, InvalidID, 0, 0}`を使用
  - SceneID / ObjectID: sentinel
  - ShaderID / MaterialFlags: 既存契約（0 = 無効）を維持。
    `OutlineByShaderID.hlsl`等が「ShaderID 0 = 無効」を前提とするため、
    描画系の挙動は一切変えない
  - 整数フォーマットへの`ClearRenderTargetView`はfloat値が変換・クランプされる。
    `float(0xFFFFFFFF)` = 2^32はUINT最大値0xFFFFFFFFへ確定する
- `ViewWindow.cpp`のPick処理へsentinel早期returnを追加
  （`GetContextFromID` / `Resolve`の0拒否への依存を排除）

## M-6: PhysX ActorEntityInfoの所有一元化

### 旧実装の問題

`physicSystem.cpp`が`PxActor::userData`へ生`new ActorEntityInfo`を代入し、
Finalize / Stop / Collider再構築の計6箇所で生`delete`していた。
例外・早期returnでリークし、解放経路が分散して二重解放リスクがあった。

### 実装内容

- 所有を`PhysicSystem`メンバ
  `std::unordered_map<const physx::PxActor*, std::unique_ptr<ActorEntityInfo>>`
  へ一元化。`userData`には非所有の生ポインタだけを渡す
- `AttachActorEntityInfo(actor, entity, context)` / `DetachActorEntityInfo(actor)`
  を新設し、生new 2箇所・生delete 6箇所をすべて置換
- `Detach`は`userData`をnullptr化してからmap eraseする（dangling防止）
- `Finalize`末尾に`m_actorEntityInfos.clear()`のリークバックストップ
- 衝突コールバック（`onContact` / `onTrigger`）の`userData`読出しは
  非所有参照のため変更なし

## 変更ファイル

- `Source/Shader/commonDefine.h`
- `Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPass/GBuffer/GBufferPass.cpp`
- `Source/GameApplication/Engine/Editor/UI/ViewWindow.cpp`
- `Source/GameApplication/Engine/Scene/System/Physic/physicSystem.h`
- `Source/GameApplication/Engine/Scene/System/Physic/physicSystem.cpp`

## 完了条件

- [x] Param(UINT4)のSceneID / ObjectIDがsentinelでClearされる
- [x] ShaderID / MaterialFlagsの既存契約（0 = 無効）を維持する
- [x] Pickがsentinelを明示的に弾く
- [x] `new ActorEntityInfo` / `delete static_cast<ActorEntityInfo*>`が0件
- [x] ActorEntityInfoの所有経路が単一である
- [ ] Windows Debug x64 Build
- [ ] Windows Release x64 Build
- [ ] 実機確認: 背景クリックで選択解除もしくは無反応（誤選択なし）
- [ ] 実機確認: Entityクリック選択 / ダブルクリックのカメラフォーカス
- [ ] 実機確認: Play / Stop繰り返しでPhysXリーク・クラッシュなし
- [ ] 実機確認: Static ⇔ Dynamic切替（isDynamic変更）でのActor再構築

## 追記（2026-07-11）

初回実装は`physicSystem.cpp`内の生delete置換のみで、**`PhysicSystemTasks.inl`の
`ReleaseActor`に残っていた生delete経路を見逃していた**（Detachとの二重解放）。
コミット`452a9333`で`ReleaseColliderRuntime`が`DetachActorEntityInfo`経由へ
統一され解消済み。教訓: userData所有の変更は対象ディレクトリ全体
（`.inl`含む）を横断検索して経路を列挙すること。

## 補足

§2.5の残項目はH2 Phase2（Device / SwapChain / View / Query Pool完全再生成による
デバイスロスト復帰）とL（低優先・安定版前に一括）のみ。H2 Phase2は規模大かつ
実TDR発火での検証が必須のため、着手時は専用工程とする。
