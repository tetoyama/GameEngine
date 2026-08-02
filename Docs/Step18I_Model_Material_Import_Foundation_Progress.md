# Step 18-I: Model Material Import Foundation Progress

Status: **MMI-1 / MMI-2 / MMI-3 runtime・persistence・inspector foundation implemented; CI verification pending — 2026-08-03**

対象:

- `refactor/ecs-scheduler-foundation`
- PR #45

関連文書:

- `Docs/Step18I_Model_Asset_Material_Import_Plan.md`
- `Docs/Step18I_Model_Material_Import_Foundation_Plan.md`
- `Docs/RHI_Device_Ownership_And_Model_Geometry_Revision_Contract.md`

---

## 1. 実装済み範囲

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
- Ambient Occlusion
- Opacity
- Emissive Color / Intensity
- Normal Scale
- Height Scale
- Alpha Mode / Alpha Cutoff
- Cull Mode
- Depth Write
- Receive Shadow

Texture Binding:

- Source Path
- 正規化Asset Path
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

旧単一`MaterialComponent::ShaderID / MATERIAL`はCustom Materialと同一視せず、Imported Defaultへ重ねる明示的なLegacy Overrideとして維持する。

### 1.4 Legacy D3D11 Bridge

`ModelMaterialLegacyD3D11Bridge`を追加した。

解決済み`MaterialDescriptor`から次を生成する。

- Legacy `MATERIAL`定数
- Shader ID
- Base Color / Normal-Bump / Roughness / Metallic / AO / Height / Emissive Texture参照
- Runtime Bindingに基づくLegacy Texture Flag

通常Model Drawは描画時の色・Material State・Texture選択に`aiMaterial`を使用せず、Packet Material Snapshotを使用する。

Static BatchもPacket Material Snapshotを優先し、Snapshot欠落時だけ旧Assimp経路へFallbackする。

### 1.5 SubMesh Render State

正規化済みSubMesh Packet展開では、`ModelRendererComponent::subMeshes`から次を適用する。

- `visible == false`: Packetを生成しない
- `castShadow == false`: Shadow Pass Maskを除外
- Material Source / Custom Material ID: Resolverへ渡す

Static Batch Resource Keyには次を含める。

- Material Resolution Source
- Imported / Custom Material ID
- Material Parameter / Render State
- Texture Set
- Stable ModelSubMeshID
- Geometry Revision

MaterialやSubMeshが異なるPacketを同一Batchへ誤統合しない。

### 1.6 Material Alpha Mode / Pass Routing

`ModelMaterialPassRouting`を追加し、通常描画とStatic Batchで同一判定を使用する。

- `Blend`
  - Opaque / Background Model PacketをGBufferから除外
  - Sorted Forwardへ昇格
  - Base Color Alphaが1でもAlpha Modeを優先
- `Masked`
  - Deferred / Shadowを維持
  - Static Batch GBuffer ShaderのAlpha discardを使用
- 明示`Transparent3D / SortTransparent3D / OverlayUI`
  - Entity LayerをMaterialが上書きしない
- Packet Snapshot欠落時
  - Legacy Base Color Alpha判定を防御的Fallbackとして使用

### 1.7 YAML Persistence

`modelMaterialYamlSerialization.h`を追加し、Schema Version 1として次を保存する。

`MaterialComponent`:

- 旧`ShaderID / Material`を後方互換として維持
- `CustomMaterialEntry[]`
- `MaterialDescriptor`
- Texture Binding
- Material Render State

`ModelRendererComponent`:

- Stable `ModelSubMeshID`ごとのSparse Override
- Visible
- Cast Shadow
- Material Source
- Custom Material ID

読み込み時には次を行う。

- 0 IDを拒否
- 重複IDは先頭だけ採用
- 壊れたEnum / 数値を既定値へFallback
- 非有限値を置換
- 0..1 ParameterをClamp
- Texture Color Space欠落時はSemanticから既定値を決定

### 1.8 Reimport State Synchronization

`ModelSubMeshStateSynchronization`を追加した。

同一Model AssetのReload / Reimport時:

- Stable ModelSubMeshIDで既存Overrideを継承
- 削除されたSubMeshの状態を除去
- 新規SubMeshへDefault Stateを追加
- Import後のSubMesh順へ並べ替え
- 重複 / 無効IDを除去
- 無効Custom Material AssignmentをModel Defaultへ戻す

一時的なResource Service不足またはModel Load失敗では、YAMLから復元したOverrideを破棄しない。

別Model Pathへ変更した場合はOverrideを明示的にClearし、別Assetへ偶然同じLocal IDが存在しても状態を持ち越さない。

### 1.9 Custom Material Collection API

`CustomMaterialCollection`と`MaterialComponent`公開APIを追加した。

- Stable ID割当
- Add
- Remove
- Find
- Sanitize
- Invalid / Duplicate ID除去
- 空Nameの既定名生成
- `uint32_t`最大ID到達時の空きID探索

SubMeshが削除済みCustom Material IDを参照した場合は、既存Resolver契約によりImported DefaultへFallbackする。

### 1.10 Inspector Foundation

`MaterialDescriptorInspector`を追加した。

`MaterialComponent` Inspector:

- Custom Material Add / Remove
- Stable ID表示
- Name
- Shader
- Base Color
- Metallic / Roughness / Ambient Occlusion
- Emissive Color / Intensity
- Opacity / Normal Scale / Height Scale
- Alpha Mode / Alpha Cutoff
- Cull Mode / Depth Write / Receive Shadow
- Texture Add / Remove
- Texture Semantic / Color Space
- Source Path / Asset Path
- UV Channel / Scale / Offset / Rotation
- Strength / Embedded状態

旧単一Material UIはScene後方互換のため残す。

`ModelRendererSubMeshInspector`を追加した。

`ModelRendererComponent` Inspector:

- Imported SubMesh Name / Stable ID / Geometry Index / Node Path
- Model Default Material表示
- Visible
- Cast Shadow
- Model Default / Custom Material Source
- Custom Material ID
- SubMesh State Reset

Material適用範囲は`ModelRendererComponent`、Material定義は`MaterialComponent`という所有分離をUIでも維持する。

現段階ではInspector APIが編集中EntityをComponentへ渡さないため、Custom Materialは名前ComboではなくStable ID入力とする。Runtime Component HeaderをEditor Serviceへ依存させる回避実装は採用しない。

---

## 2. 検証契約

`ModelMaterialFoundationSmokeTest`:

- Texture Path正規化
- Embedded Texture Index
- SemanticごとのColor Space
- Stable Local IDとCollision Diagnostic
- Imported Material解決
- Legacy Material Override
- Custom Material解決
- Broken Custom IDのImported Default Fallback
- MaterialComponent欠落時Fallback
- Imported Material欠落時Engine Default Fallback
- Legacy D3D11 Material / Texture Bridge

`ModelMaterialPassRoutingSmokeTest`:

- OpaqueのDeferred維持
- BlendのSorted Forward昇格
- MaskedのDeferred維持
- Legacy Alpha Fallback
- 明示Transparent / Overlay Layer維持

`RenderPacketModelSubMeshExpansionSmokeTest`:

- Stable ModelSubMeshIDとGeometry Indexの分離
- SubMesh配列順とGeometry配列順が異なる場合のPacket展開
- PacketへのMaterial Snapshot
- Material Shader Keyの更新
- Visible / Cast Shadow契約

`ModelSubMeshStateSynchronizationSmokeTest`:

- Invalid / Duplicate / Stale ID除去
- Reimport順への再配置
- Existing Override保持
- 新規SubMeshのDefault State生成
- 壊れたCustom Assignmentの修復

`CustomMaterialCollectionSmokeTest`:

- ID割当
- Add / Remove
- Empty Name補完
- Invalid / Duplicate ID Sanitize
- 最大ID到達時の空きID探索

`ModelMaterialInspectorHeaderSmokeTest`:

- Inspector Headerの直接include
- ImGui宣言依存
- Custom Material Collection API
- SubMesh State生成API

`ModelMaterialSerializationSourceContractSmokeTest`:

- Material / SubMesh YAML接続
- Runtime Reimport同期
- 一時Load失敗時の状態保持
- 別Model Path変更時の状態Clear
- Component Collection API接続
- Custom Material Inspector接続
- SubMesh Assignment Inspector接続

回帰確認:

- Header-only化前はMesh-only `StaticBatchFrameBufferIntegrationSmokeTest`へAssimp Link依存が伝播し失敗した
- `aiMaterialProperty`直接読取へ変更後、同Smokeは成功へ復帰した
- Alpha Routing導入後、専用Material契約とWindows Debug / Release Buildは成功済み

---

## 3. 現在の検証状態

2026-08-03時点:

- Alpha RoutingまでのModel Material Foundation Smoke: 成功
- Alpha RoutingまでのWindows Debug / Release x64 Build: 成功
- YAML Persistence / Reimport Synchronization / Custom Material Collection / Inspector追加後:
  - GitHub hosted runner待ち
  - 失敗ログなし
  - 成功は未確定

---

## 4. 意図的に未実装の範囲

- Texture Asset ServiceへのImport委譲
- Embedded Texture BinaryのAsset化
- RHI Texture / Texture View Runtime
- 全Texture SlotのStatic Batch Shader Binding
- Static Batch Snapshot欠落用Assimp Fallbackの撤去
- MaterialAsset YAMLと差分Override
- Entity-aware Inspector ContextとCustom Material名前Combo
- Inspector Combo / Vector操作の完全Undo Command化
- Reimport Diagnostic UI
- Tangent `float4` / Bitangent Sign
- UV1以降のVertex Layout

---

## 5. 次工程

```text
1. Persistence / Reimport / Collection / Inspectorの専用CIとWindows Buildを緑化
2. Entity-aware Inspector Contextを設計
3. ModelRenderer SubMesh -> Custom Material名前Combo
4. Reimport / Resolver Diagnostic UI
5. Texture Runtime Handle / RHI Binding境界
6. Static Batchの全Texture Slot対応
7. Snapshot欠落用aiMaterial Fallback撤去
8. MaterialAsset YAML + Component差分Override
```

Assimpは最終的にImport Adapter内部だけで参照し、Render Extraction以降へ`aiMaterial*`を渡さない。
