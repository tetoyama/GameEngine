# GameEngine Documentation

Status: **Canonical index — 2026-09-27**

このディレクトリは、現在のGameEngineを理解するための文書と、過去の実装記録を分離して管理する。

## 文書の優先順位

矛盾がある場合は、次の順で信頼する。

1. `develop` の現在のソースコードと自動テスト
2. このIndexから辿れるCanonical文書
3. `Docs/Design/` の将来設計
4. `Docs/Archive/` の過去記録

Archive内の文書は当時の判断や実装経緯を残すためのもので、現在仕様を保証しない。

## Canonical

### 全体
- [Project Vision / Roadmap](Project_Vision_Robocraft_Roadmap.md)
- [Engine Overview](Architecture/Engine_Overview.md)
- [Testing and Branch Policy](Testing_Policy.md)

### Engine Architecture
- [ECS and Scheduling](Architecture/ECS_and_Scheduling.md)
- [Rendering](Architecture/Rendering.md)
- [Scripting and Physics](Architecture/Scripting_and_Physics.md)
- [RenderPipeline Graph Design](Architecture/RenderPipelineGraph_Design.md)

### Editor / AI
- [Modern ImGui Editor](Editor/ModernImGui.md)
- [AgentOS / B.R.A.I.N.](AgentOS/README.md)

### Game projects
- [ElemenTactics](Games/ElemenTactics.md)
- [MiniGame Collection](Games/MiniGameCollection.md)
- [Platformer Tech Demo](Games/PlatformerTechDemo.md)

## Active design proposals

現在コードへ完全には反映されていない将来設計は `Docs/Design/` に置く。

- [Render Observability / AgentOS](Design/Render_Observability_AgentOS.md)
- [Occluded Silhouette / RenderGraph](Design/Occluded_Silhouette.md)

Design文書を現在実装の説明として扱わないこと。

## Archive

`Docs/Archive/` には、旧Migration Plan、Step単位のCompletion Report、過去の設計追補、ゲーム制作中の途中計画を置く。

詳細は [Archive index](Archive/README.md) を参照。

## 更新ルール

- 現在仕様が変わった場合は、まず該当Canonical文書を更新する。
- 一時的な実装計画や調査メモをCanonicalへ混ぜない。
- Step単位の作業記録を新規に作る場合は、完了後にArchiveへ移す。
- 同じ機能について Plan / Progress / Completion を別々に恒久保持しない。
- READMEには概要だけを書き、細部はCanonical文書へ寄せる。
