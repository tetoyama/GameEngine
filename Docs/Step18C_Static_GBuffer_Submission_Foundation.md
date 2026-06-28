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

初期対象は次を満たすModel Packetとする。

- `RenderPacketKind::Model`
- `RenderLayer::Opaque3D`
- GBuffer Pass対象
- 単一SubMesh
- 非Skinned
- Animation / Dynamic Vertex Bufferなし
- Vertex / Index Bufferが有効
- Geometry Resource Keyが一致
- 2 Instance以上

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

### Material / Texture同等性

通常`RenderableModel`と同じ順序で次を解決する。

- MaterialComponent
- Assimp Diffuse Color
- Model内Diffuse Texture
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

1. Static Pipeline / Geometry / Instance Buffer / GBuffer Viewを検証
2. Instance Upload Source Revisionを検証
3. Group Visibilityを評価
4. Representative PacketからMaterial / Texture / UV状態を解決
5. `DrawIndexedInstanced`を提出
6. Graphics Queue Submit成功GroupだけReplacement Setへ登録
7. 通常Packet LoopでReplacement Set登録PacketだけをSkip
8. Static Draw後に通常GBuffer Shader / Input Layout / Samplerを復元

どの段階で失敗しても、該当Groupの通常RenderPacketは残る。

### 成功Packet追跡

`StaticBatchPacketCache`はInstance順に次を保持する。

- Entity
- RenderPacket Index
- Transform Snapshot

Static Draw成功後は`StaticBatchPacketReplacementSet`へGroup全体を原子的に登録する。

- Group内に不正Packet Indexが一つでもあればDraw前にFallback
- 成功GroupのPacketだけ通常描画から除外
- Draw失敗Group、混在可視Group、Material非対応Groupは通常描画へ残す

### Telemetry

GBuffer Passごとに次をFrame単位で保持する。

- Source Revision
- Candidate / Submitted Group数
- Submitted Instance数
- Single Instance Fallback
- Packet Range / Layer Fallback
- Culled / Mixed Visibility Group数
- Geometry / Material Fallback
- Draw / Queue / Target Binding失敗
- Pipeline / Instance Upload Ready状態

Project Settingsへ`Static Batch`タブを追加し、次を同時表示する。

- GPU Instance Upload状態
- Geometry Binding Cache状態
- Player GBuffer提出結果
- Editor GBuffer提出結果
- 各Fallback理由

## 未完了

- Windows Debug / Release x64のコンパイル確認
- Script Debug / Release x64のコンパイル確認
- Static Batch Foundation全Smoke Test確認
- D3D11 Static Batch Interop実機Smoke確認
- Player / Editor View実機描画回帰
- Draw Call削減量の実機計測
- Model SubMesh単位Packet化
- 部分可視GroupのInstance再圧縮
- Shadow / Picking / Batch Culling展開

## Compile Gate

次が同一HEADで成功するまでStep 18-C実描画統合を完了扱いにしない。

- Windows Build Debug / Release x64
- Script Build Debug / Release x64
- Static Batch Foundation Smoke Test
- D3D11 Static Batch Interop Smoke
- RHI Smoke Test
- D3D11 Real Triangle Smoke
- FXC Static Batch Shader Contract

CIがRunner待ち・Cancelの場合は成功扱いにしない。
