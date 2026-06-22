# Step 17: Task Naming and Main-Thread Task Decomposition Plan

## 目的

実機Schedule Captureの結果を基に、SystemTaskの命名を統一し、MainThreadで実行する必要がある処理から純粋なCPU計算だけをWorker Taskへ分離する。

本Stepでは、MainThread Taskを無理にWorkerへ移動しない。

基本構造は次とする。

```text
CPU Prepare / Build
AnyWorker
        ↓
State Commit / GPU Upload / Command Submit
MainThread
```

---

## 設計上の前提

### ScriptSystem / CustomScriptSystem

`ScriptSystem`と`CustomScriptSystem`は、モックアップ作成および柔軟なゲームロジック実装を目的としたオブジェクト指向Componentである。

Scriptからは次を含む全Engine Resourceへのアクセスを許可する。

- ECS Component
- Scene / Entity
- Physics
- Audio
- Graphics
- Resource Service
- その他Engine Service

したがって、Script Taskは次の契約を維持する。

- `MainThread`
- `World::Exclusive`
- Script登録順の安定化
- Script実行後のCommand Commit
- 標準機能として並列化しない

ScriptのExclusive Access削減は、本Stepの最適化対象に含めない。

### Graphics APIを操作するTask

次のTaskはD3D11 Immediate ContextまたはGPU Resourceを操作するため、Commit / Upload / Submit部分をMainThreadに維持する。

- `RenderSystem.Animation.Upload`
- `TerrainSystem.Mesh.Upload`
- `WaveSystem.Vertex.Upload`
- `RenderSystem.Frame.Submit`

最適化対象は、これらの内部に含まれる純粋CPU計算である。

---

# 実行順

## Step 17-A: SystemTask命名規則の統一

優先度: **最優先**

状態: **完了**

この作業は以降のCapture比較、依存解析、Profiler表示、YAML Exportの基準になるため、Step 16の残作業およびRender Packet分割より先に実施する。

### 目的

Schedule Capture、Profiler、YAML Export、依存グラフ上で、Taskの所属System、Domain、役割、実行段階を一意に判断できるようにする。

### 命名形式

標準形式:

```text
<SystemName>.<Operation>
```

分割Task:

```text
<SystemName>.<Feature>.<Stage>
```

チャンクTask:

```text
<SystemName>.<Feature>.<Stage>.Chunk<Index>
```

### Stage名

原則として次の語彙を使用する。

- `Collect`: 対象収集
- `Prepare`: 入力整形
- `Build`: CPU側結果生成
- `Simulate`: シミュレーション開始または実行
- `Fetch`: 非同期処理結果待機・取得
- `Resolve`: 参照解決・結果統合
- `Commit`: ECS / Runtime状態への確定反映
- `Upload`: GPU Resourceへの転送
- `Submit`: Graphics Queue / APIへのコマンド提出
- `Dispatch`: Event / Callback配送
- `Cleanup`: 後処理

### 禁止事項

- Domain名と実処理が一致しない名前
- `Draw`という名前でCPU Mesh生成まで行うTask
- `Update`という名前だけで複数責務を持つTask
- `EditorUpdate`をRender Domainへ登録したまま名称だけ残すこと
- 同一処理に`Update`、`Execute`、`Process`が混在すること
- Task名に実装API名を含めること。ただしBackend固有Taskを明示する必要がある場合を除く

### 初期統一対象

- [x] 全`RegisterTasks()`のTask名を一覧化
- [x] DomainとTask名の不一致を修正
- [x] `Draw` / `Update` / `EditorUpdate`の曖昧な名称を役割名へ変更
- [x] Physics Taskを`Upload / Simulate / Fetch / Download / Dispatch / Commit`へ統一
- [x] Render Taskを`Build / Upload / Submit`へ統一
- [x] Schedule Profiler表示とYAML Exportで同一名称を使用
- [x] Task名重複を検出するDebug Validationを追加
- [x] 命名規則をScheduler文書へ追加

完了記録:

- `Docs/Step17A_Task_Naming_Completion.md`
- `Docs/Step17B_Render_Packet_Foundation.md`
- `Docs/Step17B_Render_Packet_Opaque_Pass_Connection.md`
- `Docs/Step17B_Render_Packet_View_Pass_Connection.md`

### 完了条件

- Capture内のTask名だけで所属Systemと責務が判別できる
- 同一System内に重複Task名がない
- Domainと名称に明確な矛盾がない
- Task名変更のみで実行順・Access・ThreadAffinityが変化しない

---

## Step 17-B: Render Packet基盤

優先度: **高**

状態: **主要RenderPass接続完了・計測待ち**

### 目的

`RenderSystem.Command.Submit`からECS走査と描画準備を分離し、WorkerがRender Packetを生成し、MainThreadが完成済みPacketを提出する構造へ移行する。

### 目標構造

```text
RenderSystem.Packet.Collect
        ↓
RenderSystem.Packet.Build
        ↓
RenderSystem.Packet.Sort
        ↓
RenderSystem.Command.Submit
```

### Worker Task

- Entity / Component走査
- Frustum Culling
- Transform抽出
- Material / Texture参照解決
- RenderLayer分類
- Shadow Caster抽出
- Sort Key生成
- Render Packet生成
- Packet Sort / Batch候補生成

### MainThread Task

- RenderTarget切替
- Pipeline / Shader設定
- Resource Binding
- Draw / Dispatch
- ImGui描画
- Present

### 実装項目

- [x] API非依存`RenderPacket`定義
- [x] Frame-local Packet Buffer
- [x] Model / Mesh / Sprite / Particle / Terrain / Wave Packet Builder
- [x] Packet Sort Key仕様
- [x] Worker-local Packet Bufferと決定的Merge
- [x] MainThread Submit Task
- [ ] RendererからComponentRegistry直接参照を段階的に除去
- [x] 既存RenderPassとの接続
- [ ] CaptureでBuildとSubmitの分離を確認

### 完了条件

- MainThreadのRender Submit中にECS Worldを再走査しない
- D3D11 Immediate Context操作はMainThreadに限定される
- Packet生成はAnyWorkerで実行可能
- 既存Player View / Editor Viewに描画回帰がない

---

## Step 17-C: Animation CPU Build / GPU Upload分離

優先度: **高**

状態: **未着手**

### 目標構造

```text
RenderSystem.Animation.Prepare
        ↓
RenderSystem.Animation.BuildPose.ChunkN
        ↓
RenderSystem.Animation.Upload
```

### Worker Task

- Animation時間計算
- Keyframe探索
- Translation / Rotation / Scale補間
- Local Pose生成
- World Pose生成
- Skinning Matrix生成

### MainThread Task

- Constant Buffer更新
- Structured Buffer更新
- D3D11 Resource Map / Unmap
- Runtime GPU Bufferへの反映

### 実装項目

- [ ] Animation入力Snapshot
- [ ] Frame-local Pose Result
- [ ] 2～4チャンクから開始
- [ ] Entity数に応じたチャンク数調整
- [ ] Pose Resultの世代・Revision検証
- [ ] GPU UploadのMainThread固定
- [ ] Editor停止中Previewの挙動維持
- [ ] CaptureでBuildとUploadの重なりを確認

---

## Step 17-D: Terrain CPU Mesh Build / GPU Upload分離

優先度: **中**

状態: **未着手**

現行Captureでは負荷が小さいため、Render PacketとAnimationの後に実施する。

### 目標構造

```text
TerrainSystem.Mesh.DetectDirty
        ↓
TerrainSystem.Mesh.Build
        ↓
TerrainSystem.Mesh.Upload
        ↓
TerrainSystem.Physics.Commit
```

### Worker Task

- 頂点生成
- Index生成
- 法線計算
- Tangent計算
- CPU Mesh Result生成

### MainThread Task

- Vertex / Index Buffer生成
- Runtime Mesh交換
- Graphics Resource反映
- Physics Collider更新要求のCommit

### 実装項目

- [ ] Terrain Revision追加
- [ ] `TerrainMeshBuildResult`
- [ ] Dirty時のみBuild Task生成
- [ ] 古いRevisionの結果破棄
- [ ] GPU UploadのMainThread固定
- [ ] Collider更新との順序確定

---

## Step 17-E: Wave CPU Vertex Build / GPU Upload分離

優先度: **中～低**

状態: **未着手**

現行Captureでは非常に小さいTaskであるため、高解像度Waveで必要性を確認してから実施してよい。

### 目標構造

```text
WaveSystem.Vertex.Build
        ↓
WaveSystem.Vertex.Upload
```

### Worker Task

- 波高計算
- 頂点位置計算
- 法線計算
- CPU Vertex Result生成

### MainThread Task

- Dynamic Vertex Buffer Map
- memcpy
- Unmap

### 実装項目

- [ ] 関数内`static std::vector`の廃止
- [ ] Task-localまたはFrame-local Bufferへ変更
- [ ] Wave Revision検証
- [ ] GPU UploadのMainThread固定
- [ ] 高解像度Captureで効果測定

---

## Step 17-F: Physics Begin–Fetch待機隠蔽の検証

優先度: **低・実測依存**

状態: **検討保留**

### 方針

Physicsの同期待ちを隠すためだけに、状態二重化や1 Tick遅延を標準導入しない。

まず既存の独立CPU Taskを`PhysicsBegin`と`PhysicsFetch`の間へ配置できるかを検証する。

### 配置可能条件

- Physics結果を読まない
- Transformを書き換えない
- Colliderへ触らない
- Scriptを呼ばない
- Scene構造を変更しない
- 前状態の利用が許容される

### 候補

- Animation Pose Build
- Terrain CPU Mesh Build
- Wave CPU Vertex Build
- Resource Decode
- 前状態から生成可能なRender Prepare

### 非対象

- Script / CustomScript
- Character移動
- Collision / Trigger Dispatch
- Rigidbody同期
- Physics後Transformを使用するCamera / Follow

### 実装判断条件

- `PhysicsFetch`待機が継続して主要ボトルネックである
- 待機中に十分な独立Taskが存在する
- 状態世代管理の複雑化より実測効果が大きい

条件を満たさない場合は実装しない。

---

# 検証手順

各Substep完了時に次を行う。

1. コンパイルチェック
2. 必要なSmoke Testのみ実行
3. Editor / Player View実機確認
4. Schedule Capture取得
5. Task名、Domain、ThreadAffinity、依存辺を比較
6. MainThread時間と最大並列数を比較
7. YAML Captureを保存
8. PR本文とCompletion Documentを更新

ビルドを伴うテストは頻度を抑え、通常はコンパイルチェックを優先する。

---

# 実装優先順位

1. **Step 17-A: SystemTask命名規則の統一**
2. **Step 17-B: Render Packet基盤**
3. **Step 17-C: Animation CPU Build / GPU Upload分離**
4. Step 17-D: Terrain CPU Mesh Build / GPU Upload分離
5. Step 17-E: Wave CPU Vertex Build / GPU Upload分離
6. Step 17-F: Physics Begin–Fetch待機隠蔽の再評価

ScriptSystem / CustomScriptSystemのMainThread・World Exclusive設計は維持する。
