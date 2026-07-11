# Step 17-D Terrain CPU Build / GPU Upload Completion

## 状態

**コード実装完了・VSビルド確認待ち（2026-07-11）**

Terrainメッシュ生成を、純CPU BuildとMain Thread GPU Uploadへ分離した。

## 実行契約

### 初回生成

`TerrainSystem::Initialize()`で同期的に次を実行する。

1. `BuildTerrainMeshes()`
2. `UploadTerrainMeshes()`

これにより、従来どおりInitialize完了時点でTerrainのGPUメッシュを利用できる。
初回だけを最初のRenderフレームまで遅延させる仕様にはしない。

### 実行中の再生成

以後のScale / HeightMap変更はScheduler上の二段Taskで処理する。

- `TerrainSystem.Mesh.Build`
  - Domain: Render
  - Phase: Earliest
  - Affinity: AnyWorker
  - Terrain staging頂点・インデックスを生成
  - D3D11 APIには触れない
- `TerrainSystem.Mesh.Upload`
  - Domain: Render
  - Phase: Early
  - Affinity: MainThread
  - stagingからVertex / Index Bufferを生成
  - 成功後にColliderの`needsUpdate`を設定

両Taskが`TerrainComponent`へWrite Accessを宣言するため、Build→Uploadの順序は
Schedulerの依存関係でも保証される。

## Staging世代管理

Build時にScaleとHeightMap内容から`stagingSignature`を生成する。
Upload直前に現在値から再計算し、一致しない古いstagingは破棄する。

Upload失敗時は以下を維持する。

- staging頂点・インデックス
- stagingSignature
- `meshBuildReady=true`
- 既存の正常なGPUメッシュ

次のRenderフレームでは同じstagingを再構築せずUploadだけ再試行する。
入力が変更された場合のみ古いstagingを破棄して再Buildする。

## Transactional GPU Upload

新しいVertex BufferとIndex Bufferは一時`ComPtr`へ生成する。
両方の生成に成功した時点をCommit Pointとし、その後でのみ
`MeshRendererComponent`の既存Bufferと交換する。

これにより以下を防止する。

- Vertex Buffer成功後、Index Buffer失敗でTerrainが消える
- 再生成失敗時に以前の正常なTerrainまで失う
- null Bufferと以前の`indexCount`を組み合わせてDrawする

`RenderableTerrain`側でもVertex Buffer / Index Buffer / indexCountを検証し、
不完全なメッシュは描画しない。

## MSBuild項目

巨大な`GameEngine.vcxproj`本体の全体書き換えを避け、
`Directory.Build.targets`でGameEngineプロジェクトの評価後Itemを正規化する。

登録対象:

- `TerrainMeshBuilder.h`
- `TerrainMeshUpload.h`
- `TerrainTaskRegistrar.h`
- `RenderSystemAnimationTaskRegistrar.h`
- `D3D11ConstantBufferUpload.h`

削除済みの`RenderSystemAnimationTaskRegistration.h`はProject Itemから除外する。
既存登録との重複を避けるため、新規Headerは一度`Remove`してから`Include`する。

## 完了条件

- [x] Initialize時の同期Terrain生成
- [x] CPU BuildとGPU UploadのTask分離
- [x] Build後の入力変更をSignatureで拒否
- [x] Upload失敗時の旧GPUメッシュ維持
- [x] Upload失敗時のstaging再試行
- [x] Renderable側のnull Buffer防御
- [x] MSBuild Project Item同期
- [ ] Windows Debug x64 Build
- [ ] Windows Release x64 Build
- [ ] 初回Scene表示時にTerrainが欠落しないこと
- [ ] Scale変更とBrush編集後にTerrainとColliderが更新されること
- [ ] GPU Buffer生成失敗を模擬した場合も旧Terrainが表示され続けること
