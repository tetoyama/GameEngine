# MiniGame Collection Phase 1: Common Briefing Foundation

## Status

`[完了]` 2026-07-14

本書は`Docs/MiniGame_Collection_Implementation_Plan.md`のPhase 1について、実装済み範囲と次工程との境界を記録する。

## 完了した実装

### Portable briefing model

追加:

```text
Source/GameApplication/Game/MiniGameCollection/Core/MiniGameBriefingModel.h
```

実装内容:

- `Full` / `Compact` briefing mode
- `Inactive` / `AwaitingSkipRelease` / `StepActive` / `Ready` / `Complete` phase
- step開始、minimum display、成功、reset、次step遷移
- Compact modeで省略するstepの選別
- skip進捗とstep進捗の取得
- `Clear()`によるScene終了時の完全reset
- ReadyとCompleteの分離

### Enter hold skip

仕様:

```text
ENTERを1.0秒長押し: Briefingをスキップ
```

- Spaceはゲーム操作と競合するため使用しない
- Selectionで使用したEnterの押しっぱなしを受け付けない`release-to-arm`
- Enterを一度離した後の長押しだけを計測する
- 1.0秒未満で離した場合はskip進捗を0へ戻す
- skip成立後は`Ready`へ進み、直接`Playing`へ入らない
- 通常の3秒Countdownを開始する責務はゲームRuntime側に残す

### Direct2D presenter

追加:

```text
Source/GameApplication/Game/MiniGameCollection/Runtime/MiniGameBriefingPresenter.h
Source/GameApplication/Game/MiniGameCollection/Runtime/MiniGameBriefingPresenter.cpp
```

表示内容:

- 背景を軽く暗くする
- 現在のstep番号と一文だけを表示する
- Enter release待ちを表示する
- Enter長押し進捗barを表示する
- 通常完了とskip完了を区別して表示する
- `Ready`表示後にCountdownへ移れる構造にする

Presenterは`MiniGameRuntimeUi`を使用し、player-facing ImGuiを追加しない。

### Session briefing state

`MiniGameCollectionManagerModel`へ次を追加した。

- GameId別の説明完了状態
- 初回未完了時は`Full`
- retryまたはsession内完了済みなら`Compact`
- 明示的な完了記録とreset

Persistent Runtimeが所有するManager内に状態を置くため、MiniGame Sceneのunload後も同一session内では説明済み状態を維持できる。

## 自動検証

`Tests/MiniGameCollectionCoreSmokeTest.cpp`へ次を追加した。

- step順序
- minimum display前の誤完了防止
- step reset
- Full / Compact step選別
- Enter押しっぱなしによる誤skip防止
- 短いEnter入力での進捗reset
- 1.0秒長押しによるskip
- skip後にReadyで止まり、Completeへ直行しないこと
- GameId別session完了状態
- briefing cleanup

加えて、Direct2D PresenterをGameEngineの独立した翻訳単位としてWindows Debug x64でコンパイルしている。

## Windows build修正

独立翻訳単位での検証により、Windows.hの`min` / `max` macroが`std::min` / `std::max`へ干渉する既存問題を検出した。

次をmacro-safeな`(std::min)` / `(std::max)`形式へ修正した。

```text
Source/GameApplication/Game/MiniGameCollection/Runtime/MiniGameRuntimeUi.h
Source/GameApplication/Game/MiniGameCollection/Runtime/MiniGameBriefingPresenter.h
```

Presenter翻訳単位はPCHへ依存しないよう`PrecompiledHeader=NotUsing`を明示した。

## 今回まだ接続しないもの

以下は共通基盤ではなくゲーム固有導入工程で実装する。

- Color Territoryの塗る、奪う、Bomb、Star briefing steps
- Sheep Roundupの回り込み、追い込み、通常羊、金羊 briefing steps
- BackShotのroute topology、Guard、背面撃破、Boost、blocker briefing steps
- 各ゲームRuntimeでの本番timer / score / CPU停止
- briefing完了またはskip後のCountdown開始
- 説明用entityの生成とstep単位reset

ゲーム固有導入前のため、現時点の実プレイ開始フローは従来通りであり、Briefing画面はまだ表示されない。

## Validation gate

- [x] Portable C++20 smoke tests
- [x] Existing mini-game rule smoke tests
- [x] Result menu contract維持
- [x] BackShot slide contract維持
- [x] Direct2D / PlayerPass boundary維持
- [x] Windows Debug x64 full solution build
- [ ] Color Territory briefing integration
- [ ] Sheep Roundup briefing integration
- [ ] BackShot briefing integration
