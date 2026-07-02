# Step 18-C Static GBuffer Submission Foundation

## 目的

Static BatchのCPU GroupとGPU Instance Bufferを、既存GBufferへ安全に接続するための提出境界を固定する。

通常RenderPacketはFallbackとして維持し、Static DrawとGraphics Queue Submitが成功したGroupだけを通常描画から除外する。

## 初期対象の修正

既存`RenderPacketKind::Mesh`は次の契約を持つ。

- Forward Pass専用
- `Transparent3D`へ正規化
- Triangle Stripによる非Index描画

したがって、MeshをGBuffer `DrawIndexedInstanced`の初期対象にはしない。

初期対象は次を満たすModel SubMesh Packetとする。

- `RenderPacketKind::Model`
- `RenderLayer::Opaque3D`
- GBuffer Pass対象
- 1 Packetが1 SubMeshを指す
- 非Skinned
- Animation / Dynamic Vertex Bufferなし
- Vertex / Index Bufferが有効
- Geometry Resource Keyが一致
- 可視Instanceが2個以上

複数SubMesh ModelはFrame Buffer公開境界でSubMesh単位Packetへ展開する。

## 完了した基盤

### Model SubMesh Packet化

`RenderPacket`へSubMesh Scopeを追加した。

- `RenderPacketAllSubMeshes`: Legacy全Scope
- `subMeshIndex`: 特定SubMesh Scope
- `TargetsAllSubMeshes()` / `TargetsSubMesh()`
- Sort Tie BreakerへSubMesh Indexを追加

Frame Buffer公開境界では次の契約を適用する。

- ロード済みModel: `mNumMeshes`個のModel Packetへ展開
- 単一SubMesh Model: index 0のScoped Packetへ正規化
- 未ロード／不完全Model: Legacy全Scope Packetを維持
- 展開後Packet数をReserve / Growth Telemetryへ反映
- 通常描画、Shadow、Forward、GBufferが同じScoped Packetを利用

`RenderableModel`はLegacy全Scopeなら従来どおり全Meshを描画し、Scoped Packetなら指定Meshだけを描画する。

Static Batch側では次をSubMesh Scopeへ対応した。

- Geometry Resource Key
- Texture Set Key
- Material State Key
- Geometry Resolver
- Material Resolver
- Geometry Binding Cache Group

これにより複数SubMesh Modelも、SubMeshごとにGeometry / Materialが一致するInstance群としてBatch化できる。

### Native Geometry Interop

- 既存D3D11 Vertex / Index BufferをRHI HandleへImport
- 作成元Device一致検証
- Bind Flags / Stride / Byte Size検証
- `ComPtr`追加参照による寿命保持
- 途中失敗時Rollback
- Geometry Resource Key単位Binding Cache
- Scoped Model SubMesh用Geometry Resolver

### Pipeline Bootstrap

- Static Batch専用VS / GBuffer PS
- `Directory.Build.targets`によるCSO生成
- CSO Bytecode Loader
- Shader / Pipeline State生成
- GBuffer 6 Target + D32 Pipeline Layout
- 欠損CSO時は通常描画を維持してStatic Pipelineだけ無効化

### Material / Texture同等性

通常`RenderableModel`と同じ順序で次を解決する。

- MaterialComponent
- 選択SubMeshのAssimp Diffuse Color
- 選択SubMeshのModel内Diffuse Texture
- TextureComponent Override
- UV Slice / Animation Frame
- Material Flags
- Shader ID
- GBuffer Alpha除外規則

追加契約:

- TextureComponentが存在してもOverride Texture未設定ならModel内TextureへFallback
- UV Start / EndをMaterial State Keyへ含める
- Override / Model TextureのGPU SRV未生成時は通常描画へFallback
- Normal Mapを参照するGroupは初期Static Draw対象外
- Packet由来Material解決とAssimp Material解決を分離し、単体TestはAssimp Linkへ依存しない

### View別Visibility再圧縮

Player / Editor GBuffer Passごとに独立した可視Instance選択とGPU Bufferを所有する。

`StaticBatchVisibleInstanceBuffer`はSource Groupを次のように再構築する。

- 全Instance可視: GroupをそのままView-local Groupへコピー
- 全Instance不可視: GroupをView-local Bufferから除外
- 可視・不可視混在: 可視Instanceと対応Packet Indexだけを連続領域へ再圧縮
- 無効Group Partition / Packet Mapping: 全Static提出を中止し、通常Packet描画へFallback
- Growth禁止時Capacity不足: OverflowとしてStatic提出を中止

再圧縮結果は次を保持する。

- View-local `StaticBatchInstanceGroup`
- View-local `StaticBatchInstanceData`
- 可視Packet Index
- 元Source Group Index
- Source Revision
- View Revision
- Visibility Mask込みSelection Revision

元Source Group Indexを保持することで、再圧縮後も`StaticBatchGeometryBindingCache::FindByGroupIndex`から正しいGeometry Bindingを取得する。

View Revisionは次を組み合わせる。

- `CullingVisibilitySet::FrameSerial()`
- Camera Entity
- `CullingViewKind`
- View Instance ID

Visibility View未構築・Overflow時は既存`ShouldRender`契約により全描画Fallbackとなる。

### Draw Submission Boundary

`StaticBatchDrawSubmissionDesc`が次を一つの提出単位として保持する。

- Pipeline State
- Slot 0 Vertex Buffer
- Slot 1 Instance Buffer
- Index Buffer
- Vertex Stride
- Index Format
- Index Count
- Instance Count
- First Instance

すべてのBindと`DrawIndexedInstanced`が成功した場合だけ提出候補とする。

### GBuffer Target Bridge

既存GBufferのNative D3D11 Viewを二重所有せず、明示的に検証・BindするBridgeを追加した。

検証内容:

- 5個の`R16G16B16A16_FLOAT` RTV
- 1個の`R32G32B32A32_UINT` RTV
- `D32_FLOAT` DSV
- 全TargetのDevice一致
- Width / Height / Sample Count / Quality一致
- Render Target / Depth Stencil Bind Flag

### GBuffer Pass統合

GBuffer Passで次を実装した。

1. Static Pipeline / Geometry Cache / GBuffer Viewを検証
2. Source Packet Cache / CPU Instance Data Revisionを検証
3. Player / Editor Viewごとに可視Instanceを再圧縮
4. Selection Revision単位でView-local GPU Instance BufferへUpload
5. 元Source Group IndexからGeometry Bindingを解決
6. Representative PacketからMaterial / Texture / UV状態を解決
7. `DrawIndexedInstanced`を提出
8. Graphics Queue Submit成功Groupの可視PacketだけReplacement Setへ登録
9. 通常Packet LoopでReplacement Set登録PacketだけをSkip
10. Static Draw後に通常GBuffer Shader / Input Layout / Samplerを復元

混在可視Groupでは、可視PacketだけStatic Drawへ置換し、不可視Packetは通常Loopへ残る。通常Loop側のView Cullingにより不可視Packetは描画されない。

どの段階で失敗しても、該当SubMesh Packetの通常RenderPacketは残る。

### 成功Packet追跡

`StaticBatchPacketCache`はInstance順に次を保持する。

- Entity
- RenderPacket Index
- Transform Snapshot

Packet Mappingの安全契約:

- Source RevisionへRenderPacket Indexを含める
- Entity / Transform / Resource Keyが同一でもPacket順が変わればCacheを再構築する
- CandidateのEntity / Scene / Kind / Layer / Material Keyが参照Packetと一致しなければGroupを拒否する
- Packet CacheとCPU Instance Dataは同一Generation / Source Revisionを共有する

Static Draw成功後は`StaticBatchPacketReplacementSet`へGroup全体を原子的に登録する。

- Group内に不正Packet Indexが一つでもあればDraw前にFallback
- 成功した可視SubMesh Packetだけ通常描画から除外
- Draw失敗Group、単一可視Instance Group、Material非対応Groupは通常描画へ残す

### Picking契約

通常GBufferとStatic GBufferは`MakeGBufferParameter`を共有し、Param TargetのChannel順を固定する。

- x: Scene ID
- y: Entity Index / Object ID
- z: Shader ID
- w: Material Flags

Static Instance入力側もChannel定義を共通化した。

- x: Entity Index
- y: Entity Generation
- z: Scene ID
- w: Reserved

Editor Pickingは既存どおりScene IDとEntity Indexから`EntityRegistry::Resolve`する。GenerationはInstance Payloadに保持するが、既存Param Target互換のため書き込まない。

### Telemetry

GBuffer Passごとに次をFrame単位で保持する。

- Source Revision
- Candidate / Submitted Group数
- View可視 / Culling Instance数
- 再圧縮した混在Group数
- Visibility Selection失敗数
- Submitted Instance数
- 置換した通常Draw Call数
- Static Draw Call数
- 推定Draw Call削減数
- Single Instance Fallback
- Packet Range / Layer Fallback
- 全不可視Group数
- Geometry / Material Fallback
- Draw / Queue / Target Binding失敗
- Pipeline / View-local Instance Upload Ready状態

1 Packetが1 SubMesh Drawに対応するため、推定Draw Call削減数は次で求める。

`Submitted Instance数 - Submitted Group数`

Project Settingsへ`Static Batch`タブを追加し、Player / Editor別に次を表示する。

- View可視 / Culling Instance数
- 再圧縮した混在Group数
- Draw Call削減数
- 各Fallback理由
- Source GPU Upload状態
- Geometry Binding Cache状態

### Smoke Coverage

Static Batch Foundation Workflowは17個のC++ Smoke TestとFXC Shader Contractを実行する。

追加の専用Workflow:

- `Static Batch Picking Contract`
  - C++ Channel契約
  - 通常GBuffer PS
  - Static GBuffer PS
- `Static Batch Visible Instance Contract`
  - 混在Group再圧縮
  - 全不可視Group除外
  - Source Group Index保持
  - Cache Hit時Telemetry保持
  - Group Partition拒否
  - Growth禁止時Overflow

SubMesh関連の専用契約:

- SubMesh Scope / Sort / Range Resolver
- 2 SubMesh ModelのFrame Packet展開
- SubMesh別Static Candidate / Group分離
- 未ロードModelのLegacy全Scope Fallback

## 未完了

- Windows Debug / Release x64のコンパイル確認
- Script Debug / Release x64のコンパイル確認
- Static Batch Foundation全Smoke Test確認
- Static Batch Picking Contract確認
- Static Batch Visible Instance Contract確認
- D3D11 Static Batch Interop実機Smoke確認
- Player / Editor View実機描画回帰
- Telemetry削減値とRenderDoc Draw Call差分の実機照合
- Static Pickingクリック実機回帰
- Static Shadow Batch提出
- Batch Culling展開

## Compile Gate

次が同一HEADで成功するまでStep 18-C実描画統合を完了扱いにしない。

- Windows Build Debug / Release x64
- Script Build Debug / Release x64
- Static Batch Foundation Smoke Test
- Static Batch Picking Contract
- Static Batch Visible Instance Contract
- D3D11 Static Batch Interop Smoke
- RHI Smoke Test
- D3D11 Real Triangle Smoke
- FXC Static Batch Shader Contract

CIがRunner待ち・Cancelの場合は成功扱いにしない。
