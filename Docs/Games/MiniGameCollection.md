# MiniGame Collection

Status: **Implementation integrated / manual play validation remains**

## Games

現在の3ゲーム:

- Color Territory
- Sheep Roundup
- Backshot

## Scene composition

Entry SceneからPersistent Sceneと各MiniGame SceneをAdditive loadする。

Storage登録を伴うAdditive loadはSchedule中に直接行わず、

`QueueAdditiveSceneLoadFromFilePath()`
-> 次の安全なframe boundary
-> `ProcessQueuedAdditiveSceneLoads()`

の順で処理する。

## Shared runtime

共通基盤には次がある。

- game selection
- result menu
- scene transition
- briefing
- telegraph
- presentation event
- runtime UI
- scene-token based cleanup

## Runtime UI

Player-facing UIはEditor ImGuiと分離する。

MiniGame側はRuntime UI / Direct2D経路を使用し、PlayerPassで合成する。

## Result flow

Result MenuではRetry / Next Game / Return to Titleを扱う。

Scene transitionはpresentation cancel、rules shutdown、unload、loadを順序化する。

## Briefing

初見向けFullと、再試行向けCompactを分ける。

Briefing中は本番gameplay updateを一時停止し、説明overlayは継続できる。

## Telegraph

重大eventと軽微eventの表示優先度を共通modelで扱う。

Color Territory / Sheep Roundup / Backshot固有の予告は共通presentation境界へ接続する。

## Automatic validation

通常CIでは `MiniGameRulesSmokeTest` を代表契約として実行する。

過去に存在した大量のgrep-based architecture guardは恒久CIから外した。

詳細なmodel / forecast / telegraph / route topology testは必要時に手動実行できる。

## Manual runbook

重要な手動確認:

1. Entry -> selection -> game開始
2. Retry
3. Next Game
4. Return to Title
5. Briefing Full / Compact
6. Telegraph priority
7. Scene cleanup後に旧UI / eventが残らないこと
8. 3ゲーム連続遷移

旧Implementation Plan、Briefing資料、Runbook、Practice Isolation資料はArchiveへ統合済み。
