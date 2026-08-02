# Step 18-I: Model Asset / Material Import Pipeline Plan

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
- `Docs/Step18H_2D_Rendering_Foundation_Plan.md`

---

# 1. 目的

Blender、MayaなどのDCC Toolから出力されたFBXを、GeometryとAnimationだけでなく、標準的なMaterial、Texture、UV、Tangent、Transparencyを含めて予測可能にImport・描画できる基盤を構築する。

単に`aiTextureType_DIFFUSE`以外を追加するのではなく、次を分離する。

```text
Source FBX / glTF / OBJ
        ↓ Import Adapter
Imported Model / Material Intermediate Representation
        ↓ Normalize / Validate / Convert
Engine Model Asset / Material Asset / Texture Asset
        ↓ Runtime Snapshot
RenderWorld / Material Binding Set
        ↓
RenderOperation / RenderGraph / RHI
```

目標:

- 1 Model内の複数Material / SubMesh Material Assignment
- Base Color / Normal / Bump / Height
- Roughness / Metallic / AO / Emissive / Opacity
- Specular / Glossiness系Materialの変換または保持
- 外部TextureとFBX Embedded Media
- 複数UV SetとTextureごとのUV Set選択
- Texture Transform / Tiling / Offset
- Wrap / Clamp / MirrorとFilter / Anisotropy
- Alpha Opaque / Mask / Blend
- Double-Sided / Cull Mode
- Normal Map Strength / Bump Scale
- sRGB / Linear Color Space
- Channel Packing / Swizzle
- Custom Texture Slot
- Blender / Maya向けImport Profile
- Reimport時のUser Override保持
- Import診断とPreview

「問題なく扱える」とは、DCC固有Shader Node Graphをそのまま実行することではない。
FBXへ表現可能な標準Material ChannelとFile TextureをEngineのMaterial Contractへ正規化し、表現できないNode、Procedural Texture、Layered Shaderは明示診断してBakeまたは手動変換へ誘導することを意味する。

---

# 2. 現状の問題

## 2.1 ModelDataが責務を混在させている

現在の`ModelData`は次を同時に保持する。

- Assimp `aiScene`
- CPU Geometry
- D3D11 Vertex / Index Buffer
- D3D11 Shader Resource View
- Animation / Bone
- Skinning Buffer

Materialは独立Assetへ変換されず、Textureは`unordered_map<string, ID3D11ShaderResourceView*>`へ格納される。

問題:

- Backend非依存Assetとして扱えない
- Texture Semantic、Color Space、UV Set、Samplerが失われる
- 同じTexture Pathでも用途ごとのsRGB / Linearを区別できない
- Device Lost / RHI Backend変更時の再生成境界が曖昧
- Reimport時にMaterial Overrideを安定保持できない

## 2.2 外部Texture読込がDiffuse中心

外部Texture Pathの探索とLoadは主に`aiTextureType_DIFFUSE`を対象としている。

Normal Textureを描画側で参照する経路は一部存在するが、外部Normal TextureがModel Import時に同じTexture Tableへ登録される保証がない。

Roughness / Metallic用SlotとFlagは定義されているが、Import、Binding、Shader Samplingが一貫した契約になっていない。

## 2.3 MaterialをDraw中にaiMaterialから読む

`RenderableModel`は各Drawで次を行う。

- `aiMesh::mMaterialIndex`から`aiMaterial`を取得
- Diffuse Colorを取得
- Diffuse / Normal Texture名を検索
- D3D11 SRVを直接Bind

問題:

- Draw PathがAssimpへ依存する
- Material解釈がFrameごとに発生する
- Static Batchと通常描画でMaterial解釈を共有しにくい
- RenderPacketが安定したMaterial Snapshotを持てない
- RHI / RenderOperationへ移行できない

## 2.4 Vertex ContractがDCC Material用途に不足する

現在の共通Vertexは主に次だけを持つ。

- Position
- Normal
- Tangent `float3`
- Vertex Color
- UV0

不足:

- Tangent Handedness
- Mirrored UVで必要なBitangent Sign
- UV1以降
- Color AttributeのSemantic / Color Space
- ImportされたCustom Normal / TangentのSource情報

## 2.5 Blender判定がBoolになっている

`isBlender`による個別座標変換は、DCC、Exporter設定、Axis、Unit Scale、Assimp変換を混在させる。

Source Application名だけで座標を決めず、Import SettingsとFBX Metadataから正規化する必要がある。

---

# 3. 基本方針

## 3.1 Canonical Materialを定義する

Engine標準MaterialはPBR Metallic-Roughnessを基本とする。

```cpp
enum class MaterialShadingModel
{
    PbrMetallicRoughness,
    PbrSpecularGlossiness,
    Unlit,
    Custom
};
```

FBXのLambert / Phong / BlinnやMaya Standard Surface、Blender Principled BSDF由来Channelは、Import時にCanonical Materialへ変換する。

完全変換できない場合:

- Source PropertyをImport Recordへ保持する
- Conversion Warningを出す
- 元Material名とSource Shader Modelを保持する
- UserがMaterial Assetを差し替えられるようにする

## 3.2 Assimp SceneをRuntime Material Sourceにしない

AssimpはImport Adapterとしてのみ使用する。

```text
aiScene / aiMaterial
    ↓ Import時だけ参照
ImportedModelDocument
    ↓ Normalize
ModelAsset + MaterialAsset[] + TextureAsset[]
```

Runtime Draw Pathは`aiMaterial`、`aiTexture`、Assimp Texture Typeへ依存しない。

## 3.3 MaterialとTextureをModelから分離する

```cpp
struct ModelSubMeshAsset
{
    GeometryRange geometry;
    MaterialAssetHandle material;
    std::string sourceMeshName;
    std::string sourceMaterialName;
};

struct ModelAsset
{
    std::vector<ModelSubMeshAsset> subMeshes;
    SkeletonAssetHandle skeleton;
    ModelImportMetadata importMetadata;
};
```

Material AssetはModel Instance間で共有可能とする。

Scene Entity側は必要に応じてMaterial Override Tableだけを持つ。

```cpp
struct ModelMaterialOverrides
{
    std::vector<MaterialAssetHandle> perSlot;
};
```

## 3.4 Texture BindingをDescriptor化する

```cpp
enum class MaterialTextureSemantic
{
    BaseColor,
    Normal,
    Bump,
    Height,
    Roughness,
    Metallic,
    AmbientOcclusion,
    Emissive,
    Opacity,
    SpecularColor,
    SpecularFactor,
    Glossiness,
    ClearCoat,
    ClearCoatRoughness,
    Transmission,
    Custom
};

struct MaterialTextureBinding
{
    MaterialTextureSemantic semantic;
    TextureAssetHandle texture;

    uint32_t uvSet = 0;
    float2 uvScale = {1.0f, 1.0f};
    float2 uvOffset = {0.0f, 0.0f};
    float uvRotation = 0.0f;

    TextureChannelSwizzle swizzle;
    TextureColorSpace colorSpace;
    SamplerAssetHandle sampler;

    float strength = 1.0f;
};
```

固定PBR Slotに加え、Custom Shader用に複数`Custom` Bindingを許可する。

## 3.5 Color Spaceを用途から決定する

既定:

```text
BaseColor / Emissive / SpecularColor
    sRGBとしてImportし、Shader Read時にLinear化

Normal / Bump / Height / Roughness / Metallic / AO / Opacity
    Linear Data Texture
```

File ExtensionやTexture ResourceだけでColor Spaceを決めない。
同じSource ImageがColorとDataの両用途で使用される場合は、ViewまたはImport Variantを分離する。

## 3.6 Normal / Bump / Heightを区別する

```text
Normal Map
    RGBでTangent Space Normalを保持

Bump Map
    Scalar Heightの局所勾配から法線を生成

Height Map
    Surface displacement量。Parallax / Tessellation / Offline displacement候補
```

初期標準経路:

- Normal Map: Runtime Sampling
- Bump Map: Import時Normal変換、または軽量Runtime Bump Mode
- Height Map: Assetとして保持
- Parallax / POM: 後続Optional Material Feature

BumpをNormal Textureとして無条件解釈しない。

---

# 4. Texture Path / Embedded Media契約

Texture Resolverは次の順で解決する。

```text
1. FBX Embedded Texture ID / aiTexture reference
2. Source File基準のRelative Path
3. Import SettingsのTexture Search Paths
4. Asset Root相対Path
5. Filename-only fallback（明示許可時のみ）
```

要件:

- `/`と`\`の正規化
- Windows Drive / UNC Pathの扱い
- Case mismatch診断
- URL Escape / UTF-8 Path
- `..`解決後のAsset Root逸脱検出
- 同一Contentの重複Import抑制
- Missing Texture Placeholder
- Source PathとResolved Pathを両方保存
- Embedded TextureのCompressed / Raw形式を区別

Texture DecodeはModel Loader内でD3D11 SRVを直接作らず、Texture Import Serviceへ委譲する。

---

# 5. Geometry / Tangent / UV Contract

## 5.1 Tangent Handedness

Tangentは`float4`へ拡張し、`w`へBitangent Signを保持する。

```cpp
struct VertexPNTUV
{
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv0;
};
```

Shaderでは次でBitangentを復元する。

```text
bitangent = cross(normal, tangent.xyz) * tangent.w
```

Mirrored UVとNegative Scaleを正しく扱う。

## 5.2 Tangent生成

優先順位:

```text
1. Source Tangentが有効でImport設定がPreserve
2. Engine側MikkTSpace生成
3. Normal Mapを無効化しDiagnostic
```

座標系変換後にTangent / Handednessを検証する。
Assimpの既定Tangent生成結果へ無条件依存しない。

## 5.3 複数UV Set

最低限:

- UV0: Base Material
- UV1: Secondary / Lightmap / Detail

将来拡張可能なAttribute Layoutを使用する。
Texture BindingごとにUV Set Indexを保持し、存在しないUV Set参照はImport ErrorまたはFallback Diagnosticとする。

## 5.4 Normal Map Convention

```cpp
enum class NormalMapConvention
{
    Auto,
    DirectX,
    OpenGL
};
```

Green Channel反転はTextureを破壊的変換せず、Import VariantまたはBinding Swizzleとして扱えるようにする。
DCC Profileは既定値を提示するが、Asset単位でOverride可能とする。

---

# 6. Standard Material Contract

```cpp
struct PbrMaterialParameters
{
    float4 baseColor = {1, 1, 1, 1};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ambientOcclusion = 1.0f;

    float3 emissiveColor = {0, 0, 0};
    float emissiveIntensity = 0.0f;

    float normalScale = 1.0f;
    float bumpScale = 1.0f;
    float heightScale = 0.0f;

    float alphaCutoff = 0.5f;
};
```

```cpp
enum class MaterialAlphaMode
{
    Opaque,
    Mask,
    Blend
};
```

```cpp
struct MaterialAsset
{
    MaterialShadingModel shadingModel;
    PbrMaterialParameters parameters;
    std::vector<MaterialTextureBinding> textures;

    MaterialAlphaMode alphaMode;
    bool doubleSided = false;

    std::string name;
    SourceMaterialMetadata source;
    uint64_t revision = 1;
};
```

## 6.1 必須標準Texture

第一段階:

- Base Color
- Normal
- Roughness
- Metallic
- AO
- Emissive
- Opacity
- Bump / Height

第二段階:

- Specular Color / Factor
- Glossiness
- Clear Coat
- Clear Coat Roughness
- Transmission
- Detail Normal / Detail Mask

## 6.2 Channel Packing

次を表現できるようにする。

```text
Separate
    Roughness = R texture
    Metallic  = R texture
    AO        = R texture

Packed ORM
    R = AO
    G = Roughness
    B = Metallic

Custom
    任意Swizzle
```

Texture BindingにChannel Swizzleを持たせ、Shader Variantを無制限に増やさない。

---

# 7. DCC Compatibility Profile

## 7.1 Blender FBX Profile

対象となる標準的なBake済み / File Texture Workflow:

- Principled BSDF由来Base Color
- Metallic
- Roughness
- Normal Map
- Alpha
- Emission
- UV Map
- Custom Split Normal
- Armature / Skinningとの共存

Node Group、Procedural Texture、複雑なMix ShaderはFBXの固定Material Channelへ完全変換できないため、Import Diagnosticで検出可能な範囲を通知し、Image TextureへのBakeを推奨する。

## 7.2 Maya FBX Profile

対象:

- Standard Surfaceの標準File Texture Channel
- Base Color
- Metalness
- Specular Roughness
- Normal Camera / Bump
- Emission
- Opacity
- Legacy Lambert / Phong / Blinn
- Multiple Material Assignment
- Embedded Media / Relative File Texture

Maya Procedural TextureはFile TextureへBakeされていることを前提とする。

## 7.3 ProfileはHard-coded Application Branchにしない

```cpp
enum class ModelImportProfile
{
    Auto,
    BlenderFBX,
    MayaFBX,
    GenericFBX,
    Gltf2
};
```

Profileは次の既定値を提供するだけとする。

- Axis / Unit候補
- Normal Convention候補
- Material Property Mapping
- Texture Search Rule
- Warning Rule

最終的な変換はFBX Metadataと明示Import Settingsで確定する。
`isBlender` Boolは廃止する。

---

# 8. Import Settings / Sidecar

```cpp
struct ModelImportSettings
{
    ModelImportProfile profile = ModelImportProfile::Auto;

    CoordinateSystem targetCoordinates;
    UnitScalePolicy unitScale;

    bool preserveSourceNormals = true;
    bool preserveSourceTangents = true;
    bool generateMissingNormals = true;
    bool generateMikkTangents = true;

    NormalMapConvention normalConvention =
        NormalMapConvention::Auto;

    bool importMaterials = true;
    bool importTextures = true;
    bool importEmbeddedTextures = true;

    MaterialConversionPolicy materialConversion;
    MissingTexturePolicy missingTexturePolicy;

    std::vector<std::string> textureSearchPaths;
};
```

SettingsはSource Assetと分離したSidecarへ保存する。

```text
robot.fbx
robot.fbx.meta.yaml
```

Reimport時に同じ設定を再利用する。

---

# 9. Reimport / Override契約

Source FBXのMaterial Slotは安定したSource Keyで識別する。

```cpp
struct SourceMaterialKey
{
    std::string sourceMaterialName;
    uint32_t sourceMaterialIndex;
    uint64_t sourcePropertyHash;
};
```

Reimport時:

- Geometry / Material Source変更をRevisionへ反映
- Slot名が維持される場合はUser Material Overrideを保持
- Source Materialを更新してもInstance Overrideを破壊しない
- 削除SlotはOrphanとしてDiagnostic
- 新規SlotはImported Materialを既定割当
- Texture Dependency変更でMaterial Revisionを更新

Model Geometry RevisionとMaterial Revisionを分離する。
Texture変更だけでGeometry Runtimeを再生成しない。

---

# 10. Runtime / RenderWorld契約

RenderPacketはAssimp Pointerではなく、Frame-owned Asset Snapshot / Handleを保持する。

```cpp
struct ModelRenderPacketSnapshot
{
    ModelAssetHandle model;
    MaterialBindingTableHandle materials;
    ModelGeometryRuntimeHandle geometry;
    uint64_t modelRevision;
    uint64_t materialRevision;
};
```

Material Binding Set:

- Material Constant
- Texture View Handle配列
- Sampler Handle配列
- Feature Mask
- Alpha / Cull / Render Queue

を持つ。

D3D11 `PSSetShaderResources`を`RenderableModel`内で直接行う構造は撤去する。
Raster OperationまたはRHI Material BinderへLoweringする。

Static Batch Keyには次を含める。

- Geometry identity / revision
- Material Asset identity / revision
- Texture Binding Set identity
- Pipeline Variant
- Alpha Mode / Cull Mode

Texture差異を無視して異なるMaterialを同じBatchへ混ぜない。

---

# 11. Shader / GBuffer契約

## 11.1 Feature Mask

Material Featureを明示する。

```text
HasBaseColorMap
HasNormalMap
HasBumpMap
HasRoughnessMap
HasMetallicMap
HasAOMap
HasEmissiveMap
HasOpacityMap
HasPackedORM
DoubleSided
AlphaMask
AlphaBlend
```

現行`MATERIAL_FLAG_*`を拡張またはMaterial Feature Maskへ置換する。

## 11.2 Missing Textureの残留Bind防止

各Draw / Batchで使用しないSlotには標準Fallback TextureをBindする。

```text
BaseColor    = White
Normal       = Flat Normal
Roughness    = WhiteまたはParameter値経路
Metallic     = BlackまたはParameter値経路
AO           = White
Emissive     = Black
Opacity      = White
Height       = Mid Gray
```

前DrawのSRVが残留してMaterialへ漏れることを禁止する。

## 11.3 Alpha

```text
Opaque
    Deferred GBuffer

Mask
    Deferredまたは専用Masked Pass
    Shadowでも同じAlpha Cutoff

Blend
    Transparent Forward Pass
    Depth Write / Sort PolicyをMaterial Contractで指定
```

Opacity TextureとBaseColor Alphaの合成規則を固定する。

## 11.4 Normal / Bump

- Normal TextureはTBNでTangent SpaceからWorld / View Spaceへ変換
- Tangent Signを使用
- Non-uniform Scale時はNormal Matrixを使用
- Skinned MeshでもTangentをSkinningする
- BumpとNormalを併用する場合の合成順を固定
- Shadow Passでは通常Normal / BumpをSampleしない

---

# 12. Editor / UX

## 12.1 Model Import Inspector

表示:

- Source Application / FBX Version
- Axis / Unit / Scale
- Mesh / Bone / Animation / Material数
- Material Slot一覧
- Texture Dependency一覧
- Missing / Unsupported Channel
- Normal Convention
- Tangent Source
- UV Set一覧
- Import Warning / Error

## 12.2 Material Inspector

- Material Preview Sphere / Plane / Imported Mesh
- 各Texture SlotのDrag & Drop
- Texture Thumbnail
- sRGB / Linear表示
- UV Set
- Tiling / Offset / Rotation
- Wrap / Filter
- Channel Swizzle
- Normal Strength / Bump Scale
- Alpha Mode / Cutoff
- Double-Sided
- Source Imported ValueとUser Overrideの区別

## 12.3 Reimport Diff

Reimport前後で次を表示する。

- Material追加 / 削除 / Rename
- Texture Path変更
- Parameter変更
- UV Set変更
- Tangent / Normal再生成
- Override保持 / Orphan

---

# 13. 実装工程

## MMI-0: Current Import Gap Audit

- [ ] Blender / Maya FBX Fixtureを準備
- [ ] 現在取得できるAssimp Material PropertyをDump
- [ ] Embedded / External Texture Pathを列挙
- [ ] 現行Shader Slot / Flag / Sampling実装を一覧化
- [ ] Static Batch Material Keyへの影響を確認

## MMI-1: Import Intermediate Representation

- [ ] `ImportedModelDocument`
- [ ] `ImportedMesh`
- [ ] `ImportedMaterial`
- [ ] `ImportedTextureReference`
- [ ] Source Metadata / Diagnostic
- [ ] Assimp Adapter
- [ ] RuntimeからAssimp Material参照を除去

## MMI-2: Texture Import Foundation

- [ ] External / Embedded Texture Resolver
- [ ] UTF-8 / Relative / Absolute Path
- [ ] Color Space
- [ ] Texture Semantic
- [ ] Mipmap生成
- [ ] Import Cache / Content Hash
- [ ] Missing Texture Placeholder
- [ ] Texture Revision

## MMI-3: Material Asset Foundation

- [ ] Canonical `MaterialAsset`
- [ ] Material Slot / SubMesh Assignment
- [ ] Material Override Table
- [ ] Parameter / Texture Binding
- [ ] Sampler / UV Transform
- [ ] YAML / Asset Serialization
- [ ] Material Revision

## MMI-4: Standard PBR Texture Set

- [ ] Base Color
- [ ] Normal
- [ ] Roughness
- [ ] Metallic
- [ ] AO
- [ ] Emissive
- [ ] Opacity
- [ ] Packed ORM / Swizzle
- [ ] Fallback Texture Binding

## MMI-5: Tangent / UV Correctness

- [ ] Tangent `float4` + Sign
- [ ] MikkTSpace
- [ ] Mirrored UV Test
- [ ] UV0 / UV1
- [ ] TextureごとのUV Set
- [ ] Normal Y Convention
- [ ] Skinned Tangent更新

## MMI-6: Bump / Height

- [ ] Bump Semantic
- [ ] Bump Scale
- [ ] Import-time Bump to Normal変換
- [ ] Runtime Bump Modeの可否判断
- [ ] Height Asset保持
- [ ] Optional Parallax Material Feature

## MMI-7: Alpha / State / Advanced Material

- [ ] Opaque / Mask / Blend
- [ ] Alpha Cutoff Shadow Parity
- [ ] Double-Sided
- [ ] Render Queue / Sort Policy
- [ ] Specular / Glossiness変換
- [ ] Clear Coat / Transmissionの段階対応

## MMI-8: RenderWorld / RHI Integration

- [ ] Material Snapshot / Handle
- [ ] Texture / Sampler RHI Binding Set
- [ ] `RenderableModel`のDirect D3D11 Bind撤去
- [ ] RenderOperation Material Binding
- [ ] Static Batch Key更新
- [ ] Device Lost / Recreate

## MMI-9: DCC Compatibility Profiles

- [ ] Blender FBX Profile
- [ ] Maya FBX Profile
- [ ] Generic FBX Profile
- [ ] glTF 2.0 Profile
- [ ] Legacy Lambert / Phong / Blinn変換
- [ ] Unsupported Node / Procedural診断

## MMI-10: Editor / Reimport / Validation

- [ ] Model Import Inspector
- [ ] Material Inspector
- [ ] Texture Dependency View
- [ ] Reimport Diff
- [ ] Override保持
- [ ] Golden Image Validation
- [ ] Asset Import Smoke Test

---

# 14. 検証Fixture

最低限、BlenderとMayaそれぞれから同等SceneをExportする。

## Material Fixture

- BaseColorのみ
- BaseColor Texture
- Normal Texture
- Bump Texture
- Roughness Texture
- Metallic Texture
- AO Texture
- Emissive Texture
- Opacity Mask
- Alpha Blend
- Packed ORM
- Multiple Material / Multiple SubMesh
- Embedded Texture
- Relative External Texture
- Missing Texture
- Same Image used as sRGB and Linear
- UV0 / UV1
- Tiling / Offset
- Wrap / Clamp
- Double-Sided

## Geometry Fixture

- Custom Normal
- Missing Normal
- Imported Tangent
- Missing Tangent
- Mirrored UV
- Negative Scale
- Non-uniform Scale
- Skinned Mesh with Normal Map
- Multiple Mesh Node
- Unit Scale違い
- Axis設定違い

## 完了条件

- Blender / Mayaの標準File Texture Materialが同じ見た目の範囲へ収束する
- SubMeshごとのMaterial Assignmentが一致する
- Normal MapがMirrored UVとSkinningで破綻しない
- Missing Textureで前DrawのTextureが漏れない
- Alpha MaskがColor / Shadowで同じCutoffを使用する
- ReimportでMaterial Overrideが維持される
- Runtime Draw PathがAssimp Materialへ依存しない
- ModelDataがNative Texture / SRVを所有しない
- D3D11 / 将来Backendで同じMaterial Assetを使用できる

---

# 15. RenderPipeline Graphとの関係

本StepはMaterial AssetとRaster Bindingの入力を整備する。
RenderPipeline GraphはMaterial内部のTexture構成をNode接続として展開しない。

```text
RasterOperation
    DrawList
    MaterialPassKey
    MaterialBindingSet
        ├─ Constant Parameters
        ├─ Texture Bindings
        ├─ Samplers
        └─ Feature Mask
```

Pipeline Graph側の責務:

- GBuffer / Forward / Shadow / Depth Pass選択
- Required Material Passの検証
- Transparent / Masked Passへの振分け
- Material Binding SetをRHIへLowering

Material Asset側の責務:

- Texture Semantic
- Parameter
- UV / Sampler
- Alpha / Cull
- Shader Feature
- Source Import Metadata

---

# 16. 実行順

Step 18-I全体をRenderPipeline Graph完了後まで待たせない。

```text
RHI Device Ownership / Geometry Revision
    ↓
RenderPacket Model Resource Snapshot
    ↓
MMI-0 Import Gap Audit
    ↓
MMI-1 Import IR
    ↓
MMI-2 Texture Foundation
    ↓
MMI-3 Material Asset
    ↓
MMI-4 PBR Texture Set
    ↓
MMI-5 Tangent / UV Correctness
    ↓
RenderWorld -> RHI Command境界
    ↓
MMI-8 RHI Integration
```

並行可能:

- MMI-0〜3はShadow Baseline取得と並行可能
- Blender / Maya Fixture作成はRHI Ownership修正と並行可能
- Editor Material InspectorはMaterial Asset Schema確定後に並行可能
- Bump / Advanced Materialは基本PBR経路の後に並行可能

先行禁止:

- Material Asset未導入のままTexture SlotだけをComponentへ追加する
- Color Space未定義のままNormal / Roughness Textureを共通Textureとして読む
- Tangent Sign未対応のままNormal Map対応完了とする
- Draw中の`aiMaterial`参照を恒久化する
- FBX Source Application名だけでAxis / Unitを決める
- Missing Texture時にSRVを未Bindのまま残す

---

# 17. 直近Action

```text
[ ] Blender Material Fixtureを追加
[ ] Maya Material Fixtureを追加
[ ] Assimp Material Dump Toolを追加
[ ] aiTextureTypeごとの現行Import結果を記録
[ ] ImportedMaterial IRを設計
[ ] Texture Color Space / Semantic Contractを固定
[ ] ModelDataからMaterial / Native Texture所有を分離する移行案を実装
```
