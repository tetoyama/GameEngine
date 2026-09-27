# AgentOS Phase実装計画（全Phase設計）

Status: Design v1 (2026-07-17)
凡例: [今回] = 本セッションで実装 / [次回] = 設計のみ確定し将来実装

## Phase 1: 静的な構造理解

| 項目 | 実装 | 状態 |
|---|---|---|
| SystemDescriptor | `EngineTools/SystemDescriptorExport` が `SystemRegistry::GetTasks()` からJSON生成 | [今回] |
| SystemAccess JSON出力 | 同上（reads/writes/domain/phase/priority/affinity） | [今回] |
| Execution Order出力 | `CompiledSystemSchedule` の依存辺をJSON化 | [今回] |
| Service Dependency Graph | `EngineContextBuilder::Build()` の登録順＋各Serviceの参照を静的表として出力 | [次回]（自動抽出は`Get<T>()`呼び出し点の計測が必要） |
| Entity/Component Schema | `ComponentRegistry::GetComponentIDToNameMap()`＋encode済YAMLキーからスキーマ推定 | [今回]（キー一覧まで） |
| YAML Validation | Scene/Prefabのスキーマ検査Tool | [次回] |

受け入れ試験: エンジン起動→`ExportSystemDescriptors` Command→`Logs/AgentOS/systems.json` が生成され、
TransformSystem等のRead/Write宣言が実コードと一致すること。

## Phase 2: 実行状態の観測

- Runtime Introspection: `ListWorlds/DescribeWorld/ListEntities/FindEntityByName/DescribeEntity/ReadComponent/ReadComponentField` [今回]
- Write Trace v1（frame diff＋Write宣言からの帰属推定） [今回]
- Write Trace v2（ComponentRegistry書込フックによる正確な帰属） [次回]
- Event Trace: 汎用イベントバスがエンジンに存在しないため、まずAgentOS用の軽量EventBus
  （Publish/Subscribe＋Trace記録）を新設し、既存システムから段階移行 [次回]
- State Diff / Profiler JSON出力: `SystemScheduleProfiler` スナップショットのJSON化 [次回]

## Phase 3: 再現と自動検証

- Fixed deltaTime / Seed固定 / Input Recording / Replay / Snapshot [次回]
  - 設計: `TimeService` に固定Δtモード、`InputService` に記録・再生レイヤを挿入。
    PhysXは完全決定論を保証せず、再現率＋最初の差異Frame＋差異Componentを報告する方式（構想§4.5）。
- Behavior Test Runner: `LoadTestScene/TriggerAction/RunForSeconds/Read<T>` のDSL [次回]
  - 現状のTests/はエンジン非リンクの単体スモークのため、ヘッドレスエンジン起動経路の整備が前提。
- Snapshot比較: Scene YAML保存を流用した `SaveSnapshot/CompareSnapshots` [次回]

## Phase 4: LLM-based OS（制御系）

すべて [今回] 実装（Core層、Linuxテスト済で納品）:
SQLite Task Store / Planner / Retrieval Workers / EvidenceBuilder / LogicBuilder(LogicGraph) /
ReasoningAgent / Critic（Rubric＋プログラム採点） / Evaluator / Repair / Synthesis /
Supervisor / Budget / Early Stopping / フラクタルTaskRuntime。

LLMバックエンド: `LlamaLlmBackend`（実機） [今回・VS検証待ち] / `MockLlmBackend`（テスト） [今回]。

## Phase 5: 安全なコード変更

- Capability Permission / Command Interceptor / Dry Run / Audit Log [今回]（パイプラインとして実装）
- ApplyCodePatch → Compile → Test → Rollback の実行系 [次回]
  - 設計: Git snapshot（`git stash create` 相当）→パッチ適用→`msbuild`起動→Tests実行→
    失敗時 `git checkout` で復元。Supervisorの Human Approval Gate を必須とする。
- 変更ファイル数上限・改善量下限などのRepair制約 [今回]（Budget構造体に含む）

## Phase 6: 自律型ゲーム開発

[次回] Task DAG継続実行の常駐化 / Performance Regression検出（Profiler比較）/
Visual Test（CaptureFrame差分）/ Editor Command API（GUI操作のCommand化。
`CommandManager`(Undo/Redo)と統合）/ 自動ドキュメント更新 / Human Approval Gateの運用UI。

## 垂直スライス（構想§14）— 本セッションの受け入れ基準

ユースケース: **特定EntityのComponent値が異常な理由を調査し、原因Systemを特定する**

```text
ユーザー入力 → IntakeAgent → PlannerAgent(Task DAG)
→ RuntimeWorker(DescribeEntity/WriteTrace) + CodeSearchWorker(SystemDescriptor検索)
→ EvidenceBuilder → ReasoningAgent → CriticAgent → (必要ならRepair)
→ SynthesisAgent → 報告
```

- Linux上: MockLlm＋FakeEngineToolsでE2Eスモークテストが緑になること。
- Windows上: AgentOSPanelから同フローを実LLM（Qwen gguf）＋実エンジンToolで起動できること（VS検証）。

## ビルド統合

- vcxproj/filtersへ追加: `Backends/sqlite/sqlite3.c`（`/W4 /WX`対象外にするため個別に警告レベルを下げる）、
  `AgentOS/**`、`Tests/AgentOS*` はビルド対象外（エンジン規約どおり）。
- nlohmann/json: `Source/GameApplication/Backends/llama/vendor` をAgentOSファイルのincludeパスに使用。
- Linux: `Tests/AgentOS/Makefile`（g++ -std=c++20、sqlite3.cはcc）。
