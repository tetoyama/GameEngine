# AgentOS 実行エンジン化 & BRAIN統合 ロードマップ

Status: draft（2026-07-21 テトと方向性合意中）

本書は、既存の `00_Architecture.md` / `01_Phase_Plan.md` を否定するものではなく、
その延長として **「調査エンジン」から「実行エンジン」への進化** と **BRAIN統合** を
位置づける追補である。既存 Phase 計画との対応は §6 に示す。

---

## 1. 現状認識：いまのAgentOSは「調査エンジン」

`Orchestrator::RunSession` を読み切った結果、現行実装は構造として
**リサーチアシスタント（調べてレポートする）** である。

```
Intake（要求理解）
→ Planner（retrieval/analysis タスクに1回だけ分解）
→ 依存順にトポロジカルソート(Kahn法) して retrieval を一巡実行、Evidence 収集
→ Reasoning（Evidenceから仮説グラフを構築）
→ Critic（仮説を検証）
→ Repair（不足なら推論を反復。EarlyStoppingで打ち切り, maxRepairRounds=2）
→ Synthesis（レポート生成）
```

特徴と限界：

- タスク種別は `retrieval` / `analysis` の2つだけ。**「ツールで世界を書き換える」タスク種別が存在しない。**
- `dependencies` は持つが、**実行前の静的トポロジカルソートに使うだけ**。上流結果が変わったときに下流を無効化して差分再計画する動的な使い方はしていない。
- `LogicGraph` は「実行タスクの依存グラフ」ではなく「仮説の依存グラフ」。
- `EarlyStopping` のラウンド反復は「証拠が飽和したか」の判定であり、行動の再計画ループではない。
- Plannerは **最初に全計画を1回で立てる**（＝小規模モデルに長期計画を要求する）。これは理想形が最も避けたい方式。
- 既存 Phase 計画上の現在地は **Phase 2（LLM統合）進行中**。実Tool接続（Phase 3）以降は未着手。

## 2. 理想形：実行エンジン（テト構想の要約）

小規模ローカルLLMでも、問題を十分細かく分解すれば大規模モデルに近い成果を出せる、
という仮説に基づく。差が出るのは「計画と問題構造化」の段階。そこを人間（大規模モデル）に
頼らず、**壊れにくい計画を継続的に更新する方式** で補う。核は3本柱：

### 柱1. タスクを「契約」として定義する

各タスクを曖昧な文章でなく、次の項目で表す：

```
Goal / Preconditions / Required observations / Action /
Expected result / Success condition / Failure condition /
Side effects / Recovery action
```

重要なのは `Expected result`（何が起こると予測しているか）と
`Success condition`（何を確認できれば完了と認めるか）を **分離** すること。
予測と実結果がズレれば、成功していても前提の再確認が要る。

### 柱2. タスクを依存グラフとして管理する

順序付きリストではなく依存グラフ。しかも「BはAの後」ではなく
「BはAの**どの出力**に依存するか」を持たせる。これにより上流が後から変わったとき：

```
上流タスクの結果が変化 → 依存する下流を invalidate
→ 影響を受けた部分だけ再計画（全計画を作り直さない）
```

`Side effects` もグラフ更新のトリガに使う。

### 柱3. 数手ごとに差分再評価する

最初に全工程を完全計画しない。

```
粗い全体計画 → 近いタスクだけ詳細化 → 1〜3手実行
→ 結果を反映 → 差分再計画
```

再評価は固定周期（2〜3手ごと）に加え、異常時（タスク失敗 / Expected≠実結果 /
予想外の副作用 / 新しい依存の発見 / Precondition崩壊 / Recovery反復）は即時。
再評価の入出力は**グラフへの差分**に限定し、毎回ゼロから作り直さない。

> 要するに「賢い計画を一度で作る方式 → 壊れにくい計画を継続的に更新する方式」への転換。

## 3. ギャップ：3本柱 × 現状充足度

| 理想形の柱 | 現状 | 差分 |
|---|---|---|
| **柱1 タスク契約** | Plannerスキーマは `id/type/description/dependencies` の4項目のみ | Goal/Precond/Expected/Success/Failure/Side effects/Recovery が無い。ただし `CommandStatus` に Precondition/Postcondition/AwaitingApproval の受け皿はある |
| **柱2 動的依存グラフ** | `dependencies` はあるが実行前の静的ソート専用 | 「どの出力に依存するか」の粒度が無い。上流変化→下流invalidate→差分再計画が無い |
| **柱3 差分再評価** | Plannerは1回のみ。末尾のCritic/Repairだけ | 実行途中の再評価経路が無い。`for`一巡で全タスクを流すだけ |

## 4. 既存資産マップ（ゼロからではない）

理想形の部品として、以下は既に噛み合っている：

- **権限**: `PermissionLevel` に `Modify`（コード変更・Entity生成）まで定義済み。書き換え前提の設計。
- **契約の土台**: `CommandStatus` に `PreconditionRejected` / `PostconditionFailed` / `AwaitingApproval`。pre/post条件とHuman Approval Gateの受け皿。
- **実行境界**: `CommandPipeline` + `CapabilitySet` によるツール実行の多層検証。
- **観測・操作の手**: `EngineTools`（EntityIntrospection / SystemIntrospection / WriteTrace / MainThreadDispatcher）。
- **状態機械と永続化**: `TaskState` + `Supervisor` + `TaskStore`(SQLite)。差分再計画の記録基盤になりうる。
- **停滞検知**: `EarlyStopping` の飽和判定ロジックは、再評価トリガに転用できる。

## 5. BRAIN統合方針

- **BRAIN**（Built-in Recon Artificial Intelligence Navigator）＝ AgentOSを活用したエンジン組み込み開発支援AI。
- パネルは既存の **AgentOSPanel に一本化** し、名前を "B.R.A.I.N." に統一。旧 `Editor/UI/BRAIN/` パネルは資産（ロゴ演出等）を吸収したうえで **廃止**（`editorService.cpp` で既にコメントアウト済み）。
- **Quickタブ（Orchestrator非経由の軽量チャット）は作らない。** 用途の本命は「ツールで実際に開発を動かす」ことであり、素の会話は用途としてほぼ無いため。
- モデルは `LLAMAService::GetModel(path)` のパスキャッシュにより重みが自動共有される。二重ロードの心配は無い。

## 6. ロードマップ（既存Phaseと接続）

既存 `01_Phase_Plan.md` の Phase を尊重し、**Phase 4 を「実行ループ」として再定義**、
RAG を並行レーンとして追加する。

| 段階 | 内容 | 既存Phaseとの対応 |
|---|---|---|
| **S0. ガワ統合** ✅ | AgentOSPanel を BRAIN として一本化。Quick無し（§6.1 に完了内容） | UI整理（既存Phase外） |
| **S1. 実Tool接続** | CommandPipeline に EngineTools を接続し Read/Observe で状態観測 | 既存 **Phase 3** |
| **S2. タスク契約の導入** | Plannerスキーマに `goal/preconditions/expected/success/failure/side_effects/recovery` を追加、TaskRecordにも保持。まず「書かせる」だけ | 柱1（新規） |
| **S3. 観測ベースの成否判定** | Tool実行後に Expected と実結果を照合し Success/Failure を機械判定。`PostconditionFailed` を活用 | 柱1 + 既存 Phase 3 の延長 |
| **S4. 差分再評価ループ** | Orchestratorの `for` 一巡を `while(未完了){ 数手詳細化→実行→観測→差分再計画 }` へ改造。EarlyStopping を再評価トリガに転用 | 既存 **Phase 4 を再定義**（柱3） |
| **S5. 動的依存グラフ** | 依存を「出力単位」に細粒度化。上流変化→下流invalidate→影響範囲だけ再計画。Side effects を接続 | 柱2（新規） |
| **S6. Apply gate** | ApplyCodePatch→Compile→Test→Rollback と Human Approval Gate。Modify権限を段階有効化 | 既存 **Phase 5** |

### 6.1 S0 完了内容（2026-07-25）

| 項目 | 実装 |
|---|---|
| パネル登録 | `editorService.cpp` に `UIs.push_back({"BRAIN", new agentos::AgentOSPanel()})`。表示名を `"AgentOS"` から変更（Performance Monitor / Profiler の表示名もこれに追従） |
| ウィンドウタイトル | `AgentOSPanel::Draw` の `ImGui::Begin` を `"B.R.A.I.N."` に統一（旧BRAINと同一文字列） |
| 背景ロゴ | `AgentOSPanel::DrawBackgroundLogo()` を新設。`Asset/BRAIN/logo/Icon.png` をアスペクト比維持で内接配置し `IM_COL32(255,255,255,8)` で敷く（旧`BRAIN.cpp`のロジックを移植）。ロード失敗・`resourceService`未接続でも起動を妨げないようガード済み |
| 表示トグル | `AgentOSPanel::ResolveShowFlag()` が `MenuBar::showBRAIN` を参照。加えて Window メニューに `"B.R.A.I.N."` の `MenuItem` を**新規追加**。旧BRAINにはこのメニュー項目が無く、ウィンドウを閉じると再表示不能だった（潜在バグの解消）。MenuBar が解決できない場合のみ内部フラグ `m_show` にフォールバック |
| サービス注入 | 既存の `engine.cpp` → `GetUI<AgentOSPanel>()` → `SetService()` 経路をそのまま利用（型ベース解決のため登録名変更の影響なし） |
| 旧 `Editor/UI/BRAIN/` | **今回は残置**（`editorService.cpp` でコメントアウトのまま、vcxprojにも残る）。参照用。廃止の判断は保留 |

未処理の既知事項：

- `BRAIN_MODEL_PATH`（`BRAIN.h`）と `engine.cpp` の `agentOSContext.modelPath` に同じ文字列 `"Asset/BRAIN/model/Qwen3.5-9B-Q4_K_M.gguf"` が二重定義されている。旧BRAIN廃止時に一本化する。
- 上記はすべてLinuxサンドボックス上の編集であり **MSVC実ビルド未検証**（§7）。特に `AgentOSPanel.cpp` に追加した `Editor/UI/MenuBar.h` / `Resources/*` のinclude解決はVSでの確認が要る。

### 並行レーン：RAG（コード/API検索基盤）

狙いは「巨大な自作エンジンのコード/APIを小規模モデルに理解させる」こと。2層に分ける：

- **下層（検索基盤）= 先行・並行で着手可。** コードベースをチャンク分割 → embedding → ベクトル検索。実行ループの設計と独立に進められ、S2〜S3でも「関連コードを引く」形で早期に役立つ。
- **上層（利用）= S4以降で接続。** 実行ループが「いつ・どの粒度で・何をクエリし、結果をどう詰めるか」を決めてから繋ぐ。先に作り込むと手戻りになるため。

RAG下層の前提決めごと（**2026-07-25 実測により確定**）：

1. **埋め込みモデル**: → **評価セットで実測して決める**。GPUは描画に温存するためCPU推論前提。
   量子化は **Q8_0** を採る（F32はBF16訓練からのアップキャストで無意味、BF16はCPUで
   ネイティブ命令が無く遅い、Q4_0は小規模モデルほど劣化が大きい上にレガシー形式）。
   候補は Qwen3-Embedding-0.6B-Q8_0（32kコンテキスト・分割不要）と
   embeddinggemma-300M-Q8_0（2倍速・ただし2048制約で5.2%が超過）。
2. **ベクトルストア**: → **自前フラット全走査で確定**。チャンクは実測1,473件しかなく、
   1024次元でも6MB。総当たり内積は150万回で1ms未満。sqlite-vecを入れる理由が無い。
   永続化はSQLiteのBLOB（差分更新の容易さが決め手。§6.2参照）。
3. **チャンク戦略**: → **関数 + 型宣言の2種で確定**。変数は独立させない
   （実測9,403件で関数の14倍、かつ単体では意味が定まらない）。

### 6.2 RAG下層 ステップ1〜2 完了内容（2026-07-25）

実装は全て `AgentOS/Core/CodeIndex/` に置き、llama.cpp / D3D11 に依存させていない。
`Tests/AgentOS/Makefile` からLinuxでビルド・実行できるため、MSVC実ビルドを待たずに
検証できる（§7の制約を回避できる唯一の領域）。

| 層 | ファイル | 役割 |
|---|---|---|
| 前処理 | `SourceMasker` | コメント/文字列/生文字列を空白化し波括弧カウントを安全にする |
| 切り出し | `CodeChunk` / `CodeParser` | 関数定義・型宣言の抽出、名前空間追跡、構造ヘッダ生成 |
| 走査 | `CodeIndexBuilder` | ディレクトリ走査、除外、JSON出力、統計 |
| 埋め込み抽象 | `IEmbeddingBackend` / `MockEmbeddingBackend` | llama.cpp非依存の抽象と決定論的Mock |
| ベクトル演算 | `VectorMath` | L2正規化、内積、float⇔BLOB変換 |
| 永続化 | `CodeIndexStore` | SQLiteスキーマ、ファイルハッシュによる差分更新 |
| 検索 | `CodeSearch` | 字句(IDF+長さ正規化)、ベクトル全走査、RRF融合 |
| CLI | `AgentOS/Tools/CodeIndexCli.cpp` | オフライン索引構築・検索。`make -f Tests/AgentOS/Makefile codeindex` |

`SqliteDb` に `BindBlob` / `ColumnBlob` を追加。
`Tests/AgentOS/Makefile` にヘッダ依存追跡(`-MMD -MP`)を追加（無いとヘッダ変更で
オブジェクトが再ビルドされず、古いレイアウトが混ざってクラッシュする）。

**実走査の結果**（542ファイル / 6秒、Backends除外）:

| 指標 | 値 |
|---|---|
| チャンク | 1,473（関数 712 / 型宣言 736） |
| トークン中央値 | 142 |
| 総トークン | 約756,000 |
| 2048トークン超 | 77件 (5.2%) ← EmbeddingGemma-300M の上限 |
| 8192トークン超 | 8件 (0.6%) |
| 最長 | 17,403トークン（`ColorTerritoryRuntime.h`、1,335行の単一クラス） |

→ **どのモデルを選んでも長大チャンクの分割ロジックは必要。**
CPU推論での初回フル構築は 0.6B で25〜40分の見込み。差分更新なら日常的には1秒未満。

**実走査で発見し修正した不具合**（推測ではなく実データで出たもの）:

1. `class Foo final : public Bar` の `final` を型名として抽出 → final指定の型が全て同名に潰れ索引が衝突
2. `char F(char c) { return std::tolower(c); }` の本体内呼び出しを定義と誤検出 → 修飾名は行内で最初の `(` より前にある必要がある
3. 字句検索が日本語を1トークンも拾わない（`isalnum`がUTF-8マルチバイトを弾く）→ CJK bigram索引を導入。コメント行の44.7%が日本語なので致命的だった
4. 字句スコアにIDFが無く、日本語の頻出bigram（して/いる）で「日本語コメントが多いだけのチャンク」が上位を独占
5. 長さ正規化が無く、1,000行超のチャンクが「たまたま語を含む」だけで常に1位

### 6.3 RAG下層の残作業（テト側）

- `Service/LlamaEmbeddingBackend`（`IEmbeddingBackend` の llama.cpp 実装）。
  `llama_context_params::embeddings = true` と `pooling_type` の設定が要る。
  現行 `LLAMAAgent::CreateContext()` は生成専用で embedding 経路が存在しない。
- 上記のVS実ビルド確認（S0の変更分と CodeIndex 一式）。
- 評価セット（クエリ20件 + 正解チャンク）の作成 → モデル比較。
- 長大チャンクの分割方針の決定。



## 7. バグ潰しの位置づけ

現行コードは「Linuxサンドボックス上で実装され、MSVC実ビルド未検証」（`02_VS_Integration.md` 明記）。
git log にも試行錯誤の跡。バグの多くはこの実ビルド未検証に由来する可能性が高い。
バグ潰しは上記の各段階（特にS2〜S4の仕様変更）に織り込んで進める。

## 8. 未確定事項 / 次アクション

- ~~「どっちから」→ **S0（ガワ統合）+ RAG下層を並行**で開始~~ → S0 完了（§6.1）。次は **S1（実Tool接続）** か **RAG下層** の二択。
- テトによるVS実ビルド確認（S0の変更分）。
- RAG前提3点（埋め込みモデル・ストア・チャンク）の確定。**未着手・S0完了により次の律速。**
- この環境ではMSVC/DirectXの実ビルド不可。実装は「検証可能な単位に分解 → テトがVSでビルド確認」の分担で進める。
