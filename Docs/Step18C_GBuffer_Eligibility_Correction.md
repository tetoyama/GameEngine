# Step 18-C GBuffer Eligibility Correction

## 発見した不整合

既存の`RenderPacket`契約では`RenderPacketKind::Mesh`はForward専用である。

- `RenderPacketPassesForKind(Mesh)`は`Forward`だけを返す
- `ResolveRenderPacketLayer`はMeshを`Transparent3D`へ正規化する
- `RenderableMesh`はTriangle Stripによる非Index描画を行う

したがって、`Mesh`を初期Static GBuffer `DrawIndexedInstanced`対象にする以前の方針は、現在のPass契約およびGeometry契約と一致しない。

## 修正後の初期対象

最初のGBuffer Static Instancing対象は、次をすべて満たす`Model` Packetとする。

- `RenderPacketKind::Model`
- `RenderLayer::Opaque3D`
- `GBuffer` Pass Maskを持つ
- Model Resourceが有効
- SubMesh数が1
- Vertex BufferとIndex Bufferが各1個以上存在する
- 対象SubMeshがBoneを持たない
- Entity固有AnimationおよびDynamic Vertex Bufferを使用しない
- 永続Geometry Resource Keyが有効
- Static Instance Groupの代表Packet Indexが有効

この制限により、既存Model描画結果と同じTriangle List / UInt32 Index契約を維持したまま、最小のStatic GBuffer経路を構築できる。

## 現時点で対象外とするもの

### Mesh Packet

MeshはForward専用かつ現状Triangle Strip / 非Index描画であるため、GBuffer Static Instancingへ流さない。

将来対応する場合は次のどちらかを明示的に実装する。

1. Forward用Static Instance Pipelineを追加する
2. Meshの通常描画契約をIndexed Triangle Listへ移行する

暗黙にGBufferへPass変更することは禁止する。

### 複数SubMesh Model

現在のModel PacketはModel全体を1 Packetとして保持する一方、実描画はSubMeshごとにVertex / Index BufferとMaterialを切り替える。

そのため、複数SubMesh Modelは次が完了するまで対象外とする。

- SubMesh単位Render Packet
- SubMesh単位Geometry Resource Key
- SubMesh単位Material / Texture Key
- SubMesh単位Representative PacketまたはDraw Record

### Skinned Model

Static Batch専用Vertex Shaderは共有Static Vertex Bufferを入力とする。Entity固有Skinning結果を扱わないため、Boneを持つModelとDynamic Vertex Bufferを使用するModelは対象外とする。

## Fallback契約

Eligibilityを一つでも満たさないGroupは、Static Instance Bufferへデータが存在していてもStatic Drawへ提出しない。

通常Render Packetを削除せず、既存描画経路へ残す。Static Draw成功が確認されたGroupだけを後工程で通常提出から除外する。

## 次工程

1. D3D11 Native Buffer Import実機Smoke Test
2. 単一SubMesh Model用Geometry Source Resolver
3. Geometry Resource Key単位のBinding Cache
4. View可視性を含むGroup Eligibility判定
5. GBuffer `DrawIndexedInstanced`
6. Static Draw成功Groupだけ通常Packet提出から除外
