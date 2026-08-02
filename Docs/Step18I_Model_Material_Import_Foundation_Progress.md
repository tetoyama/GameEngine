# Step 18-I: Model Material Import Foundation Progress

Status: **MMI-1 / MMI-2 / MMI-3 foundation in progress — 2026-08-02**

対象:

- `refactor/ecs-scheduler-foundation`
- PR #45

関連文書:

- `Docs/Step18I_Model_Asset_Material_Import_Plan.md`
- `Docs/Step18I_Model_Material_Import_Foundation_Plan.md`
- `Docs/RHI_Device_Ownership_And_Model_Geometry_Revision_Contract.md`

---

## 1. 今回実装した範囲

### 1.1 Assimp Material正規化Adapter

`modelAssimpMaterialPropertyNormalization.h`を追加した。

Import Sourceとしての`aiScene / aiMesh / aiMaterialProperty`から、次をEngine共通定義へ変換する。

- `ImportedMaterialDefinition[]`
- `ModelSubMeshDefinition[]`
- `MaterialTextureBinding[]`
- Import Diagnostic[]

Adapterは`aiMaterial::Get*`を呼ばず、`aiMaterialProperty`を直接読み取るHeader-only実装とする。これにより、`ModelData`を参照するRender Extraction / Static Batchの単体SmokeへAssimp実装LibraryのLink依存を伝播させない。

初期対応Semantic:

- Base Color / Diffuse
- Normal
- Bump
- Height / Displacement
- Metallic
- Roughness
- Ambient Occlusion
- Emissive
- Opacity

Material Parameter:

- Base Color
- Metallic
- Roughness
- Opacity
- Emissive Color / Intensity
- Bump Scale
- Double Sided

Texture Binding:

- Source Path
- 正規化Path
- Embedded Texture Index
- UV Channel
- UV Scale / Offset / Rotation
- Blend Strength
- Color Space

### 1.2 Stable Asset-local ID

SubMeshとImported Materialは配列Indexとは別に32-bit Asset-local IDを持つ。

```text
Runtime Geometry Index
    Frame内の高速走査とGPU Geometry選択

ModelSubMeshID
    Scene保存、Material Assignment、Reimport追従
```

IDはSource Identityから決定的に生成し、同一Asset内の衝突はSaltで解決してDiagnosticへ記録する。

UUIDは使用しない。

### 1.3 Packet Material Resolution

Model Packet展開時に次を解決する。

```text
Custom Material Assignment
    MaterialComponent内IDが有効
        -> Custom Material Snapshot

    MaterialComponentまたはIDが壊れている
        -> Imported Model DefaultへFallback

Model Default
    -> ModelData Imported Material

Imported Materialも壊れている
    -> Engine Default Material
```

Packetは次を保持する。

- Runtime Geometry Index
- Stable ModelSubMeshID
- 解決済みMaterial Descriptor
- Imported / Custom Material ID
- Resolution Source
- Primary Issue
- Fallback Issue
- Fallback使用有無

Custom MaterialはFrame-owned Snapshotへ複製し、Imported MaterialはPacketが所有する`ModelData`内定義を参照する。

### 1.4 SubMesh Render State

正規化済みSubMesh Packet展開では、`ModelRendererComponent::subMeshes`から次を適用する。

- `visible == false`: Packetを生成しない
- `castShadow == false`: Shadow Pass Maskを除外
- Material Source / Custom Material ID: Resolverへ渡す

---

## 2. 検証契約

`ModelMaterialFoundationSmokeTest`:

- Texture Path正規化
- Embedded Texture Index
- SemanticごとのColor Space
- Stable Local IDとCollision Diagnostic
- Imported Material解決
- Custom Material解決
- Broken Custom IDのImported Default Fallback
- MaterialComponent欠落時Fallback
- Imported Material欠落時Engine Default Fallback

`RenderPacketModelSubMeshExpansionSmokeTest`:

- Stable ModelSubMeshIDとGeometry Indexの分離
- SubMesh配列順とGeometry配列順が異なる場合のPacket展開
- PacketへのImported Material Snapshot
- Material Shader Keyの更新
- Legacy Component PointerなしでのMaterial解決

回帰確認:

- Header-only化前はMesh-only `StaticBatchFrameBufferIntegrationSmokeTest`へAssimp Link依存が伝播し失敗した
- `aiMaterialProperty`直接読取へ変更後、同Smokeは成功へ復帰した

---

## 3. 意図的に未実装の範囲

今回の範囲には含めない。

- Texture Asset ServiceへのImport委譲
- Embedded Texture BinaryのAsset化
- RHI Texture / Texture View Runtime
- 全Material Texture SlotのShader Binding
- Static Batch Resolverからの`aiMaterial`参照撤去
- 通常`RenderableModel`からの`aiMaterial`参照撤去
- YAML / Inspector
- Reimport対応表の永続化
- MaterialAsset YAML
- Tangent `float4` / Bitangent Sign
- UV1以降のVertex Layout

---

## 4. 次工程

```text
1. Packet Material DescriptorをLegacy MATERIALへ変換するBridgeを追加
2. RenderableModelの色・Material StateをPacket Snapshotから取得
3. StaticBatchModelMaterialResolverをPacket Snapshotへ移行
4. Texture Runtime Handle / RHI Binding境界を設計・実装
5. Draw中のaiMaterial参照を完全撤去
```

Assimpは最終的にImport Adapter内部だけで参照し、Render Extraction以降へ`aiMaterial*`を渡さない。
