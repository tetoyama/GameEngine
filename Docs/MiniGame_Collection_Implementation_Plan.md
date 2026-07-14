# ミニゲーム集 改善作業計画書

## 1. 文書の位置付け

- Repository: `tetoyama/GameEngine`
- Branch: `game/minigame-collection`
- Pull request: `#48`
- Base branch: `refactor/ecs-scheduler-foundation`
- Base commit: `355b9ac450d2461bac3fffb49820d73d8b2e8f2e`
- 調査基準HEAD: `9626ef5a3356477c0c82d27619cad4e0539e63a5`
- 更新日: 2026-07-14

本書は初期実装計画ではなく、現在のコードを確認したうえで作成した差分計画である。すでに成立しているルール、Scene構成、演出基盤を作り直さず、初見理解、予告、BackShotの通路構造と終盤変化を追加する。

このブランチはECS / Scheduler / RenderWorld / Static Batch / RHI移行から分離可能な状態を維持する。ゲームコードからRenderPass、D3D11リソース、RHI、raw PhysX actorへ直接依存しない。

## 2. 状態表記

- `[完了]`: 現行コードと自動検証で確認済み
- `[部分]`: 基礎はあるが今回の要求を満たしていない
- `[未着手]`: 対応する実装が存在しない
- `[再設計]`: 現行実装を残しつつ、データ構造の拡張が必要
- `[手動確認]`: Windows実行環境で体感確認が必要

## 3. 現行実装の確認結果

### 3.1 共通基盤

| 項目 | 状態 | 現在確認できた内容 |
|---|---|---|
| Entry -> Persistent -> MiniGameのMulti-Scene構成 | [完了] | Entry Sceneのシリアライズ値からPersistentを安全なフレーム境界で追加ロードする |
| SceneToken単位の終了処理 | [完了] | 入力ロック、演出キャンセル、Rules shutdown、Scene unload、次Scene loadの順序がある |
| プレイヤー向けDirect2D UI | [完了] | Runtime UIをPlayerPassへ合成し、通常UIでImGuiを使わない |
| Countdown / Result / Retry / Next / Selection | [完了] | 3ゲーム共通フローが存在する |
| Result Menu | [完了] | 矢印キーで選択しSpaceで決定。初期位置は「もう一度」。Escapeを遷移に使わない |
| Audio / one-shot effect pool | [完了] | Persistent側に固定voice / effect poolがある |
| Camera shake / flash / HUD burst / bloom | [完了] | Presentation Service経由で利用可能 |
| 通常演出の密度抑制 | [部分] | 軽いScore演出の集約・間引きはあるが、通知優先度と予告の排他管理はない |
| 体感型操作説明 | [未着手] | 現在はヘッダーの説明文と3秒カウントダウンのみ |
| Briefing skip | [未着手] | 初回・再戦ともに説明を明示的に飛ばす共通操作がない |
| 共通イベント予告モデル | [未着手] | イベント発生前、発生、結果を状態として分離する共通機構はない |
| 初回説明済み状態 | [未着手] | 初回full、再戦compact、session内完了状態を管理していない |

### 3.2 COLOR TERRITORY

| 項目 | 状態 | 現在確認できた内容 |
|---|---|---|
| 11 x 7の床塗り、得点、CPU | [完了] | タイル所有権、順位、CPU判断が成立している |
| 境界での高速塗り替え対策 | [完了] | 候補保持時間、再塗りクールダウン、同時競合無効化がある |
| 後半20秒のItem Rush | [完了] | Bomb / Starを固定slotから決定論的に出現させる |
| Bomb | [完了] | 未取得時は3 x 3消去、取得済みは3 x 3塗装、範囲内Stun |
| Star | [完了] | 6秒間の速度上昇・無敵。接触妨害は1回で消費される |
| 落下予告 | [部分] | Falling phase、beam、bannerはあるが落下時間は0.9秒、bannerは約0.9秒で短い |
| 爆発予告 | [部分] | Fuseは3.4秒あるが、範囲・残り秒数・重大通知の専用表示がない |
| Item Rush開始予告 | [未着手] | 残り20秒で開始した後に演出する構造で、数秒前からの予告がない |
| 体感型説明 | [未着手] | 塗る、奪う、Bomb取得、爆発回避、Star取得を練習できない |

### 3.3 SHEEP ROUNDUP

| 項目 | 状態 | 現在確認できた内容 |
|---|---|---|
| 羊の操舵、壁回避、群れ、CPU | [完了] | 物理結果に依存しないcontrolled steeringで動作する |
| 無限補充 | [完了] | 24slotを再利用し、タイマー終了まで補充する |
| 金羊 | [完了] | 3点、金属色、強いEmissive、回転halo、CPU認識がある |
| 後半FLOCK RUSH | [完了] | 残り25秒から目標頭数を8から18へ上げ、補充間隔とbatchを増やす |
| FLOCK RUSHの事前予告 | [未着手] | `IsLateRush()`成立後に1.35秒bannerとHit演出を出している |
| 金羊出現予告 | [部分] | 出現後のspawn popと1.1秒bannerはあるが、出現地点を事前に示さない |
| 通常羊の情報整理 | [部分] | 通常spawnは弱い演出だが、後半batch時の発生領域を示す予告はない |
| 体感型説明 | [未着手] | 羊の反対側へ回り、囲いへ押し込む練習がない |

### 3.4 BACKSHOT

| 項目 | 状態 | 現在確認できた内容 |
|---|---|---|
| 直線スライド移動 | [完了] | 4方向を選ぶと壁、固定block、他player、予約経路の手前まで滑る |
| 滑走中の操作lock | [完了] | 方向転換と射撃を禁止し、停止後0.10秒のlanding lockを入れている |
| 前面 / 背面の可視化 | [完了] | 白い前面marker、赤い背面marker、滑走path表示がある |
| 背面射撃、Guard、Cooldown、CPU | [完了] | 既存BackshotRulesを維持し、CPUも同じ合法移動を使う |
| 通路tile topology | [再設計] | 現在は全面gridと固定blocked cell。直線、90度curve、T字、十字の接続情報を持たない |
| 複数layout | [未着手] | 11 x 7の固定配置1種類のみ |
| 一定時間速度上昇item | [未着手] | boost状態、item slot、出現予告、残り時間UIがない |
| おじゃまマス | [未着手] | 一時的な通路閉鎖、予告、解除、予約経路との整合がない |
| 体感型説明 | [未着手] | slide、停止方向、正面Guard、背面撃破を段階的に試せない |

## 4. 今回の改善目標

### 4.1 プレイヤー体験の基本順序

```text
ゲーム選択
  -> 体感型説明、またはEnter長押しでSkip
  -> READY
  -> 3秒Countdown
  -> 本番
  -> 長めの予告
  -> イベント発生
  -> 結果を短く表示
  -> Result Menu
```

説明、予告、発生演出を同じ瞬間に重ねない。

- 説明: 入力と因果関係を実際に試す
- 予告: 次に何が起こるか考える時間を与える
- 発生: 短く明確に結果を見せる
- 結果: 得点や状態変化を約1秒残す

### 4.2 演出方針

予告は派手さより可読性を優先する。

- 予告中は強い画面揺れを使わない
- 全画面flashを使わない
- 発生位置、範囲、残り時間、内容を表示する
- 穏やかなpulseと低頻度SEを使う
- 発生時だけ短いparticle、SE、必要最小限のshakeを使う
- 一度に表示する重大予告は1件まで

### 4.3 Briefing skip方針

Briefingは初回・再戦を問わずスキップ可能にする。

```text
ENTERを1.0秒長押し: Briefingをスキップ
```

SpaceはColor TerritoryやSheep Roundupでは将来のaction追加余地があり、BackShotでは射撃に使用するため、Briefing skipには使わない。

EnterはSelection画面のゲーム開始にも使うため、誤スキップ防止として`release-to-arm`を採用する。

1. MiniGame Sceneへ入った時点ではskip判定を無効にする。
2. Selectionで押していたEnterが一度離されたことを確認する。
3. その後のEnter長押しだけをskip進捗として数える。
4. 1.0秒未満で離した場合は進捗を0へ戻す。
5. 長押し中は画面上にゲージまたは円形progressを表示する。
6. skip成立後は説明用entity・score・状態をresetしてから`Ready`へ進む。
7. skipから直接`Playing`へ入らず、通常の3秒Countdownを必ず通す。
8. Escapeはアプリケーション終了と競合するため使用しない。

## 5. 目標アーキテクチャ

### 5.1 MiniGame phase

現行の各Runtimeの`m_started`分岐を段階的に共通phaseへ寄せる。

```cpp
enum class MiniGameRuntimePhase {
    Loading,
    Briefing,
    Ready,
    Countdown,
    Playing,
    Finishing,
    Result,
    Transition
};
```

一括置換は行わず、最初はBriefingを追加できる最小共通modelとして導入する。

### 5.2 体感型説明

追加候補:

```text
Core/MiniGameBriefingModel.h
Runtime/MiniGameBriefingPresenter.h
```

```cpp
struct BriefingStep {
    std::string prompt;
    BriefingInputMask allowedInput;
    float minimumDisplaySeconds;
    std::function<bool()> completionCondition;
    std::function<void()> beginAction;
    std::function<void()> resetAction;
};

struct BriefingSkipConfig {
    int keyCode = VK_RETURN;
    float holdSeconds = 1.0f;
    bool requireReleaseToArm = true;
};
```

設計条件:

1. 別の説明専用Sceneを量産せず、本番と同じMiniGame Sceneとstageを使う。
2. Briefing中は本番timer、正式score、通常CPUを進めない。
3. 各stepに不要な入力をlockする。
4. 成功したstepだけ次へ進む。
5. 失敗時は対象だけ安全な初期状態へ戻す。
6. Persistent側がGameIdごとの「このsessionで説明済み」を保持する。
7. 初回はfull briefing、retryはcompact briefingを標準表示する。
8. 初回・retryのどちらでもEnter長押しによるskipを許可する。
9. Selectionで使用したEnterの持ち越しを`release-to-arm`で無効化する。
10. skip時も説明用状態をcleanupし、通常Countdownへ接続する。

### 5.3 イベント予告

追加候補:

```text
Core/WorldEventTelegraphModel.h
Runtime/MiniGameTelegraphPresenter.h
```

```cpp
enum class TelegraphPhase {
    Warning,
    Armed,
    Resolving,
    Aftermath,
    Complete
};

struct TelegraphDescriptor {
    TelegraphId id;
    SceneToken sceneToken;
    TelegraphPriority priority;
    Vec2 worldPosition;
    TelegraphShape shape;
    float warningSeconds;
    float armedSeconds;
    float aftermathSeconds;
    std::string label;
};
```

設計条件:

1. Modelは純粋データで時間とphaseだけを管理する。
2. ゲーム固有Runtimeがresolve eventをconsumeし、Rules変更を実行する。
3. PresenterはDirect2Dと既存poolだけを使用する。
4. SceneTokenで全予告をcleanupする。
5. Majorは同時1件、Minorは同時2件まで表示する。
6. 通常Scoreや補充通知はMajor予告中に抑制または集約する。
7. 予告時間はgameplay時間に同期し、表示pulseとSEのみunscaled更新可能にする。
8. CPUも表示上の予告と同じ確定情報だけを使う。

## 6. 実装工程

### Phase 0: 現行baseline固定

状態: `[完了]`

- [x] 現在HEADでPortable Smoke Test成功
- [x] Windows Debug x64 full build成功
- [x] Result Menu、BackShot slide contractを自動検証
- [x] 本計画書を現行実装へ更新

完了条件:

- `9626ef5a...`を今回の差分基準として記録する。
- 既存3ゲームのルールを新機能追加前に変更しない。

### Phase 1: 共通Briefing基盤

状態: `[未着手]`

実装:

1. `MiniGameBriefingModel`をportable C++で追加する。
2. step開始、minimum display、成功、resetを状態機械化する。
3. `MiniGameBriefingPresenter`へEnter長押しskip状態を追加する。
4. `MiniGameRuntimeScriptBase`へ共通Briefing UI描画helperを追加する。
5. Persistent RuntimeへGameId別のsession内完了状態を追加する。
6. Briefing中はCountdownを開始せず、通常presentation commandを抑制する。
7. Briefing完了またはskip後に一度だけCountdownを開始する。
8. SelectionからのEnter持ち越しを防ぐrelease-to-armを実装する。

表示:

- 中央下部に現在の一文だけを表示する。
- 操作対象を明るくし、それ以外を軽く暗くする。
- 完了条件を満たした瞬間に小さなcheck演出を出す。
- 画面端へ`ENTER長押し：説明をスキップ`を常時表示する。
- Enter長押し中はskip progressを視覚化する。
- 文章だけで自動進行させない。

自動テスト:

- step順序
- minimum display前の誤完了防止
- reset後の再試行
- full / compact mode
- Enterの短いtapではskipしない
- Enter長押し1.0秒でskipする
- 長押し途中のreleaseでprogressがresetされる
- Scene開始時にEnterが押されたままでもskipしない
- release後の再長押しでのみskipする
- skip後にbriefing用状態がcleanupされる
- skip後もCountdownを通る
- SceneToken cleanup

### Phase 2: 共通Telegraph基盤

状態: `[未着手]`

実装:

1. `WorldEventTelegraphModel`をportable C++で追加する。
2. Warning -> Armed -> Resolving -> Aftermathを明示する。
3. priority queueと同時表示上限を追加する。
4. world marker、range、path、countdown textをDirect2D / pooled cubeで描画する。
5. 既存`SubmitPresentation`はresolve時と重大結果時だけ呼ぶ。
6. Major予告中の通常banner上書きを禁止する。

初期時間:

| Event | Warning | Armed / 発生直前 | Aftermath |
|---|---:|---:|---:|
| Bomb落下 | 2.8秒 | 0.35秒 | 0.8秒 |
| Bomb爆発 | 現行Fuse 3.4秒を全て表示 | 0.25秒 | 1.0秒 |
| Star落下 | 2.4秒 | 0.25秒 | 0.6秒 |
| FLOCK RUSH | 4.0秒 | 0.5秒 | 1.0秒 |
| 金羊出現 | 2.2秒 | 0.25秒 | 0.7秒 |
| BackShot Boost | 2.8秒 | 0.25秒 | 0.6秒 |
| おじゃまマス閉鎖 | 4.0秒 | 0.5秒 | 0.8秒 |

数値はInspectorまたはconfig structから調整可能にする。定数をRuntime各所へ分散させない。

### Phase 3: COLOR TERRITORY導入改善

状態: `[部分]`

既存利用:

- Tile rules、CPU、Bomb / Star rules、固定item slotは維持する。
- Falling visual、beam、haloをTelegraph Presenterへ接続する。

Briefing steps:

1. 矢印キーで指定された3tileを塗る。
2. 説明用CPUの色を1tile奪う。
3. Bombへ触れてowner色が変わることを確認する。
4. 表示された爆発範囲から出る。
5. Starを取得し、速度上昇を短時間体験する。

各step中もEnter長押しでBriefing全体をskipできる。Spaceはskipへ使用しない。

Telegraph変更:

- Item Rush開始3秒前に`ITEM RUSH IN 3`を開始する。
- Fallingを0.9秒から約2.4〜2.8秒へ延長する。
- Bombの3 x 3対象tileをFuse中ずっと示す。
- Fuse残り1秒からpulse頻度だけを上げる。
- Starは取得可能になる前から着地点を示す。
- Major予告中は通常paint bannerを出さない。

完了条件:

- 初見testerが説明なしでBomb取得後の結果を答えられる。
- 爆発前に安全範囲へ移動する時間がある。
- 通常塗りで重大予告が隠れない。

### Phase 4: SHEEP ROUNDUP導入改善

状態: `[部分]`

既存利用:

- 24slot pool、endless spawn、金羊3点、FLOCK RUSH設定を維持する。
- Sheep steeringとCPU targetingは変更しない。

Briefing steps:

1. 羊へ近づき、反対方向へ逃げることを確認する。
2. 羊の囲いと反対側へ移動する。
3. 説明用羊を自分の囲いへ入れる。
4. 通常羊が1点になることを表示する。
5. 説明用金羊を入れ、3点になることを表示する。

各step中もEnter長押しでBriefing全体をskipできる。

Telegraph変更:

- 残り29秒で4秒のFLOCK RUSH予告を開始し、残り25秒でRulesを切り替える。
- Rush予告中はstage周辺に複数の弱いspawn markerを出す。
- 金羊は生成2.2秒前に位置markerと`GOLDEN = 3`を出す。
- 通常羊は個別bannerを出さず、spawn位置へ0.6〜0.9秒の小さなringだけを出す。
- 後半batch spawnは1個ずつ通知せず、発生領域をまとめて示す。

完了条件:

- FLOCK RUSHの発生後ではなく、増加前に理解できる。
- 通常羊の大量補充でbannerが連続しない。
- 金羊の位置と3点価値を出現前から把握できる。

### Phase 5: BACKSHOT通路topology

状態: `[再設計]`

現行`BackshotSlideBoard`の直線停止計算とreserved path contractは残す。盤面データをblocked cellだけから、方向接続を持つroute cellへ拡張する。

追加するcell種別:

```text
Empty
StraightHorizontal
StraightVertical
CornerNE / CornerNW / CornerSE / CornerSW
TJunctionN / TJunctionE / TJunctionS / TJunctionW
Cross
DeadEndN / DeadEndE / DeadEndS / DeadEndW
Block
```

接続はenum名だけに依存せず、N / E / S / Wのbit maskを正とする。

移動規則案:

1. 入力方向に接続がなければ移動しない。
2. StraightとCornerは接続に沿って自動進行する。
3. T字とCrossは分岐点で停止する。
4. 壁、Block、他player、reserved pathの手前で停止する。
5. 停止時の最後の進行方向を正面にする。
6. 既存の射撃、Guard、rear eliminationを維持する。

最初に作るlayout:

- Layout A: 中央Crossと4本の直線
- Layout B: 90度Corner主体の周回路
- Layout C: T字分岐と短い袋小路

LayoutはRuntimeのhard-code配列ではなく、専用data structへ分離する。最初は決定論的なround indexで切り替え、random化は後工程とする。

自動テスト:

- Cornerで方向が90度変化する
- Junctionで停止する
- 接続されていない隣接cellへ進まない
- Blockとreserved routeの手前で止まる
- 同一layout / seedで結果が一致する

### Phase 6: BACKSHOT Boost item

状態: `[未着手]`

仕様:

- 一定時間の滑走速度上昇
- 初期値: 5.0秒
- 速度倍率: 1.4倍
- 停止位置、射撃判定、landing lockは変えない
- 効果時間中に複数回滑走できる
- 再取得時は残り時間を最大値まで延長し、倍率を重複させない

出現:

- 前半は出現させない。
- 中盤から1個、終盤は同時最大2個を候補とする。
- 固定poolを使い、play中にentityを増減させない。
- 2.8秒前から着地点を青いringで予告する。

表示:

- player本体に青い残像またはtrail
- path emissiveを増加
- HUDへ`BOOST 4.2s`
- 予告中は画面揺れなし

実装注意:

現行のslide durationは距離から計算し`0.18〜0.52秒`へclampしている。Boost時は距離と倍率からdurationを再計算し、短距離が視認不能にならないminimumを別configとして持つ。

### Phase 7: BACKSHOT おじゃまマス

状態: `[未着手]`

仕様:

- route cellを一定時間だけBlock扱いにする。
- 初期予告4.0秒、閉鎖5.0〜7.0秒、解除予告1.0秒。
- 現在playerがいるcell、現在reserved pathに含まれるcellは閉鎖候補から除外する。
- 予告開始後に新しいslideを予約する場合、閉鎖予定時刻より前に通過完了できる場合だけ許可する案を検証する。
- 最初の安全実装では、予告開始時点から新規経路計算上はBlockとして扱い、既存slideだけ完走させる。
- CPUと人間へ同じblocked topologyを渡す。

表示:

- Warning: 黄色の縞と残り秒数
- Armed: 橙色pulse
- Closed: 暗いBlockと赤い輪郭
- Reopen: 1秒前から緑色pulse

完了条件:

- 突然playerの目前に生成されない。
- 閉鎖でplayerが不正cellへ埋まらない。
- 全員が移動不能になるlayoutを生成しない。
- 予告を見て別routeを選べる時間がある。

### Phase 8: BACKSHOT体感型説明

状態: `[未着手]`

通路topology、Boost、おじゃまマスが確定した後に実装する。

Briefing steps:

1. 直線へ入力し、stopperまで滑る。
2. Cornerを通り、進行方向が変わることを確認する。
3. Cross / T字で停止し、次の方向を選ぶ。
4. 正面から説明用targetを撃ち、Guardされる。
5. 赤い背面側へ移動して撃破する。
6. Boostを取り、効果時間中に2回滑る。
7. おじゃまマスの予告を見て別routeを選ぶ。

BackShotは今回最もルール変更が大きいため、初回はfull briefingを標準表示する。ただし強制はせず、他ゲームと同じEnter長押しでいつでもskip可能にする。Spaceは射撃専用のまま維持する。

### Phase 9: 演出優先度と情報整理

状態: `[部分]`

- Major warningは常に通常score bannerより優先する。
- 同じ種類の通常eventは0.5秒単位で集約する。
- `Warning`, `Resolve`, `MajorResult`以外で強いshakeを使わない。
- 画面中央banner、右上status、world markerの役割を固定する。
- 同じ文言を複数箇所へ重複表示しない。
- Countdown中にgame固有eventを進めない。
- Briefing完了演出と本番Countdownを同じframeに開始しない。
- Skip成立時も強い演出を出さず、短い確認音と`SKIPPED`表示だけにする。

### Phase 10: 安定性・受け入れ確認

状態: `[手動確認]`

自動検証へ追加:

- Briefing model contract
- Briefing Enter-hold / release-to-arm contract
- Telegraph phase / priority contract
- Color item warning duration contract
- Sheep pre-rush timing contract
- Route topology contract
- Boost duration / non-stacking contract
- Temporary block safety contract
- Result Menu contract維持
- Full Windows Debug x64 build

必須手動sequence:

```text
Entry
 -> Color full briefing
 -> Color game
 -> Result / Retry compact briefingをEnter長押しでskip
 -> Next
 -> Sheep full briefingを途中でEnter長押しskip
 -> Sheep game
 -> Result
 -> Next
 -> BackShot full briefing
 -> BackShot game
 -> Result
 -> Return to selection
```

最低3周実施し、次を確認する。

- SelectionでEnterを押し続けてもBriefingが自動skipされない。
- Enterを一度離してから再長押しするとskipできる。
- 短いEnter tapではskipされない。
- Enter長押し中のprogressが分かる。
- skip後も3秒Countdownを通る。
- skip後に説明用entity、score、CPU状態が残らない。
- 以前のBriefing UI、Telegraph、world markerが残らない。
- Audio / effect voiceが残らない。
- CPUがBriefing中に通常行動しない。
- gameplay timerがBriefing中に減らない。
- Major warningが通常scoreに上書きされない。
- BoostとBlockがretry後に残らない。
- SceneToken変更後に古いresolve eventが発火しない。

初見tester確認:

各gameを事前説明なしで1回プレイしてもらい、終了後に以下を質問する。

- 何をすると得点または勝利になるか。
- 特殊eventの予告時に何が起こると思ったか。
- 予告を見て行動を変えられたか。
- 発生後に初めて意味を理解したeventがあったか。
- Briefingをskipしたい操作が明確だったか。
- 意図せずskipしてしまったことがなかったか。

「発生後に初めて理解した」が残る場合、派手さではなくBriefing stepまたはwarning durationを修正する。

## 7. 優先順位

1. 共通Briefing基盤とEnter長押しskip
2. 共通Telegraph基盤
3. COLOR TERRITORYへの適用
4. SHEEP ROUNDUPへの適用
5. BACKSHOT route topology
6. BACKSHOT Boost
7. BACKSHOT おじゃまマス
8. BACKSHOT Briefing
9. 演出優先度の全体調整
10. 初見testerによる反復確認

理由:

- 既存3ゲームのルールは成立している。
- 現在最大の問題は、ルール追加より「理解前にeventが発生する」ことにある。
- Briefingを任意skip可能にすることで、初見理解と再プレイ速度を両立する。
- 共通基盤を先に作ることで、3ゲームごとの一時的なbanner実装を増やさない。
- BackShotは盤面data構造の変更を伴うため、共通導入基盤の後に進める。

## 8. 今回変更しないもの

- Entry SceneをGameApplicationへhard-codeしない。
- Player-facing UIへImGuiを導入しない。
- game側からRender / D3D11 internalsへ直接アクセスしない。
- Color TerritoryのBomb / Star基本効果を変更しない。
- Sheepの24slot poolとendless scoringを廃止しない。
- BackShotの背面撃破、正面Guard、射撃Cooldownを変更しない。
- SpaceをBriefing skipへ割り当てない。
- EscapeをBriefingやゲーム内遷移へ割り当てない。
- 予告を強くするためだけに全画面flashや連続camera shakeを増やさない。

## 9. Completion gates

### 現行baseline

- [x] Entry / Persistent / additive MiniGame composition
- [x] Direct2D Runtime UI
- [x] Shared presentation pool
- [x] Shared Result Menu
- [x] Color Territory item rush
- [x] Sheep endless flock / golden sheep / late rush
- [x] BackShot straight slide foundation
- [x] Portable smoke tests green at baseline
- [x] Windows Debug x64 build green at baseline

### 今回の改善

- [ ] Common interactive Briefing model complete
- [ ] Enter-hold Briefing skip complete
- [ ] Release-to-arm and accidental-skip prevention complete
- [ ] Common Telegraph model complete
- [ ] Color full Briefing complete
- [ ] Color item / bomb telegraphs complete
- [ ] Sheep full Briefing complete
- [ ] Sheep FLOCK RUSH / golden telegraphs complete
- [ ] BackShot route topology complete
- [ ] BackShot three layouts complete
- [ ] BackShot timed Boost complete
- [ ] BackShot temporary blocker complete
- [ ] BackShot full Briefing complete
- [ ] Major / Minor presentation priority complete
- [ ] Three-cycle cleanup pass complete
- [ ] First-time-player comprehension pass complete
- [ ] Final Windows Debug x64 build and smoke tests green

## 10. Progress log

### 2026-07-14

- 現在HEAD `9626ef5a3356477c0c82d27619cad4e0539e63a5`を調査基準に設定した。
- Portable smoke suiteとWindows Debug x64 full buildの成功を確認した。
- 既存機能と未実装案を状態表へ分離した。
- 既存bannerは主に発生時または発生後の通知であり、共通Telegraphではないと判断した。
- Briefing、Telegraph、通路topology、Boost、おじゃまマスを今後の差分工程として整理した。
- Briefingは初回・再戦ともにEnter長押しでskip可能とする方針を確定した。
- SelectionのEnter入力持ち越しを防ぐため、release-to-armを必須契約へ追加した。
