# MiniGame Practice Isolation

## 目的

Briefing中の練習を、通常Matchを単に閉じ込めた状態ではなく、明示的なPractice実行コンテキストとして扱う。

## Practiceで維持するもの

- プレイヤー操作
- CPU操作
- 得点処理
- ヒット処理
- アイテム
- Telegraph
- Camera Shake
- Score / Hit / NearMissのPresentation

練習中もゲーム固有の手触りと危険予告は本番と同じ経路を通す。

## Practiceから隔離するもの

- 公式Result Presentation
- Success / Failure Presentation
- Result menuからのRetry / Next / Title遷移
- 練習結果の記録
- 練習中の配置、得点、タイマー、乱数状態の本番への持ち越し

## ラウンド終了

Game RuntimeがResultへ到達すると、MailboxはResult commandをPersistent Presentationへ渡さない。代わりにPractice Overlayへラウンド終了を通知する。

Overlayは以下を行う。

1. Result UIを覆う`PRACTICE / RESETTING ROUND`表示を出す。
2. 0.55秒後、Practice専用遷移で同じSceneを再ロードする。
3. Briefing Bypassは設定しないため、再ロード後もPracticeを継続する。

## 本番開始

Enter長押しが成立した場合だけ以下を行う。

1. Practice contextを終了する。
2. Briefing完了を記録する。
3. 1回限りのBriefing Bypassを設定する。
4. 同じSceneを再ロードする。
5. 再ロード後は通常Matchとして開始する。

この再ロードにより、Practiceの得点、残り時間、位置、Item、CPU decision clock、乱数状態を破棄する。

## 状態所有者

`MiniGameRuntimeMailbox`がSceneToken単位で以下を所有する。

- Practice active
- Practice round finished
- Guidance active
- Major telegraph active
- Pending transition

PracticeとGuidanceは別集合とし、PracticeでScore/Hit Presentationが消えないようにする。

## 検証項目

- Practice中にScore/Hit演出が出る。
- Practice終了時に公式Result panelと勝敗音が見えない。
- Practice終了後は自動的に同じ練習へ戻る。
- Practice中にResult menu操作をしても遷移しない。
- Enter長押し後だけ本番へ移る。
- 本番開始時に練習得点、位置、残り時間が初期化されている。
- 本番ResultではRetry / Next / Titleが通常どおり動く。
