# Step 18-A RenderWorld / Runtime Ownership Progress

## 状態

**実装中 — RenderWorld基盤、Camera / ModelRendererのNative描画資源分離、Static Batch Geometry Source Provider境界まで完了。CI・実機確認待ち。**

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

### 4. Static Batch Model Geometry Source Provider

Static BatchのGroup Resolver / Geometry Binding Cacheから、`ModelData`内のNative Vertex / Index Buffer配置への直接依存を除いた。

- `IStaticBatchModelGeometrySourceProvider`を追加
- ResolverはProviderから検証済み`StaticBatchD3D11GeometrySource`だけを受け取る
- Geometry Binding CacheへProvider注入Overloadを追加
- 既存Runtime互換用に`StaticBatchLegacyModelGeometrySourceProvider`を維持
- Legacy Providerだけが移行期間中の`ModelData::VertexBuffer / IndexBuffer`を参照する
- Animation Group拒否、Skinned SubMesh拒否、Geometry Resource Key一致検証を維持

この境界により、次工程でModel共有GeometryをRenderSystem側Runtime Storageへ移してもResolver / Cache / GBuffer / Shadow提出契約を変更せずに差し替えられる。

## 回帰テスト

追加済み:

- `RenderWorld Foundation Smoke Test`
- `Camera Post Effect Runtime Smoke Test`
- `Model Renderer GPU Runtime Smoke Test`
- `Static Batch Model Runtime Boundary Smoke Test`

検証内容:

- ComponentへNative D3D型が戻っていないこと
- Runtime所有者がPass / RenderSystemであること
- Transactional GPU Resource Commit
- Generation / Reset / Camera切替 / Scene失効 / Entity削除契約
- Inspector Previewの非所有Handle契約
- Animation UploadとRenderableのRuntime参照一致
- SchedulerのRuntime Storage Write Access
- Static Batch Resolver / CacheがModelData Native Bufferを直接参照しないこと
- Legacy Providerだけが既存ModelData Geometryを参照すること
- Provider注入経路がGeometry Binding Cacheまで接続されていること

## Step 18-A残作業

- [ ] `renderSystem.cpp`のBuild / Submitを`RenderWorld` APIへ直接接続し、一時Facadeを削除
- [ ] RendererのComponentRegistry直接走査を専用Extraction Taskへ分離
- [ ] Model共有GeometryをRenderSystem側Runtime Storageへ移し、Legacy Providerを置換
- [ ] Native API型をRenderSystem公開境界から段階的に撤去
- [ ] RenderWorldからRHI Commandを生成する境界を追加
- [ ] Windows Debug / Release x64 Build
- [ ] Player / Editor PostEffect Graph実機確認
- [ ] GPU / CPU Skinning Model実機確認
- [ ] Static Model GBuffer / Shadow実機確認
- [ ] Scene Reload / Play / Stop / Model Reload時のResource解放確認

## 次の実装単位

1. `RenderSystem.Packet.Build`をRenderWorld Extraction責務として独立
2. Build / Submitの一時Facade撤去
3. Model共有Geometry Runtime Storageを追加し、Provider実装を差し替え
4. Render PacketのComponent Pointer依存をSnapshot / Handleへ縮小
5. RHI Command生成境界へ接続

Static Batch経路は既存Dynamic Packetを置き換えず、同じRender Passへ併存提出する方針を維持する。
