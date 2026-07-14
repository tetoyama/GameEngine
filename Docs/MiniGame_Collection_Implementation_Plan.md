# ミニゲーム集 改善作業計画書

## 1. 文書の位置付け

- Repository: `tetoyama/GameEngine`
- Branch: `game/minigame-collection`
- Pull request: `#48`
- Base branch: `refactor/ecs-scheduler-foundation`
- Base commit: `355b9ac450d2461bac3fffb49820d73d8b2e8f2e`
- 初期調査基準HEAD: `9626ef5a3356477c0c82d27619cad4e0539e63a5`
- 実装完了HEAD: `79566919ad9475151a6ce047d3e0118af9650a0a`
- 更新日: 2026-07-14

本書は現行コードを基準にした差分計画として作成し、Phase 0〜10の実装と自動検証を完了した。残る作業はWindows実行環境での手動プレイ確認と初見プレイヤーによる理解度確認である。

## 2. 状態表記

- `[完了]`: 実装と自動検証が完了
- `[手動確認]`: 実装済みで、実行環境による体感確認が必要

## 3. 最終実装状態

### 3.1 共通基盤

| 項目 | 状態 | 実装内容 |
|---|---|---|
| Entry -> Persistent -> MiniGameのMulti-Scene構成 | [完了] | Entryのシリアライズ値からPersistentとMiniGame Sceneを安全なフレーム境界で追加ロードする |
| SceneToken単位の終了処理 | [完了] | 入力lock、演出cancel、Rules shutdown、Scene unload、次Scene loadを順序化 |
| Direct2D Runtime UI | [完了] | PlayerPass合成。プレイヤー向けImGuiは使用しない |
| Result Menu | [完了] | 矢印キーで選択、Spaceで決定。Escapeはアプリケーション終了専用 |
| 共通Briefing | [完了] | Full / Compact、step成功、reset、Ready、cleanup、session内説明済み管理 |
| Briefing skip | [完了] | Enter 1.0秒長押し、release-to-arm、短押しreset、skip後もReadyと3秒Countdownを通る |
| Briefing中の停止 | [完了] | 本番RuntimeのFrame / Fixed更新をsuspendし、説明Overlayのみ継続する |
| 共通Telegraph | [完了] | Pending / Warning / Armed / Resolving / Aftermath / Complete |
| 演出優先度 | [完了] | Major同時1件、Minor同時2件。Major中は通常Score / NearMissを抑制・集約 |
| Scene cleanup | [完了] | Briefing、Telegraph、Presentation、TransitionをSceneToken単位で破棄 |

### 3.2 COLOR TERRITORY

| 項目 | 状態 | 実装内容 |
|---|---|---|
| 基本ルール、CPU、Bomb / Star | [完了] | 既存効果を維持 |
| Full Briefing | [完了] | 塗る、奪う、Bomb取得、爆発範囲回避、Star加速を体感する |
| Item Rush予告 | [完了] | 開始3秒前から`ITEM RUSH IN 3` |
| Bomb落下予告 | [完了] | 2.8秒前から着地点を表示 |
| Star落下予告 | [完了] | 2.4秒前から着地点を表示 |
| Bomb Fuse予告 | [完了] | Fuse中に3 x 3対象範囲と残り時間を表示 |
| 情報優先度 | [完了] | 重大予告中は通常paint通知で上書きしない |

### 3.3 SHEEP ROUNDUP

| 項目 | 状態 | 実装内容 |
|---|---|---|
| 24slot pool、endless spawn、金羊3点 | [完了] | 既存ルールを維持 |
| Full Briefing | [完了] | 羊の逃げ方、回り込み、囲い、通常1点、金羊3点を体感する |
| FLOCK RUSH予告 | [完了] | 発生4秒前から予告し、発生領域をまとめて示す |
| 金羊予告 | [完了] | 2.2秒前から位置と`3 POINTS`を表示 |
| 通常補充通知 | [完了] | 個別bannerを避け、Minorな発生領域表示へ集約 |

### 3.4 BACKSHOT

| 項目 | 状態 | 実装内容 |
|---|---|---|
| Route topology | [完了] | N / E / S / W接続bit maskを正とする |
| Cell種別 | [完了] | Straight、Corner、T字、Cross、DeadEnd、Empty、Block |
| 移動 | [完了] | Corner自動旋回、T字 / Cross停止、Block / player / reserved route手前停止 |
| 3 Layout | [完了] | Layout A / B / Cを決定論的round indexで切替 |
| Boost | [完了] | 5.0秒、1.4倍、再取得は延長、倍率非重複、固定2slot |
| Boost予告 | [完了] | 2.8秒前から青いring |
| おじゃまマス | [完了] | 4秒予告、6秒閉鎖、1秒再開予告 |
| Block安全性 | [完了] | occupied / reserved除外、接続性維持、Warning開始から新規routeをblock |
| Full Briefing | [完了] | 直線、Corner、分岐、正面Guard、背面撃破、Boost、Blocker回避 |
| facing整合 | [完了] | route用`facing`を明示し、連続移動では`forward`と同期する |

## 4. プレイヤー体験の最終順序

```text
ゲーム選択
  -> Full / Compact体感型Briefing
  -> Enter長押しskip可能
  -> READY
  -> 3秒Countdown
  -> 本番
  -> 長時間Telegraph
  -> Event resolve
  -> Aftermath
  -> Result Menu
```

説明、予告、発生演出は同じ瞬間へ重ねない。

## 5. Briefing仕様

```text
ENTERを1.0秒長押し: Briefingをスキップ
```

- Selectionで押していたEnterを一度離すまでskipをarmしない。
- 1.0秒未満で離した場合はprogressを0へ戻す。
- Briefing中は本番timer、正式score、通常CPUを進めない。
- 初回未完了はFull、retryまたはsession内説明済みはCompact。
- skip成立後も直接Playingへ入らず、Readyと3秒Countdownを通す。
- SpaceはBackshot射撃などと競合するためskipに使用しない。
- Escapeはアプリケーション終了専用のまま維持する。

## 6. Telegraph仕様

```text
Pending -> Warning -> Armed -> Resolving -> Aftermath -> Complete
```

- Modelはportableな純粋データとして時間とphaseを管理する。
- ゲーム固有RuntimeがResolve eventをconsumeしてRules変更を実行する。
- PresenterはDirect2Dと既存poolを使用する。
- SceneToken変更時に旧予告と旧Resolve eventを破棄する。
- Major同時1件、Minor同時2件。
- Major warning中は通常Score / NearMissを抑制または集約する。
- 予告中は強いflashや連続shakeを使用しない。

| Event | Warning | Armed / 発生直前 | Aftermath |
|---|---:|---:|---:|
| Bomb落下 | 2.8秒 | 0.35秒 | 0.8秒 |
| Bomb爆発 | Fuse 3.4秒 | 0.25秒 | 1.0秒 |
| Star落下 | 2.4秒 | 0.25秒 | 0.6秒 |
| FLOCK RUSH | 4.0秒 | 0.5秒 | 1.0秒 |
| 金羊出現 | 2.2秒 | 0.25秒 | 0.7秒 |
| Backshot Boost | 2.8秒 | 0.25秒 | 0.6秒 |
| おじゃまマス閉鎖 | 4.0秒 | 0.5秒 | 0.8秒 |

## 7. 自動検証

実装完了HEAD `79566919ad9475151a6ce047d3e0118af9650a0a` で、`MiniGame Collection Validation` run `29346206737`が成功した。

成功項目:

- [x] Runtime UI boundary
- [x] Result Menu controls
- [x] Interactive Briefing flow
- [x] Shared Telegraph architecture
- [x] Color / Sheep predictive warning contracts
- [x] Route-based Backshot contract
- [x] PlayerPass Runtime UI composition
- [x] Scene-driven Entry flow
- [x] Presentation stack
- [x] Deferred additive loading
- [x] Portable MiniGame model tests
- [x] Telegraph phase / priority tests
- [x] Exact forecast timing tests
- [x] Presentation retry tests
- [x] MiniGame rule flow tests
- [x] Legacy Backshot slide tests
- [x] Route topology / Boost / blocker tests
- [x] Required Scene asset validation
- [x] Windows Debug x64 full solution build

## 8. 手動受け入れ確認

状態: `[手動確認]`

実装と自動検証は完了した。以下はWindows実行環境でユーザーが確認する。

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
  -> Backshot full briefing
  -> Backshot game
  -> Result
  -> Return to selection
```

最低3周で確認する項目:

- SelectionでEnterを押し続けてもBriefingが自動skipされない。
- Enterを一度離してから再長押しした場合だけskipできる。
- 短いEnter tapではskipされず、progressがresetされる。
- skip後も3秒Countdownを通る。
- Briefing中にtimer、score、通常CPUが進まない。
- retry / Next / Selection復帰後に説明用entityやUIが残らない。
- Major warningが通常Score通知に上書きされない。
- Boost / blocker / Telegraphがretry後に残らない。
- SceneToken変更後に古いResolve eventが発火しない。
- ColorのBomb / Star / Item Rushを発生前に理解できる。
- SheepのFLOCK RUSHと金羊3点を発生前に理解できる。
- Backshotのroute、Guard、背面、Boost、blockerを本番前に理解できる。

詳細手順は`Asset/Game/MiniGameCollection/FINAL_VALIDATION.md`を参照する。

## 9. Completion gates

### 実装・自動検証

- [x] Common interactive Briefing model complete
- [x] Enter-hold Briefing skip complete
- [x] Release-to-arm and accidental-skip prevention complete
- [x] Common Telegraph model complete
- [x] Color full Briefing complete
- [x] Color item / Bomb telegraphs complete
- [x] Sheep full Briefing complete
- [x] Sheep FLOCK RUSH / golden telegraphs complete
- [x] Backshot route topology complete
- [x] Backshot three layouts complete
- [x] Backshot timed Boost complete
- [x] Backshot temporary blocker complete
- [x] Backshot full Briefing complete
- [x] Major / Minor presentation priority complete
- [x] SceneToken cleanup contracts complete
- [x] Final Windows Debug x64 build and smoke tests green

### 実行環境確認

- [ ] Three-cycle manual cleanup pass
- [ ] First-time-player comprehension pass

## 10. 変更しない方針

- Entry SceneをGameApplicationへhard-codeしない。
- Player-facing UIへImGuiを導入しない。
- ゲーム側からRender / D3D11 / RHI internalsへ直接アクセスしない。
- Color TerritoryのBomb / Star基本効果を変更しない。
- Sheepの24slot poolとendless scoringを廃止しない。
- Backshotの背面撃破、正面Guard、射撃Cooldownを変更しない。
- SpaceをBriefing skipへ割り当てない。
- EscapeをBriefingやゲーム内遷移へ割り当てない。
- 予告を強くする目的だけで全画面flashや連続camera shakeを増やさない。
