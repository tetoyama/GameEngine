# Step 18-C Static GBuffer Submission Foundation

## 目的

Static BatchのCPU GroupとGPU Instance Bufferを、既存GBufferへ安全に接続するための提出境界を固定する。

この段階では通常RenderPacket描画を削除しない。Static Drawが完全に成功したGroupだけを後段で通常描画から除外する。

## 初期対象の修正

既存`RenderPacketKind::Mesh`は次の契約を持つ。

- Forward Pass専用
- `Transparent3D`へ正規化
- Triangle Stripによる非Index描画

したがって、MeshをGBuffer `DrawIndexedInstanced`の初期対象にはしない。

初期対象は次を満たすModel Packetとする。

- `RenderPacketKind::Model`
- `RenderLayer::Opaque3D`
- GBuffer Pass対象
- 単一SubMesh
- 非Skinned
- Animation / Dynamic Vertex Bufferなし
- Vertex / Index Bufferが有効
- Geometry Resource Keyが一致

複数SubMesh ModelはSubMesh単位Packet化まで対象外とする。

## 完了した基盤

### Native Geometry Interop

- 既存D3D11 Vertex / Index BufferをRHI HandleへImport
- 作成元Device一致検証
- Bind Flags / Stride / Byte Size検証
- `ComPtr`追加参照による寿命保持
- 途中失敗時Rollback
- Geometry Resource Key単位Binding Cache
- 単一SubMesh Model用Geometry Resolver

### Pipeline Bootstrap

- Static Batch専用VS / GBuffer PS
- `Directory.Build.targets`によるCSO生成
- CSO Bytecode Loader
- Shader / Pipeline State生成
- GBuffer 6 Target + D32 Pipeline Layout
- 欠損CSO時は通常描画を維持してStatic Pipelineだけ無効化

### Visibility

Group可視性は次の保守的契約とする。

- 全Instance可視: Instanced Draw候補
- 全Instance不可視: Group全体をSkip可能
- 可視・不可視混在: 通常Packet描画へFallback
- 無効Range / Context不整合: 通常Packet描画へFallback

Instance Bufferの部分詰め直しは初期実装では行わない。

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

すべてのBindと`DrawIndexedInstanced`が成功した場合だけ成功扱いとする。

### GBuffer Target Bridge

既存GBufferのNative D3D11 Viewを二重所有せず、明示的に検証・BindするBridgeを追加した。

検証内容:

- 5個の`R16G16B16A16_FLOAT` RTV
- 1個の`R32G32B32A32_UINT` RTV
- `D32_FLOAT` DSV
- 全TargetのDevice一致
- Width / Height / Sample Count / Quality一致
- Render Target / Depth Stencil Bind Flag

### 成功Packet追跡

`StaticBatchPacketCache`はInstance順に次を保持する。

- Entity
- RenderPacket Index
- Transform Snapshot

Static Draw成功後は`StaticBatchPacketReplacementSet`へGroup全体を原子的に登録する。

- Group内に不正Packet Indexが一つでもあれば何も登録しない
- 成功GroupのPacketだけ通常描画から除外する
- Draw失敗Group、混在可視Group、Material非対応Groupは通常描画へ残す

## 未完了

### Material / Texture同等性

既存`RenderableModel`はSubMeshごとに次を解決する。

- MaterialComponent
- Assimp Diffuse Color
- Model内Diffuse Texture
- TextureComponent Override
- UV Animation
- Normal Map
- Material Flags

Static Batch専用PSは現時点でNormal Mapを再現していない。

初回GBuffer提出では、通常描画と結果が一致すると証明できるMaterial / Texture条件だけを対象にする必要がある。

### GBuffer Pass統合

残る作業:

1. Representative PacketからMaterial Stateを解決
2. Static Pipeline / Geometry / Instance Buffer / GBuffer Viewを検証
3. Group Visibilityを評価
4. `DrawIndexedInstanced`を提出
5. 成功GroupをReplacement Setへ登録
6. 通常Packet LoopでReplacement Set登録PacketだけをSkip
7. Draw Call数とFallback理由をTelemetryへ追加

## Compile Gate

実描画統合へ進む前に次が同一HEADで成功することを必須とする。

- Windows Build Debug / Release x64
- Script Build Debug / Release x64
- Static Batch Foundation Smoke Test
- D3D11 Static Batch Interop Smoke
- RHI Smoke Test
- D3D11 Real Triangle Smoke
- FXC Static Batch Shader Contract

CIがRunner待ちの場合は成功扱いにしない。
