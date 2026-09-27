# AgentOS / B.R.A.I.N.

Status: **Canonical current overview — 2026-09-27**

## Position

AgentOSはGameEngine内で動くLLM Agent基盤。

Editor上では `AgentOSPanel` をB.R.A.I.N.として表示する。

旧BRAIN UIを別系統で増やすのではなく、AgentOSへ統合する方針。

## Core

主要構成:

- Intake / Planner / Reasoning / Critic / Synthesis
- Orchestrator
- Command Pipeline
- Capability / Permission
- Evidence
- Logic graph
- Budget / Early stopping
- TaskStore
- Code Index / Search

## Orchestrator

`Orchestrator::RunSession()` がrequest単位の実行入口。

LLM出力だけを信頼せず、Tool resultとEvidenceを分離して扱う。

repair / recursive flowを持ち、Critic結果に応じて追加調査や修復を行える。

## Engine integration

`AgentOSService` がEngine Service境界。

主な責務:

- user request受付
- worker lifecycle
- LLM backend
- main-thread bridge
- code index
- progress / transcript
- UI向けstate snapshot

Engine Entity / Component / Systemへの操作はTool境界から行う。

## LLM backend

現在はLLAMA Serviceと接続できる。

GPU layer設定やbackend availabilityもServiceから扱う。

## Code investigation

Code Index / Code SearchをCore機能として持つ。

Repository調査時は、会話だけで推測するのではなくIndex / Tool evidenceを使う。

## UI

Editor上の表示名はB.R.A.I.N.。

Window menuから再表示でき、Modern ImGuiのEditor UIと共存する。

## Tests

通常CIでは代表的な8本をCore gateとして実行する。

Full regression suiteは `Tests/AgentOS/Makefile` の `full` targetで手動実行できる。

詳細は [Testing Policy](../Testing_Policy.md)。

## Historical docs

旧Architecture draft、Phase Plan、VS integration、Chat UI、Execution roadmap、GPU setup資料は `Docs/Archive/AgentOS/` へ移動した。

現在仕様は本書と現在コードを正とする。
