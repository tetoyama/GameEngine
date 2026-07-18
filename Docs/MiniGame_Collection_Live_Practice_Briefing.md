# MiniGame Collection: Live Practice Briefing

## Status

`[実装完了]` 2026-07-15

## 目的

従来のBriefingは、ゲーム本体を停止し、説明専用cubeで小さな課題を1項目ずつ達成させる構成だった。
この方式では操作とルールを個別には確認できる一方、本番で同時に起きるCPU、得点変動、アイテム、予告、時間制限の関係を体験しにくかった。

Briefingを、任天堂系パーティゲームのルール画面に近い「ルールを表示しながら本番相当のゲームを自由に試す」形式へ変更した。

## 新しいプレイヤーフロー

```text
ゲーム選択
  -> PRACTICEルールカード表示
  -> 本番と同じフィールド、CPU、timer、item、telegraphで自由練習
  -> ENTERを一度離す
  -> ENTERを1.0秒長押し
  -> 同じMiniGame Sceneを再ロード
  -> 練習中の状態を完全破棄
  -> 通常の3秒Countdown
  -> 本番
```

## 実装契約

### Full-game practice

- Briefing中にScene updateをsuspendしない。
- MiniGame固有Runtimeを通常通り更新する。
- プレイヤー入力、CPU、timer、score、item、telegraphをすべて動かす。
- 説明専用entityとstep単位resetは使用しない。

### UI

- 実ゲームを隠さない低濃度の背景暗転にする。
- 上部に目的、操作、重要ルール3項目を固定表示する。
- 下部に自由練習中であることと、本番開始操作を表示する。
- step番号や達成課題は表示しない。

### 本番開始

- ENTERはselectionからの押しっぱなしを受け付けないrelease-to-armを維持する。
- ENTERを1.0秒長押しするとReadyへ進む。
- Ready後は同じMiniGame SceneへRetry transitionを送る。
- 再ロード先ではone-shot briefing bypassを消費し、Briefingを再表示しない。
- score、timer、player位置、CPU状態、item、random stateはScene再生成によって初期化する。

### 安全性

- Guidance中にゲーム本体から送られたRetry、Next、Selection transitionは拒否する。
- Practice用のBeginSceneとCountdown presentationは許可する。
- 通常のScore、Hit、Result presentationはGuidance中に抑制し、説明UIの可読性を守る。
- 同一frameでtransition処理が進んでも安全なよう、one-shot bypassはtransition要求前にarmする。
- transition要求が失敗した場合はbypassを取り消し、Practiceへ復帰する。

## ゲーム別表示

### COLOR TERRITORY

- 目的: 塗ったマスが最も多いプレイヤーを目指す。
- 相手色を奪う得点差。
- BOMBの取得と3x3爆発範囲。
- STARの加速、無敵、接触硬直。

### SHEEP ROUNDUP

- 目的: 羊を自分の囲いへ追い込む。
- 羊がプレイヤーの反対方向へ逃げるルール。
- 回り込みの位置関係。
- 通常羊1点、金羊3点。

### BACKSHOT

- 目的: routeを滑り、相手の赤い背面を撃つ。
- 正面Guardと背面撃破。
- Corner、T字、Crossの移動規則。
- Boostとroute閉鎖予告。

## 手動確認項目

- [ ] Practice中にプレイヤー、CPU、timer、scoreが動く。
- [ ] ColorでBOMBとSTARがPractice中にも発生する。
- [ ] Sheepで通常羊と金羊を実際に囲いへ入れられる。
- [ ] BackshotでGuard、背面撃破、Boost、blockerを実際に試せる。
- [ ] SelectionでENTERを押したままでも即開始しない。
- [ ] ENTERを離してから1.0秒長押しすると本番へ進む。
- [ ] Practice終了時に同じSceneが再ロードされる。
- [ ] 本番開始時に得点、timer、配置、itemが初期状態へ戻る。
- [ ] 本番側ではBriefingが再表示されず、通常Countdownが始まる。
- [ ] Practiceを最後まで遊び切ってもResult Menuから遷移できない。
- [ ] transition失敗時にPractice UIと入力が復帰する。
