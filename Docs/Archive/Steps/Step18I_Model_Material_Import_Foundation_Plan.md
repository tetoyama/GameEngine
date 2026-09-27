# Step 18-I: Model Material Import Foundation Plan

Status: **Planned — 2026-08-02**

対象:

- `refactor/ecs-scheduler-foundation`
- PR #45

関連文書:

- `Docs/ECS_Scheduler_Migration_Plan.md`
- `Docs/Step18A_RenderWorld_Runtime_Ownership_Progress.md`
- `Docs/RenderPipeline_Graph_Architecture.md`
- `Docs/RenderPipeline_Graph_Integration_Plan.md`
- `Docs/RenderPipeline_Graph_Execution_Order_Amendment.md`

---

# 1. 目的

現在の`ModelRendererComponent`と`MaterialComponent`を拡張し、Blender・Maya等から出力された一般的なFBXについて、SubMesh、複数Material、複数TextureをEngine側で再設定せず扱える3D Model描画基盤を構築する。

最低限の対象は次とする。

- 1 Model内の複数SubMesh
- 1 Model内の複数Imported Material
- 複数SubMeshによるImported Material共有
- Base Color / Diffuse Texture
- Normal Map
- Bump / Height Map
- Metallic Map
- Roughness Map
- Ambient Occlusion Map
- Emissive Map
- Opacity / Alpha Clip
- Embedded Textureと外部Texture
- Tangent Space
- Material単位のAlpha Mode / Double Sided等の描画設定
- Entity単位で複数Custom Materialを定義し、SubMesh単位で割り当てる機能

DCC固有Shader Node Graphを完全再現することではなく、FBXへ書き出された標準的なMaterial情報をEngine共通Material表現へ正規化し、実用上大きく破綻しない表示を目標とする。

---

# 2. 基本方針

Componentを複数付与できる設計へ変更しない。

```text
Entity
├─ ModelRendererComponent × 1
└─ MaterialComponent      × 0 or 1
```

複数SubMesh・複数MaterialはComponent内部の配列で扱う。

ただし、Materialの定義、Model付属Material、SubMeshへの割当は同じ場所へ混在させない。

```text
ModelData
├─ Geometry[]
├─ SubMeshDefinition[]
└─ ImportedMaterialDefinition[]

ModelRendererComponent
├─ Model参照
└─ SubMeshRenderState[]
    └─ SubMeshごとのMaterial Assignment

MaterialComponent
└─ CustomMaterial[]
    └─ ユーザー定義Materialの内容のみ
```

---

# 3. 確定した所有関係

## 3.1 ModelData

`ModelData`はFBX等からImportされた共有・読取専用データを所有する。

所有対象:

- Geometry
- Vertex / Index Source
- SubMesh定義
- SubMeshとImported Materialの既定対応
- Imported Material定義
- Imported Texture参照
- Skeleton / Animation等のModel共有情報

Model付属Materialは`ModelData`のみが所有する。

Entity単位の`ImportedMaterialInstance`は作成しない。

モデル付属MaterialをEntityごとに変更したい場合は、`MaterialComponent`へCustom Materialを作成する。

## 3.2 ModelRendererComponent

`ModelRendererComponent`は次を所有する。

- 使用するModel参照
- SubMeshごとの表示状態
- SubMeshごとのCast Shadow等のRenderer設定
- SubMeshごとのMaterial割当

MaterialのパラメータやTextureそのものは所有しない。

MaterialをどのSubMeshへ適用するかという範囲・対応関係は`ModelRendererComponent`の責務とする。

## 3.3 MaterialComponent

`MaterialComponent`はユーザー定義Custom Materialを複数保持する。

所有対象:

- Custom MaterialのローカルID
- Custom Material名
- Shader / Material Parameter
- Texture Binding
- Material Render State

所有しないもの:

- Model参照
- SubMesh参照
- 適用対象・適用範囲
- Imported MaterialのEntity別コピー

Custom MaterialがどのSubMeshへ使われるかは知らない。

## 3.4 将来のMaterialAsset

将来的にはCustom Material本体をYAML Assetとして分離する。

```text
MaterialAsset YAML
    Shader
    Parameters
    Texture Bindings
    Render State
          ↑
MaterialComponent
    Asset File Path
    Instance Difference / Override
          ↑
ModelRendererComponent
    SubMesh → Custom Material Local ID
```

最終的に`MaterialComponent`はMaterialAssetのファイルパスと差分だけを保持する。

---

# 4. ModelData構造

概念構造は次とする。

```cpp
using ModelSubMeshID = uint32_t;
using ImportedMaterialID = uint32_t;

struct ModelSubMeshDefinition
{
    ModelSubMeshID id = 0;
    std::string name;

    uint32_t geometryIndex = 0;
    ImportedMaterialID defaultMaterialID = 0;

    LocalBounds localBounds;
};

struct ImportedMaterialDefinition
{
    ImportedMaterialID id = 0;
    std::string name;

    MaterialDescriptor descriptor;
};

struct ModelData
{
    std::vector<ModelMeshGeometryCpuData> geometries;
    std::vector<ModelSubMeshDefinition> subMeshes;
    std::vector<ImportedMaterialDefinition> importedMaterials;
};
```

配列IndexはRuntime走査に使用できるが、Scene保存・Reimport追従に使用する識別子はAssetローカルIDとする。

UUIDは使用しない。

---

# 5. ModelRendererComponent構造

Material割当はSubMesh単位で明示的に保持する。

```cpp
using CustomMaterialID = uint32_t;

constexpr CustomMaterialID InvalidCustomMaterialID = 0;

enum class SubMeshMaterialSource : uint8_t
{
    ModelDefault,
    CustomMaterial
};

struct SubMeshMaterialAssignment
{
    SubMeshMaterialSource source =
        SubMeshMaterialSource::ModelDefault;

    CustomMaterialID customMaterialID =
        InvalidCustomMaterialID;
};

struct ModelSubMeshRenderState
{
    ModelSubMeshID subMeshID = 0;

    bool visible = true;
    bool castShadow = true;

    SubMeshMaterialAssignment material;
};

class ModelRendererComponent
{
public:
    ModelAssetReference model;
    std::vector<ModelSubMeshRenderState> subMeshes;
};
```

`ModelDefault`の場合は、`ModelData::ModelSubMeshDefinition::defaultMaterialID`からImported Materialを解決する。

`CustomMaterial`の場合は、同一Entityの`MaterialComponent`から`CustomMaterialID`を解決する。

MaterialComponentが存在しない、またはID参照が壊れている場合はModel DefaultへFallbackし、EditorとRender診断へBroken Referenceを記録する。

---

# 6. MaterialComponent構造

初期段階ではCustom MaterialをInline定義できる構造とする。

```cpp
struct CustomMaterialEntry
{
    CustomMaterialID id = 0;
    std::string name;

    MaterialDescriptor inlineMaterial;
};

class MaterialComponent
{
public:
    std::vector<CustomMaterialEntry> materials;
};
```

同一Custom Materialを複数SubMeshから参照できる。

```text
SubMesh 0 ─┐
SubMesh 1 ─┼→ Custom Material ID 4
SubMesh 5 ─┘
```

MaterialComponent内の並べ替えによって参照が変わらないよう、ModelRendererは配列IndexではなくローカルIDを保存する。

## 6.1 MaterialAsset導入後

```cpp
struct CustomMaterialEntry
{
    CustomMaterialID id = 0;
    std::string name;

    MaterialAssetPath assetPath;
    MaterialOverrideSet overrides;
};
```

移行期間はInline MaterialとAsset参照を共存させてもよいが、最終形はAsset Path + Differenceとする。

---

# 7. 共通Material表現

Imported MaterialとCustom Materialは、描画時に同じ`MaterialDescriptor`へ解決できることを必須とする。

概念上の内容:

```cpp
struct MaterialDescriptor
{
    ShaderReference shader;

    MaterialParameters parameters;
    MaterialTextureSet textures;
    MaterialRenderState renderState;
};
```

## 7.1 Material Parameter候補

- Base Color
- Metallic
- Roughness
- Ambient Occlusion
- Emissive Color
- Emissive Intensity
- Opacity
- Normal Scale
- Bump / Height Scale
- Alpha Cutoff

## 7.2 Texture Semantic候補

- Base Color / Diffuse
- Normal
- Bump / Height
- Metallic
- Roughness
- Ambient Occlusion
- Emissive
- Opacity

各Texture Bindingは最低限次を表現できるようにする。

```cpp
struct MaterialTextureBinding
{
    TextureAssetReference texture;

    uint8_t uvChannel = 0;
    float2 uvScale = {1.0f, 1.0f};
    float2 uvOffset = {0.0f, 0.0f};
    float uvRotation = 0.0f;

    float strength = 1.0f;
};
```

Color TextureとData TextureでColor Space契約を分離する。

- Base Color / Emissive: 原則sRGB
- Normal / Height / Metallic / Roughness / AO: Linear Data

## 7.3 Material Render State候補

- Opaque / Masked / Blend
- Alpha Cutoff
- Cull Mode / Double Sided
- Depth Write
- Receive Shadow

正確なフィールド確定はShader / RenderPipeline契約と同時に行う。

---

# 8. FBX Import Pipeline

Assimpの`aiScene`、`aiMesh`、`aiMaterial`はImport Sourceとして使用するが、通常描画中に直接参照しない構造へ移行する。

```text
FBX
  ↓ Assimp Import
aiScene / aiMesh / aiMaterial
  ↓ Import Normalization
ModelData
  Geometry[]
  SubMeshDefinition[]
  ImportedMaterialDefinition[]
  Texture Asset Reference[]
  ↓ Runtime Resource Build
RenderWorld / RenderPacket / RHI
```

Import時に次を行う。

- MeshごとのSubMesh定義生成
- `mMaterialIndex`から既定Imported Material IDを設定
- aiMaterialをEngine共通`MaterialDescriptor`へ変換
- Embedded Textureと外部TextureをTexture Assetへ解決
- Texture Pathの区切り・相対Path・大文字小文字差を正規化
- Normal / Tangent / Bitangentの検証または生成
- Material SemanticごとのColor Spaceを設定
- Import Warning / Unsupported Propertyを記録

Draw中に`aiMaterial::GetTexture`やD3D11 SRV Mapを探索する処理は廃止対象とする。

---

# 9. Material解決

各SubMeshの最終MaterialはRender Extraction時に解決する。

```text
ModelRenderer SubMesh Assignment
    source == CustomMaterial
        ↓
MaterialComponentからID解決
        ↓ 成功
Custom Material

        ↓ 失敗
Model DefaultへFallback

    source == ModelDefault
        ↓
ModelData SubMesh Default Material
```

描画コードへ渡す時点では、次を直接参照しない。

- `aiMaterial*`
- `MaterialComponent*`
- `TextureComponent*`
- D3D11 Native SRV

RenderPacketには解決済みMaterial Runtime Handleまたは描画に必要なSnapshotを保持する。

```cpp
struct ModelSectionRenderPacket
{
    ModelGeometryHandle geometry;
    MaterialRuntimeHandle material;

    ModelSubMeshID subMeshID;
    bool castShadow;
};
```

---

# 10. Editor操作

## 10.1 ModelRenderer Inspector

SubMesh一覧を表示し、SubMesh単位で次を編集する。

- Visible
- Cast Shadow
- Material Source
- Custom Material選択

```text
SubMeshes
  Body
    Material: Model Default (Body)
  Glass
    Material: Custom / CustomGlass
  Tire_FL
    Material: Model Default (Tire)
```

## 10.2 MaterialComponent Inspector

Custom Material一覧を管理する。

- Add
- Duplicate
- Delete
- Rename
- Shader選択
- Parameter編集
- Texture Binding編集
- 将来のMaterial Asset作成・参照

MaterialComponent側ではSubMesh適用対象を編集しない。

## 10.3 Imported MaterialからCustom Materialを作成

ModelRenderer Inspectorから次の操作を提供する。

```text
Imported Material: Body
    [Custom Materialとして複製]
```

処理:

1. `MaterialComponent`がなければ追加
2. Imported Materialの`MaterialDescriptor`をCustom Materialへ複製
3. 新しいCustom Material Local IDを発行
4. 選択SubMeshのMaterial AssignmentをそのIDへ変更

複数SubMeshへの一括適用はEditor操作として行い、結果はModelRenderer内の明示的なSubMesh Assignmentへ展開する。

MaterialComponentへ`All SubMeshes`等の暗黙的な適用範囲規則は持たせない。

---

# 11. Reimport契約

FBX再出力でMeshやMaterialの配列順が変化しても、可能な限りScene上のCustom Material Assignmentを維持する必要がある。

そのため、SubMeshとImported MaterialにはAssetローカルIDを付与する。

```text
Model Asset
  SubMesh ID 4: Body
  SubMesh ID 7: Glass

Reimport
  Body  → ID 4を維持
  Glass → ID 7を維持
  Decal → 新規ID 8
```

ID対応表はModel Import Metadataへ保存する。

完全一致できなかった場合は、Name、Node Path、Material Name、Geometry Fingerprint等を用いた候補提示を行う。

自動対応規則の詳細は未確定事項とする。

---

# 12. Legacy互換

現在の単一`TextureComponent`によるModel Texture Override、`ModelData::m_Texture`、Draw中のAssimp Material参照は段階的に撤去する。

移行期間は旧Sceneを表示できるAdapterを維持するが、新規Model描画の正規経路にはしない。

正規経路:

```text
ModelData Imported Material
    または
MaterialComponent Custom Material
        ↓
ModelRenderer SubMesh Assignment
        ↓
Resolved Material Runtime
        ↓
RenderPacket
        ↓
RHI
```

---

# 13. 実装工程

```text
MMI-0  Current Contract Inventory
MMI-1  MaterialDescriptor / Texture Semantic Contract
MMI-2  ModelData SubMesh / Imported Material Normalization
MMI-3  ModelRenderer SubMesh Material Assignment
MMI-4  MaterialComponent Multiple Custom Materials
MMI-5  Shader / RHI Multi-Texture Binding
MMI-6  RenderPacket Material Snapshot / Runtime Handle
MMI-7  Inspector / YAML / Broken Reference Diagnostics
MMI-8  Reimport Stable Local ID / Migration
MMI-9  MaterialAsset YAML / Asset Path + Difference
MMI-10 Legacy Model Texture Path Removal
```

## MMI-0

- 現在のModel Loader、RenderableModel、MaterialComponent、TextureComponentを棚卸し
- Draw中Assimp参照とNative D3D11 Texture所有箇所を列挙
- Blender / Mayaから基準FBXを用意

## MMI-1

- Material Parameter
- Texture Semantic
- Color Space
- Render State
- UV Binding
- Missing Texture Fallback

を確定する。

## MMI-2

- Assimp MaterialをImport時に正規化
- ModelDataへSubMesh / Imported Material配列を追加
- Embedded / External Textureを共通参照へ変換

## MMI-3

- ModelRendererComponentへSubMeshRenderState配列を追加
- Model Default / Custom Materialの割当を追加
- Model変更時の同期処理を追加

## MMI-4

- MaterialComponentを複数Custom Material対応へ変更
- Local ID発行
- Inline Material編集

## MMI-5

- Base Color以外のTexture Slotを正規Bindingへ移行
- Normal Mapping
- Bump / Height
- Metallic / Roughness / AO / Emissive / Opacity
- Shadow Alpha Clip整合

## MMI-6

- Extraction時にMaterialを解決
- Component PointerとaiMaterial参照をDrawから除去
- Static / Skinned / Forward / Deferred / Shadowで同じMaterial契約を使用

## MMI-7

- SubMesh Material Inspector
- Custom Material Inspector
- YAML保存
- Missing Texture / Broken Material Reference表示

## MMI-8

- Reimport Metadata
- Stable Local ID
- Assignment維持
- Ambiguous / Removed SubMesh診断

## MMI-9

- YAML Material Asset
- Shared Material Runtime
- MaterialComponentをAsset Path + Differenceへ移行
- Inline MaterialからAssetへの昇格

## MMI-10

- `ModelData::m_Texture`撤去
- Draw中`aiMaterial`参照撤去
- Model向け単一TextureComponent OverrideをLegacy Adapterへ限定
- Native D3D11 Resource所有をRHI Runtimeへ統合

---

# 14. 完了条件

- Blenderから出力した複数Material FBXが手作業のTexture再設定なしで表示される
- Mayaから出力した複数Material FBXが手作業のTexture再設定なしで表示される
- 複数SubMeshが同じImported Materialを共有できる
- Base Color / Normal / Metallic / Roughness / AO / Emissive / Opacityが描画へ反映される
- Bump / Heightの採用方式が決定し描画へ反映される
- Embedded Textureと外部Textureの両方を扱える
- Custom Materialを1 Entity内に複数作成できる
- ModelRendererからSubMesh単位でCustom Materialを割り当てられる
- Custom Material削除時にModel Defaultへ安全にFallbackする
- Static、Skinned、Deferred、Forward、ShadowでMaterial解決が一致する
- Runtime Draw中に`aiMaterial`を参照しない
- RenderPacketがComponent PointerではなくMaterial Runtime HandleまたはSnapshotを保持する
- FBX Reimport後も可能な範囲でSubMesh Assignmentが維持される

---

# 15. 未確定事項

以下は本計画追加時点では確定しない。

## 15.1 Bump / Height

- Shader内でHeight勾配からNormalを生成する
- Import時にNormal Mapへ変換する
- Parallax Mappingを別機能として扱う

のどれを標準経路にするか。

## 15.2 PBR Workflow

- Metallic / Roughnessへ統一するか
- Specular / GlossinessもRuntimeで維持するか
- Maya由来MaterialをImport時に変換するか

## 15.3 Texture Packing

- Metallic / Roughness / AOを個別Textureとして保持するか
- Channel Packed Textureを標準化するか
- Import時またはBuild時に再Packingするか

## 15.4 複数UV

- 対応する最大UV Channel数
- Lightmap UVとの分離
- Material TextureごとのUV Channel選択

## 15.5 Sampler

- Wrap / Clamp / Mirror
- Filter
- Anisotropy

をTexture Asset、Material Binding、Shaderのどこが所有するか。

## 15.6 Imported Materialの差し替え範囲

初期契約はModel DefaultまたはCustom Materialとする。

ModelData内の別Imported Materialを任意SubMeshへ再割当する機能を標準UIへ含めるかは未確定。

## 15.7 高度なDCC表現

- Clear Coat
- Sheen
- Transmission
- Anisotropy
- UDIM
- Procedural Texture
- DCC Shader Node Graph
- Tessellation Displacement

は初期完了条件へ含めない。必要に応じてMaterial Shader / Material Graphの別工程で扱う。
