# AgentOS Architecture — LLM-based OS × GameEngine 統合基盤

Status: Design v1 (2026-07-17)
対象: 本リポジトリの自作エンジン（ECS / SystemScheduler / yaml-cpp / llama.cpp 組込）

本書は構想ドキュメント「LLM-based OSとゲームエンジンを統合した自律型ゲーム開発基盤」を、
実際のコードベースへ落とし込むための確定設計である。

---

## 1. 三つのPlane

```text
Control Plane   : AgentOS Core（ポータブルC++20、エンジン非依存、Linuxでも単体テスト可能）
Execution Plane : GameEngine（ECS / Scheduler / Renderer / PhysX / Editor）
State Plane     : SQLite (WAL) + JSON Artifact + Logs/AgentOS/
```

AgentOSは**エンジン組み込み**である。`AgentOSService : IService` として
`EngineContextBuilder::Build()`（engineContext.cpp）に登録し、
BRAIN改造版のEditorパネル（`AgentOSPanel : IEditorUI`）がUI兼対話層になる。
LLMバックエンドは既存 `LLAMAService` / `LLAMAAgent`（llama.cpp）。

「OS」はメタファであり、実体は
**タスク実行ランタイム＋権限管理＋検証パイプライン＋永続ストア** である。

## 2. モジュール配置

```text
Source/GameApplication/AgentOS/
  Core/            ← ポータブル層。Windows API・エンジンヘッダをincludeしない
    AgentOsTypes.h         ID型・Result型・列挙
    Json.h                 nlohmann/json includeラッパ（Backends/llama/vendor を再利用）
    Command/               Command実行パイプライン（§5）
    Budget/                Budget管理（§7）
    Store/                 SQLite Task/Evidence/Logic Store（§8）
    Evidence/              Evidence構造とEvidenceBuilder（§6）
    Logic/                 LogicGraph（仮説グラフ）（§6）
    Llm/                   ILlmBackend / MockLlmBackend / PromptTemplates
    Agents/                Intake〜Synthesisの各Agent（§9）
    Orchestrator/          Supervisor / TaskDag / EarlyStopping / Orchestrator（§10）
  EngineTools/     ← エンジン依存層（MSVCのみ）。ICommandExecutor実装群
    EngineToolContext.h    SceneManager / SystemRegistry / DebugLogService 等への参照束
    SystemDescriptorExport SystemAccess・Schedule → JSON（Phase 1）
    QueryTools             ListSystems / FindWriters / DescribeEntity / ReadComponent
    RuntimeTools           RunFrames / FindEntityByName（自前インデックス）
    WriteTrace             Component書込トレース（§11）
    EngineToolRegistry     全ToolをCommandPipelineへ登録
  Service/
    AgentOSService         IService。Orchestratorとワーカースレッドを所有
    LlamaLlmBackend        ILlmBackend実装（LLAMAAgentをポーリング）
  UI/
    AgentOSPanel           BRAIN改造版 IEditorUI（チャット＋Task木＋Evidence表示）

Source/GameApplication/Backends/sqlite/   sqlite3.c / sqlite3.h（amalgamation 3.53.2, vendored済）
Tests/AgentOS*.cpp                        エンジン規約どおり自己完結main()+assert
Tests/AgentOS/Makefile                    Linux(g++)用ビルド＆実行
Docs/AgentOS/                             本設計書群
```

**依存方向の鉄則**: `Core/` は `EngineTools/` `Service/` `UI/` を一切知らない。
エンジン側が `ICommandExecutor` と `ILlmBackend` を実装して差し込む（DI）。
これによりCoreはLinux g++でビルド・テストでき、MSVC往復なしで品質を担保する。

## 3. 信頼境界

LLM出力はすべて「提案」。実行可否は決定的なC++コードのみが決める。

```text
LLM text → JSON抽出 → Schema検証 → Capability検証 → Precondition検証 → 実行 → Postcondition → Audit
```

- LLMの自己申告Confidenceは採用しない。Confidenceは
  Rubricスコア×Evidence数×矛盾数×テスト結果からプログラムで算出する。
- Modification系Tool（SetComponentField等）は高権限Capabilityが必須。
- 全CommandはSQLiteのCommandテーブルへ監査記録される。

## 4. Evidence / Logic / Decision の分離

- **Evidence**: 観測・検索・実行から直接得た事実。必ずProvenance（source_type / source_uri /
  frame / session）を持つ。Logicが破綻しても破棄しない。
- **Logic**: Evidence間の関係から組み立てた仮説。supports / contradicts でEvidence IDを参照。
- **Decision**: 検証済みLogicに基づく実行判断。requiredTests を必ず持つ。

いずれもSQLiteに永続化し、子Taskは成果物をコピーせずEvidence IDで参照する。

## 5. Command実行パイプライン（Core/Command）

```cpp
struct CommandRequest {
    CommandId  id;
    AgentId    issuer;
    std::string tool;        // "ReadComponent" など
    Json        arguments;
    CapabilityToken capability;
};
struct CommandResult {
    CommandStatus status;    // Ok / SchemaRejected / CapabilityRejected /
                             // PreconditionRejected / ExecutionFailed / PostconditionFailed
    Json        payload;
    std::string error;
};
```

- `CommandSchemaRegistry`: Tool名→引数スキーマ（型・必須・数値範囲・enum）。宣言的に登録。
- `CapabilitySet`: Agent/Workerごとの許可Toolリスト＋権限レベル
  （Read < Observe < RunControl < Modify）。
- `ICommandExecutor`: Tool実体。`Precondition(args)` と `Execute(args)` を分離実装。
  Dry Runは `Precondition` のみ実行することで実現する。
- `CommandPipeline::Submit()` が上記を順に通し、全結果をAuditSinkへ流す。

## 6. Evidence / Logic 実装

- `Evidence` は `claim`（自然言語1文）＋ `payload`（構造化JSON）＋ Provenance。
- `EvidenceBuilder`: 重複除去（claim+payloadハッシュ）、frame整列、矛盾検出
  （同一対象・同一frameで値が食い違うペアの列挙）、Coverage算出
  （計画されたTaskのうちEvidenceを生んだ割合）、欠損の明示。
- `LogicGraph`: LogicNode（仮説）とLogicEdge（supports / contradicts / causes）。
  Confidenceは `base(rubric) × f(supports数) × g(contradicts数)` で決定的に計算。

## 7. Budget と Supervisor

```cpp
struct Budget {
    int maxToolCalls; int maxRetries; int maxDepth;
    int maxLlmCalls;  int64_t maxTokens; int64_t maxMillis;
};
```

`BudgetTracker` が消費を記録し、超過時は `BudgetExceeded` を返す。
`Supervisor` は推論しない。担当は Task状態遷移の合法性検査 / Budget / 再帰深度 /
リトライ上限 / Rollback要求 / Human Approval Gate（Modify権限のCommandを保留キューへ）のみ。

## 8. SQLiteスキーマ（Core/Store）

WALモード。Task単位でトランザクション。

```sql
Task(id INTEGER PK, parent_id, type TEXT, state TEXT, depth INT, retry_count INT,
     spec_json TEXT, result_json TEXT, created_at, updated_at);
Evidence(id INTEGER PK, task_id, source_type TEXT, source_uri TEXT,
         claim TEXT, payload_json TEXT, confidence REAL, created_at);
LogicNode(id INTEGER PK, task_id, hypothesis TEXT, confidence REAL, status TEXT);
LogicEdge(from_id, to_id, relation TEXT);          -- supports / contradicts / causes
Command(id INTEGER PK, task_id, issuer TEXT, tool TEXT, arguments_json TEXT,
        validation_status TEXT, execution_status TEXT, result_json TEXT, created_at);
Session(id INTEGER PK, goal_json TEXT, state TEXT, created_at, updated_at);
```

Task状態機械: `Pending → Running → (Succeeded | Failed | Cancelled | AwaitingApproval)`。
遷移はSupervisorのみが行う。

## 9. Agent構成（Core/Agents）

全AgentはLLM呼び出しを `ILlmBackend`（`Generate(prompt) → text`）経由で行い、
出力はJSON契約（```json フェンス）で受け取り `JsonExtractor` で頑健にパースする。
パース失敗はリトライ1回→Taskを`Failed`にする（無限リトライしない）。

| Agent | 入力 | 出力 | LLM使用 |
|---|---|---|---|
| IntakeAgent | 自然言語要求 | goal/symptoms/constraints JSON | ○ |
| PlannerAgent | Intake結果+Tool一覧 | Task DAG（依存・allowedTools・budget付き） | ○ |
| RetrievalWorker | Task spec | Evidence列（結論は出さない） | △(検索語生成のみ) |
| EvidenceBuilder | Evidence列 | 統合Evidence+矛盾+Coverage | ×（決定的） |
| ReasoningAgent | Evidence+Logic のみ | 仮説列（supports/missingEvidence付き） | ○ |
| CriticAgent | 仮説+Evidence | Rubricスコア+failures | ○(所見) + プログラム(採点合成) |
| EvaluatorAgent | goal+テスト結果 | 達成判定 | △ |
| RepairAgent | Critic failures | 追加Task提案（Supervisor制約下） | ○ |
| SynthesisAgent | 確定Evidence/Decision/TestResult | 人間向け報告 | ○（新規調査・断定は禁止） |

フラクタル構造: Worker内部でも `LocalPlanner→Retrieval→EvidenceBuilder→Critic→Repair`
の縮小版ループを回せる（`TaskRuntime` を再帰利用、depthはSupervisorが制限）。
Workerは上位へ「検索戦略・失敗試行数・未確認範囲・Coverage・打ち切り理由」を必ず返す。

## 10. Orchestrator と Early Stopping

メインループ: `Intake → Plan → (Worker実行 → EvidenceBuild → Reason → Critic →
[不足ならRepair→再実行] ) → Evaluate → Synthesize`。

Early Stoppingは単一Confidenceでは判断しない。指標:
直近N回の新規Evidence量 / 仮説順位の変化 / 未解決矛盾数 / 同一失敗の反復回数 /
Tool Error率 / 残Budget比。停止時は「最有力仮説＋確認済みEvidence＋未確認範囲＋停止理由」を返す。

## 11. エンジン側Tool（EngineTools）

調査レポート（既存コード）に基づく実装方針:

- **SystemDescriptorExport**: `SystemRegistry::GetTasks()` / `CompiledSystemSchedule` から
  name / domain / phase / priority / SystemAccess(reads/writes) / 依存辺 をJSON化。
  `ScheduleProfileYamlExporter` と同じ情報源を使い、出力先は `Logs/AgentOS/`。
- **QueryTools**: `ComponentRegistry::GetComponentIDToNameMap()` で名前⇔型解決。
  `DescribeEntity` は `GetAllComponentsOfEntitySorted()`＋各Componentの `encode()`
  （YAML）→JSON変換で値を返す。`FindWriters/FindReaders` はSystemAccessから決定的に回答。
- **FindEntityByName**: エンジンに存在しないため、`NameComponent` 走査による
  名前インデックスをAgentOS側で保持（StructureVersionで無効化検知）。
- **WriteTrace**: エンジン無改造では完全なフックが無いため二段構え。
  - v1（frame diff）: 対象Entity/Componentを毎フレームスナップショットし差分を記録。
    書込System名の帰属は「そのフレームで当該Component型へWrite宣言を持つTask一覧」から推定
    （候補が1つなら確定、複数なら候補列挙として記録する。**推定であることをEvidenceに明示**）。
  - v2（フック）: `ComponentRegistry` 書込パスへ計測点を追加し正確な帰属を得る（エンジン改修）。
  - Ring Buffer（既定4096 record）、対象限定でオーバーヘッドを抑える。
- **構造変更の制約**: Schedule実行中の直接構造変更はassertで落ちるため、
  AgentOSのModification系ToolはEditor Domainのタイミング（またはEntityCommandBuffer経由）
  でのみ実行する。Preconditionで実行タイミングを検査する。

## 12. Service / UI 統合

- `AgentOSService`: `EngineContextBuilder::Build()` でLLAMAServiceの後・EditorServiceの前に登録。
  自前ワーカースレッドでOrchestratorを回し、UIへは状態スナップショットを公開
  （mutex保護のコピー渡し。エンジンスレッドをブロックしない）。
- `LlamaLlmBackend`: `LLAMAService::CreateAgent()` でAgent専用contextを確保し、
  `RunAsync` → `GetState()==Running` の間 `GetOutput()` を50msポーリング（BRAIN.cppの確立パターン）。
- `AgentOSPanel`: BRAINのUI構造を継承しつつ、チャットに加えて
  Task DAGツリー / Evidenceリスト / Command監査ログ / Budget残量 / Approvalボタンを表示。
  `EditorService::Initialize()` に登録（BRAINは現在コメントアウトされている行の近く）。

## 13. テスト戦略

- Core層: `Tests/AgentOSXxxSmokeTest.cpp`（自己完結main+assert、エンジン規約準拠）。
  Linuxでは `Tests/AgentOS/Makefile`、WindowsではCIワークフローと同形式のcl.exe直叩きで実行可能。
- 垂直スライスE2E: MockLlmBackend（台本方式）＋FakeEngineTools（インメモリWorld）で
  「Component値異常の原因System特定」フローを最初から最後まで通す。
- エンジン統合層: VSビルド後、実エンジンでSystemDescriptorExport→JSON確認が最初の受け入れ試験。

## 14. 評価指標・差別化・結論

構想ドキュメント §15〜§17 をそのまま採用する。要点:
- 「動いた」ではなく、原因System特定率 / Build成功率 / Regression率 / 不正Command拒否率 /
  調査時間短縮で測る。
- 本基盤はAI機能の追加ではなく、**エンジンを観測可能・説明可能・再現可能・検証可能にする
  アーキテクチャ刷新**である。WriteTraceやReplayはLLM無しでも通常のデバッグ・CIに価値を持つ。
