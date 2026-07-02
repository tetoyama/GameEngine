# Culling View Integration Completion

## 目的

`CullingComponent`を明示的にAttachしたEntityだけを対象に、Editor / Player ViewごとのFrustum Cullingを実描画経路へ接続する。

単一の可視FlagをComponentへ保存せず、同じCamera Entityを異なるView用途・解像度で利用しても結果を混同しない。

## 確定した実行順

```text
Transform / Follow / Script / Physics Update
    ↓
CullingSystem.Bounds.Update
    - Model / Mesh / Terrain / Wave Local Bounds供給
    - World Matrix Revision検証
    - World Bounds更新
    ↓
RenderSystem.Packet.Build
    - 全View共通の描画候補Packetを構築
    - Hidden / Disabled / stale Entityを除外
    ↓
PlayerPass / EditorPass
    - ViewProjectionからFrustumを生成
    - View単位Visibility Setを構築
    ↓
GBufferPass / ForwardPass
    - View VisibilityでPacketを絞り込み
    ↓
RenderSystem.Command.Submit
```

## Bounds Source優先順位

同じEntityへ複数Renderable Componentが存在する場合は、次の順序でLocal Boundsを採用する。

```text
Model > Valid Mesh Metadata > Terrain > Wave
```

Providerごとに異なるSource Revision Seedを使用し、別ProviderのRevision値を同一Sourceとして誤認しない。

## Bounds供給

### Static Model

`ModelCullingBoundsProvider`がAssimp頂点列からLocal AABBを生成する。

- 通常Model: `(x, y, z)`
- Blender軸変換Model: `(x, -z, y)`
- Model Resource / aiScene / Path / Blender FlagからSource Revisionを生成
- Source Revisionが変化した場合だけLocal Boundsを再生成

### Mesh

`MeshData`へGPU Bufferと分離したDerived Metadataを追加した。

```cpp
Vector3 localBoundsMin;
Vector3 localBoundsMax;
uint64_t localBoundsRevision;
bool localBoundsValid;
```

Mesh生成側はCPU頂点列を破棄する前に`SetLocalBounds()`を呼ぶ。

- GPU Bufferから頂点を逆読みしない
- Bounds MetadataはScene YAMLへ保存しない
- Mesh Count / Index Count / Metadata RevisionからSource Revisionを生成する
- 非有限値とMin > Maxを拒否する
- Metadata解除時、下位Providerがなければ旧Boundsを無効化する
- Terrain / Waveが同一Entityに存在する場合は、そのFrameの後続ProviderへFallbackする

### Terrain

Terrain Mesh生成式と同じ座標契約を使用する。

- X / Z: `[-0.5, 0.5]`
- Y: HeightMapの最小値 / 最大値
- HeightMap要素数が不足する場合は`Y=0`平面へFallback
- Scale / CurrentScale / HeightMapからSource Revisionを生成する

### Wave

Wave頂点更新式の最大変位を使用する。

- X / Z: `[-1.0, 1.0]`
- Y: `[-abs(Amplitude), abs(Amplitude)]`
- Time / Speed / Wavelengthは最大AABBを変えないためRevisionへ含めない
- Resolution / AmplitudeからSource Revisionを生成する

### Skinned Model

Rest Pose Boundsによる誤Cullingを避けるため、現段階ではLocal Boundsを供給しない。`boundsValid=false`を維持し、安全側で描画候補へ残す。

### 未接続Resource

- Particle
- Effect
- Sprite
- Billboard

Boundsを供給できないResourceは安全側で描画候補へ残す。

## World Bounds更新

`CullingBoundsRuntime`はWorld Matrixの16要素からRevision Hashを生成する。

Source RevisionまたはTransform Revisionが変化した時だけ、Local AABBの8頂点をWorld変換してWorld AABBを再計算する。

Local Bounds未供給、Transformなし、無効Boundsの場合は`boundsValid=false`を維持する。

## Scheduler契約

Task:

```text
CullingSystem.Bounds.Update
```

設定:

- Domain: Render
- Phase: Early
- Priority: -100
- Thread Affinity: AnyWorker

Access:

- Read `TransformComponent`
- Read `ModelRendererComponent`
- Read `MeshRendererComponent`
- Read `TerrainComponent`
- Read `WaveComponent`
- Write `CullingComponent`
- Read `SceneManager`

実行順:

1. Model Bounds Provider
2. Mesh Bounds Provider
3. Terrain Bounds Provider
4. Wave Bounds Provider
5. World Bounds Updater

## View Visibility

View Keyへ次を含める。

- Scene Context ID
- Camera Entity
- View Kind
- Instance ID

Editor / Player / Shadow / Customの可視結果を分離する。

`visibleEntityReserve`をView単位の集合へ適用する。

`allowRuntimeGrowth=false`で容量不足になった場合は部分的な可視集合を公開しない。

```text
Capacity不足
    ↓
Viewをoverflowedへ変更
    ↓
部分集合を破棄
    ↓
HasView=false
    ↓
ShouldRender=trueの安全Fallback
```

## 描画Pass接続

接続済み:

- Player View
- Editor View
- GBuffer Pass
- Forward Pass

意図的に未接続:

- Shadow Map Pass

ShadowはCamera FrustumではなくLight View / Cascade Frustumで判定する。Camera結果を流用すると画面外のShadow Casterを誤除外するため、専用Light View Culling完成までは従来経路を維持する。

## 安全側Fallback

次の場合は描画候補へ残す。

- `CullingComponent`なし
- Bounds未生成
- Bounds Metadata解除
- View Visibility未構築
- Visibility容量不足
- Camera Entityが無効
- Resource Local Bounds未供給
- Skinned Model

Hidden / Disabled / stale Entityは既存Render Packet契約により除外する。

## 検証

専用Smoke Test:

- Culling AABB / Frustum
- View単位Visibility Set
- Visibility容量不足時の全描画Fallback
- Editor / Player / Shadow Key分離
- ViewProjection Frustum抽出
- World Bounds Revision
- Scene Bounds Updater
- Static Model Local Bounds
- Mesh Metadata Bounds / Revision / Invalid値拒否
- Mesh Metadata解除とTerrain Fallback
- Terrain Local Bounds / Flat Fallback / Revision
- Wave Conservative Bounds / Time非依存Revision
- Culling System Task Access
- Render Packet View Culling
- Render Packet Visibility / Reserve

Workflow:

- Resource Culling Bounds Provider Smoke Test
- Culling System Task Smoke Test
- Culling Visibility Growth Fallback

## 実機確認待ち

- Editor / Player Viewの視錐台外描画除外
- Camera移動時のVisibility更新
- Model Resource差替え時のBounds再生成
- Mesh生成側からの`SetLocalBounds()`実接続
- Terrain HeightMap編集時のBounds再生成
- Wave Amplitude変更時のBounds再生成
- Scene Save / Load / TempSave / TempLoad
- Shadow Caster回帰
