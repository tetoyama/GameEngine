# RHI Device Ownership / Reset / Abandon and Model Geometry Revision Contract

Status: **Implemented foundation — 2026-08-02**

対象:

- `refactor/ecs-scheduler-foundation`
- PR #45
- `ModelGeometryRuntimeStorage`
- `StaticBatchModelGeometryRuntimeStorage`
- `StaticBatchD3D11GeometryBinding`
- `RenderHardwareInterfaceService`
- `ModelData::MeshGeometry`

---

## 1. 目的

Model Geometry Runtimeが、破棄済みまたは交換済みのRHI DeviceへNative Resource破棄を発行することを防ぐ。

また、ModelDataのPointer、Mesh数、Vertex数、Index数が変化しないGeometry更新でも、通常RenderableとStatic Batchの共有Vertex / Index Runtimeを確実に再生成する。

---

## 2. RHI Device Ownership Epoch

`RenderHardwareInterfaceService`はDevice所有状態が変わるたび、単調増加する`DeviceGeneration`を進める。

Generationが変化する操作:

- `AdoptDevice`
- 所有Deviceが存在する状態での`ReleaseDevice`
- 所有Deviceが存在する状態での`ResetDevice`
- Deviceを破棄する`SelectBackend`
- Deviceを破棄する`Shutdown`

`DeviceGeneration == 0`はDevice未確立を表すInvalid値とする。

同じ`IRHIDevice*`が再度Adoptされた場合でも、新しいOwnership Epochとして扱う。

---

## 3. Device Lifetime Token

`IRHIDevice`は自身の寿命だけを示すLifetime Tokenを提供する。

Runtime Storageは次を保持できる。

- 非所有`IRHIDevice*`
- Device Lifetime Tokenのweak reference
- Device Generation

Device Lifetime Tokenが失効した後は、保存済みDevice Pointerを逆参照してはならない。

Lifetime TokenはDevice所有権を付与しない。Deviceを延命もしない。

---

## 4. ResetとAbandon

### 4.1 Reset

Resetは、StorageがBindingしたものと同一の次の組み合わせでのみNative Destroyを実行する。

```text
IRHIDevice identity
+ Device Lifetime
+ Device Generation
```

Reset成功時:

- Storage所有HandleをDeviceへ返却する
- Entryを削除する
- Device Bindingを解除する

一部Destroyに失敗した場合:

- 失敗したHandleを持つEntryを保持する
- Reset失敗をTelemetryへ記録する
- 後続SynchronizeまたはResetで再試行可能にする

### 4.2 Abandon

AbandonはNative Destroyを呼ばず、Storage側のHandle記録だけを破棄する。

使用条件:

- Device Lifetime Tokenが失効した
- RHI ServiceがDeviceを既に破棄した
- Device Generationが変化し、旧Ownership Epochへ安全に到達できない
- Graphics / RHI Serviceが利用不能になった

Abandonは通常のScene切替や、Deviceが有効な状態の停止処理におけるResetの代用ではない。

---

## 5. Device Transition

`ModelGeometryRuntimeStorage::Synchronize`は、現在Binding中のDevice契約と入力Device契約を比較する。

異なる場合:

1. 旧EntryをAbandonする
2. 新しいDevice Lifetime / GenerationへBindingする
3. 現在のRenderPacketからRuntimeを再生成する

旧Device Handleを新Deviceへ渡してDestroyしてはならない。

RHI共通契約は他のGPU Runtime Storageにも再利用可能だが、各StorageはDevice Binding、Reset、Abandonの採用を個別に行う。今回の実装対象は通常Model Geometry Runtimeである。

---

## 6. Model Geometry Revision

`ModelData`は単調増加するGeometry Revisionを所有する。

```cpp
std::uint64_t GetGeometryRevision() const noexcept;
std::uint64_t MarkGeometryDirty() noexcept;
```

次が変化した場合、更新処理は公開前に`MarkGeometryDirty()`を呼ぶ。

- Vertex内容
- Index内容
- MeshGeometryの追加・削除・並べ替え
- Vertex LayoutまたはStrideの解釈
- Import座標変換の結果
- Tangent / Normal再生成結果
- ReimportによるGeometry差し替え

要素数が同一でもRevisionを進める。

Animation Pose、Material Parameter、SubMesh Material Assignmentだけの変更ではGeometry Revisionを進めない。

`ModelRendererComponent::modelRuntimeRevision`はEntity固有Animation Runtimeの世代であり、共有`ModelData` Geometry Revisionとは別契約とする。

---

## 7. 通常Model Geometry Runtime一致条件

通常Model Geometry Runtimeを再利用できるのは、最低限次がすべて一致する場合だけとする。

- ModelData identity
- Geometry Revision
- Mesh count
- 各MeshのVertex count
- 各MeshのIndex count
- Runtime Handle readiness

Geometry Revision不一致時は、Vertex / Index数が同一でもRuntimeを置換する。

Build中にGeometry Revisionが変化した場合、新Runtimeを公開せず破棄して次回同期へ再試行する。

---

## 8. Static BatchへのRevision伝播

Static BatchはGeometry Resource KeyだけでGeometry内容を識別しない。

ModelData由来Geometryでは次の経路でRevisionを伝播する。

```text
ModelData::Geometry Revision
    -> StaticBatchModelCpuGeometrySourceProvider
    -> StaticBatchD3D11GeometrySource::sourceRevision
    -> StaticBatchModelGeometryRuntimeStorage Entry
    -> StaticBatchD3D11GeometryBinding
```

契約:

- CPU Snapshot EntryはModel identity、Entity側Model Runtime Revision、共有Geometry Revision、SubMesh Scopeで一致判定する
- Snapshot複製中にGeometry Revisionが変化した場合は公開しない
- D3D11 Geometry BindingはGeometry Key、Stride、Countが同一でもSource Revisionが異なれば再利用しない
- Geometry KeyはBatch Group分類用として維持し、Asset内容の更新検知をRevisionへ分離する
- Revision契約を持たないLegacy / Mesh Sourceは`sourceRevision == 0`を許容する

これにより同サイズReimportで通常Renderableだけ更新され、Static Batchが古いGPU Bufferを保持する不整合を防ぐ。

---

## 9. Telemetry

最低限次を観測可能にする。

- Runtime creation
- Runtime reuse
- Runtime replacement
- Geometry Revisionによるreplacement
- Resource release
- Abandoned entry
- Device transition
- Reset failure
- Rejected model

通常Model Geometry RuntimeとStatic Batch CPU Snapshot Runtimeは、Geometry Revisionによる置換を個別に記録する。

---

## 10. 検証条件

- 同じDevice / Generation / Geometry RevisionではHandleを再利用する
- 同じVertex数・Index数でもGeometry Revision更新後はHandleを置換する
- Device Generation変更時に旧DeviceへDestroyを発行しない
- 新Device側へRuntimeを再生成する
- 有効な同一DeviceでのResetはHandleをDestroyする
- Device破棄後のResetは生Pointerを逆参照せずAbandonする
- RHI ServiceのAdopt / Release / Re-adopt / ResetでGenerationが変化する
- Static Batch Bindingは同じGeometry KeyとCountでもSource Revision不一致を拒否する
- Static Batch CPU SnapshotはGeometry Revision変更後に置換される
