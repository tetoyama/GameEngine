# Step 18-A RenderWorld / Runtime Ownership Progress

## 状態

**実装中 — RenderWorld基盤、Camera / ModelRendererのNative描画資源分離、Model CPU Geometry SourceからStatic Batchと通常RenderableのRHI Geometryを生成する経路、Render Packet Extraction Task分離まで完了。CI・追加実機確認待ち。**

親計画:

- `Docs/ECS_Scheduler_Migration_Plan.md` Step 18-A
- `Docs/Step18_Static_Entity_Batching_Plan.md`

## 完了済み

### 1. RenderWorld所有境界

`RenderWorld`が同じFrame Generationで次を所有する。

- Dynamic Render Packet
- Static Batch候補
- Static Batch Cache
- Static Batch Instance Data
- View Culling結果
- 最終提出Generation

`RenderSystem::Stop()`は`RenderWorld::Reset()`を呼び、Scene TempLoad / Shutdown前にComponentへの非所有Pointerと可視結果を無効化する。

`RenderSystem`側に`RenderPacketFrameBuffer`、`CullingVisibilitySet`、提出Generationの重複Storageは残さない。

### 2. Camera PostEffect GPU Runtime

`CameraPostEffect`はGraphと永続設定だけを保持する。
Inspector Preview互換用に、参照Countを持たない`uintptr_t`ベースの一時Handleだけを保持する。
Native D3D型とResource所有権はComponentへ戻さない。

Componentから撤去したもの:

- `ID3D11Texture2D`
- `ID3D11RenderTargetView`
- `ID3D11ShaderResourceView`
- Texture生成・Resize・Clear処理

`PostEffectPass`が`CameraPostEffectRuntimeStorage`を所有し、次のKeyでRuntimeを解決する。

```text
Scene Context ID + Camera Entity + Effect Index
```

契約:

- Texture / RTV / SRVは一時`ComPtr`へ生成する
- 3資源すべて成功した後だけ旧Runtimeと交換する
- Resize失敗時は以前の正常なRuntimeを継続使用する
- Nodeへ渡すMip数と解像度は実際に使用中のRuntime値を使う
- Effect削除時は同Cameraの未使用RuntimeをFrame末に破棄する
- 同Scene内のCamera切替ではRuntimeを維持し、非Active CameraのInspector Previewを有効に保つ
- Scene Context失効時に、そのSceneの全Camera Runtimeを破棄する
- Pass Finalize時はStorageをResetする
- Preview HandleはPassが毎回Reset / 再公開し、Serializationしない

### 3. ModelRenderer Dynamic Vertex Buffer Runtime

`ModelRendererComponent`はModel設定とEntity固有CPU Animation状態だけを保持する。

Componentから撤去したもの:

- `std::vector<ID3D11Buffer*> dynamicVertexBuffers`
- `CreateBuffer`
- 手動`Release`

`RenderSystem`が`ModelRendererGpuRuntimeStorage`を所有し、次のKeyでRuntimeを解決する。

```text
Scene Context ID + Entity packed value
```

契約:

- Modelの全Mesh用Dynamic Vertex Bufferを一時`ComPtr`へ生成する
- 全Buffer成功後だけ旧Runtimeと交換する
- `modelRuntimeRevision`不一致時は古いRuntimeを使用しない
- Animation UploadとRenderableModelは同じStorageを参照する
- Pose再計算待ちのFrameは最後に成功したDynamic Bufferを維持する
- Scene Unload、Entity削除、Animation解除で未使用になったRuntimeをFrame境界で破棄する
- Animation Upload TaskはStorageへのWrite Accessを宣言する
- RenderSystem Stop時に全RuntimeをResetする

### 4. Static Batch Model Geometry Provider / Runtime Storage

Static BatchのGroup Resolver / Geometry Binding Cacheから、`ModelData`内のNative Vertex / Index Buffer配置への直接依存を除いた。

Provider境界:

- `IStaticBatchModelGeometrySourceProvider`を追加
- `StaticBatchModelCpuGeometrySourceProvider`が`ModelData::MeshGeometry`をbyte spanとして公開
- ResolverはProviderから検証済みGeometry Sourceだけを受け取る
- Geometry Binding CacheへProvider注入Overloadを追加
- Animation Group拒否、Skinned SubMesh拒否、Geometry Resource Key一致検証を維持
- Packet Build時に確定した`group.key.geometryKey`をProviderへ明示的に渡す
- Provider / Runtime Storage内では`StaticBatchResourceKey::MakeGeometryKey(packet)`を再実行しない
- Providerは`ModelData::VertexBuffer / IndexBuffer`を参照しない

Runtime Storage:

- `StaticBatchUploadSystem`が`StaticBatchRuntimeModelGeometrySourceProvider`を所有する
- Provider内部の`StaticBatchModelGeometryRuntimeStorage`がModel Geometry Entryを保持する
- EntryはVertex / IndexのCPU byte snapshotを独立した`vector<byte>`へ複製する
- Model識別は`weak_ptr<ModelData>`、`modelRuntimeRevision`、SubMesh Scopeで行う
- 同一Keyが同じ同期内に別Model実体へ衝突した場合、先に採用したEntryを維持して後続を拒否する
- `BeginSynchronization / EndSynchronization`間で使用されたKeyだけを維持し、未使用Entryを解放する
- Geometry Synchronize Taskは`StaticBatchModelGeometryRuntimeStorage`へのWrite Accessを宣言する
- `Stop / Finalize`でGeometry Binding CacheとRuntime Storageを破棄する
- Entry数、Import、Reuse、Replacement、Release、RejectをTelemetryとして公開する

RHI Geometry生成:

- Geometry SourceはCPU byte spanを主経路とする
- `StaticBatchD3D11GeometryBinding`が`IRHIDevice::CreateBuffer`を使用する
- Vertex / Index Bufferは`ResourceUsage::Immutable`で生成する
- Vertex Bufferの初期状態は`VertexBuffer`
- Index Bufferの初期状態は`IndexBuffer`
- CPU dataが存在しないSourceに限り、従来のD3D11 Native Importを互換Fallbackとして使用できる
- Static BatchのModel Provider / Runtime StorageからはNative Import経路へ入らない

このため、Static Batch Model Geometryについては`ModelData`のLegacy Native Vertex / Index BufferをBootstrapする依存を撤去済み。

### 5. Model CPU Geometry Source

`ModelMeshGeometryCpuData`を追加し、Assimp Meshから抽出した共有GeometryをNative Bufferとは独立して保持する。

```text
ModelData::MeshGeometry[SubMesh]
    vertices : vector<VERTEX_3D>
    indices  : vector<uint32_t>
```

契約:

- Model Loaderは一時`new[]`配列を作らない
- Vertex / IndexのCPU Snapshotを先に構築する
- 移行中の通常描画用D3D11 Vertex / Index Bufferも同じSnapshotから初期化する
- Static Batch Geometry CountはAssimp Face CountではなくSnapshotの要素数から取得する
- Geometry Resource Keyは`VertexBuffer / IndexBuffer`配列サイズへ依存しない
- Geometry Resource KeyへSnapshotのVertex / Index件数を含める
- `MeshGeometry.size()`とAssimp SubMesh数が一致しないModelはStatic Geometry Keyを生成しない
- Native Bufferが存在しなくてもCPU SnapshotだけでGeometry Keyを決定できる

通常描画互換のNative Buffer生成・破棄はまだ`modelLoader.h / ModelData::Release`に残るが、Active通常描画は共有RHI Geometryを優先する。Legacy BufferはRHI Runtime生成失敗時のFallbackとしてのみ残す。

### 6. Render Packet Extraction Task

`RenderSystem::BuildRenderPackets()`に埋め込まれていたComponentRegistry走査を`RenderWorldExtraction`へ分離した。

Active経路:

```text
RenderSystem.Packet.Build
    -> RenderWorldExtractionTaskRegistrar
    -> RenderSystem::BuildRenderPackets
    -> RenderWorldExtraction::Extract
    -> RenderWorld::BeginFrame / Publish
```

契約:

- Sceneは`contextID`、同値時はScene名で安定ソートする
- EntityはPacked Valueで安定順序を決定する
- Packetの`stableSequence`をExtraction内で一意に採番する
- Transform Snapshot、Component Binding、Layer / Pass / Sort Key生成をExtraction責務とする
- Environment Map EntityはShadow Passを除外する
- `RenderSystem`はSceneManager入力、Generation更新、Task起動だけを担当する
- TaskのComponent / Resource Access宣言は`RenderWorldExtractionTaskRegistrar`へ集約する
- Active `renderSystem.cpp`はComponentRegistryを直接走査しない
- Active Submitは`RenderWorld::MarkSubmitted()`を直接使用する

巨大な旧実装は挙動保持のため`RenderSystemLegacyImplementation.inl`へ隔離した。旧Build / Submit / RegisterTasksはActive Taskから呼ばれない。Legacy Facadeと隔離実装の物理削除は次工程で行う。

### 7. 通常Renderable共有RHI Geometry Runtime

`ModelGeometryRuntimeStorage`を追加し、通常`RenderableModel`を`ModelData::MeshGeometry`から生成した共有RHI Bufferへ接続した。

同期経路:

```text
RenderSystem.Packet.Build
    -> RenderSystem.ModelGeometry.Synchronize
    -> ModelGeometryRuntimeStorage::Synchronize
    -> IRHIDevice::CreateBuffer
    -> RenderSystem.Command.Submit
```

所有単位:

```text
ModelData identity -> SubMesh[] -> Vertex Buffer + Index Buffer
```

契約:

- Model単位で全SubMeshのVertex / Index Bufferをトランザクション生成する
- Bufferは`ResourceUsage::Immutable`で生成する
- 同一ModelDataを参照するEntity間でGeometry Runtimeを共有する
- 同一Generation内の重複Packetは一度だけ同期する
- 次Generationで再利用されたModelは既存Handleを維持する
- 未使用ModelはGeneration末にVertex / Index Bufferを破棄する
- Static Modelは共有RHI Vertex / Index Bufferを使用する
- Animated ModelはEntity単位Dynamic Vertex Bufferと共有RHI Index Bufferを組み合わせる
- `D3D11RHIDevice::NativeBuffer`はRHI所有権を移さない非所有Interopとしてのみ公開する
- 通常Renderableは共有RHI Geometryを優先し、Legacy Native Bufferは生成失敗時Fallbackに限定する
- `RenderSystem::Stop()`で共有Geometry RuntimeをResetする
- Synchronize TaskはModel、Packet Buffer、Geometry Storage、Graphics ContextのAccessを宣言する

## 回帰テスト

追加済み:

- `RenderWorld Foundation Smoke Test`
- `RenderWorld Extraction Smoke Test`
- `Camera Post Effect Runtime Smoke Test`
- `Model Renderer GPU Runtime Smoke Test`
- `Model Geometry Runtime Storage Smoke Test`
- `Static Batch Model Runtime Boundary Smoke Test`
- `D3D11 Static Batch Interop Smoke`

検証内容:

- ComponentへNative D3D型が戻っていないこと
- Runtime所有者がPass / RenderSystemであること
- Transactional GPU Resource Commit
- Generation / Reset / Camera切替 / Scene失効 / Entity削除契約
- Inspector Previewの非所有Handle契約
- Animation UploadとRenderableのRuntime参照一致
- SchedulerのRuntime Storage Write Access
- Static Batch Resolver / CacheがModelData Native Bufferを直接参照しないこと
- Model CPU ProviderがNative Bufferを参照しないこと
- Runtime StorageがCPU Geometryを独立複製すること
- Provider注入経路がGeometry Binding Cacheまで接続されていること
- 同期開始 / 終了と未使用Entry解放契約
- 確定済みGroup Geometry KeyがProviderへ伝播すること
- Provider / Runtime StorageでGeometry Keyを再生成しないこと
- LoaderがCPU Geometry Snapshotを構築すること
- Native Bufferが空でもCPU Snapshotから決定的なGeometry Keyを生成できること
- CPU SnapshotのVertex / Index件数変更でGeometry Keyが変わること
- WARP上でCPU byte dataからImmutable RHI Vertex / Index Bufferを生成できること
- CPU生成BindingのBind / Matches / Release契約
- Native Buffer Import互換Fallbackが維持されること
- Extraction TaskのAccess集合、Domain、Phase、Thread Affinity
- 空WorkerのPublishでRenderWorld Generation / Ready状態が更新されること
- Active Build / Submit経路がRenderWorld APIへ直接接続されること
- ComponentRegistry直接走査が隔離済みLegacy実装だけに残ること
- 共有Model Geometry Runtimeの生成、再利用、世代解放
- RHI Buffer HandleからD3D11 Bufferへの非所有Interop
- 通常Renderableが共有RHI Vertex / Index Bufferを優先すること
- Animated ModelがDynamic Vertex + Shared Indexを使用すること
- Model Geometry Synchronize TaskのAccess集合と実行順序

## Step 18-A残作業

- [x] RendererのComponentRegistry直接走査を専用Extraction Taskへ分離
- [ ] `renderSystem.cpp`の隔離済み旧Build / Submit / RegisterTasksと一時Facadeを削除
- [x] Active Build / Submitを`RenderWorld` APIへ直接接続
- [x] Static Batch Model Geometry Runtime Storageを追加し、Geometry Cacheへ接続
- [x] Model共有GeometryのBackend非依存CPU Sourceを抽出
- [x] Static Batch RHI GeometryをCPU Sourceから直接生成
- [x] Static BatchのLegacy Native Geometry Bootstrapを撤去
- [x] 通常Renderableを共有RHI Geometry Runtimeへ接続
- [ ] 通常RenderableのLegacy Native Geometry Fallbackを撤去
- [ ] `ModelData`のLegacy Native Geometry生成・破棄を撤去
- [ ] Native API型をRenderSystem公開境界から段階的に撤去
- [ ] RenderWorldからRHI Commandを生成する境界を追加
- [ ] Windows Debug / Release x64 Build
- [ ] Player / Editor PostEffect Graph実機確認
- [ ] GPU / CPU Skinning Model実機確認
- [ ] Static Model GBuffer / Shadow実機確認
- [ ] Scene Reload / Play / Stop / Model Reload時のResource解放確認

## 次の実装単位

1. 隔離済み旧Build / Submit / RegisterTasksとRenderWorld互換Facadeを削除
2. 通常RenderableのLegacy Native Geometry Fallbackを撤去
3. `ModelData`のLegacy Native Geometry生成・破棄を撤去
4. Render PacketのComponent Pointer依存をSnapshot / Handleへ縮小
5. RHI Command生成境界へ接続

Static Batch経路は既存Dynamic Packetを置き換えず、同じRender Passへ併存提出する方針を維持する。
