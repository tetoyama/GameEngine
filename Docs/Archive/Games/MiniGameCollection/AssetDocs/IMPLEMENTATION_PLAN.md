# ミニゲーム集 改善作業計画書

## 状態

実装・自動検証: `[完了]`

手動プレイ確認: `[手動確認]`

- Repository: `tetoyama/GameEngine`
- Branch: `game/minigame-collection`
- Pull request: `#48`
- Base branch: `refactor/ecs-scheduler-foundation`
- 初期調査基準HEAD: `9626ef5a3356477c0c82d27619cad4e0539e63a5`
- 実装完了基準HEAD: `79566919ad9475151a6ce047d3e0118af9650a0a`
- 更新日: 2026-07-14

詳細な最終計画・実装状態:

```text
Docs/MiniGame_Collection_Implementation_Plan.md
```

手動検証手順:

```text
Asset/Game/MiniGameCollection/FINAL_VALIDATION.md
```

## 完了した工程

- [x] Phase 0: baseline固定
- [x] Phase 1: 共通Briefing基盤
- [x] Enter 1.0秒長押しskip
- [x] release-to-arm誤skip防止
- [x] Full / Compact briefing
- [x] Briefing中のtimer / score / CPU停止
- [x] Phase 2: 共通Telegraph基盤
- [x] Warning / Armed / Resolving / Aftermath
- [x] Major同時1件 / Minor同時2件
- [x] Major warning中の通常通知抑制
- [x] Phase 3: Color Territory full briefing
- [x] Item Rush 3秒前予告
- [x] Bomb 2.8秒 / Star 2.4秒落下予告
- [x] Bomb 3 x 3 Fuse範囲予告
- [x] Phase 4: Sheep Roundup full briefing
- [x] FLOCK RUSH 4秒前予告
- [x] 金羊2.2秒前・3点予告
- [x] Phase 5: Backshot route topology
- [x] Straight / Corner / T字 / Cross / DeadEnd / Block
- [x] 決定論的Layout A / B / C
- [x] Corner自動旋回 / Junction停止
- [x] Phase 6: Backshot Boost
- [x] 5.0秒、1.4倍、固定2slot、2.8秒前予告
- [x] Phase 7: Backshot temporary blocker
- [x] 4秒予告、6秒閉鎖、1秒再開予告
- [x] occupied / reserved除外、接続性安全確認
- [x] Phase 8: Backshot full briefing
- [x] Phase 9: 演出優先度・情報整理
- [x] SceneToken cleanup契約
- [x] Phase 10: Portable Smoke / Windows Debug x64 build

## 自動検証項目

`MiniGame Collection Validation`で以下を検証する。

- Runtime UI boundary
- Result Menu controls
- Interactive Briefing flow
- Shared Telegraph architecture
- Color / Sheep predictive warning contracts
- Route-based Backshot contract
- PlayerPass Runtime UI composition
- Scene-driven Entry flow
- Presentation stack
- Deferred additive loading
- Portable MiniGame model tests
- Telegraph lifecycle / priority tests
- Exact forecast timing tests
- Presentation retry tests
- MiniGame rule flow tests
- Legacy Backshot slide tests
- Route topology / Boost / blocker tests
- Required Scene asset validation
- Windows Debug x64 full solution build

## 手動確認

ユーザーによるWindows実行確認では、`FINAL_VALIDATION.md`のsequenceを最低3周する。

重点確認:

- Selection Enterの持ち越しでBriefingがskipされない
- skip後も3秒Countdownを通る
- Briefing中に本番timer / score / CPUが進まない
- retry / Scene変更後にBriefing、Telegraph、Boost、blockerが残らない
- Major warningが通常Score通知に上書きされない
- Color / Sheep / Backshotの特殊ルールを発生前に理解できる

## 維持する制約

- EntryをGameApplicationへhard-codeしない
- Player-facing ImGuiを追加しない
- ゲーム側からRender / D3D11 / RHI internalsへ直接アクセスしない
- SpaceをBriefing skipへ使わない
- Escapeをゲーム内遷移へ使わない
- 強い全画面flashや連続camera shakeで予告を代替しない
