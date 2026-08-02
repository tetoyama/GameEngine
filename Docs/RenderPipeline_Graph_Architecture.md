# RenderPipeline Graph 設計方針

Status: **Architecture v1 — 2026-08-02**

対象:

- `refactor/ecs-scheduler-foundation`
- PR #45

関連文書:

- `Docs/ECS_Scheduler_Migration_Plan.md`
- `Docs/Step16_RHI_MultiBackend_Architecture.md`
- `Docs/Step18A_RenderWorld_Runtime_Ownership_Progress.md`
- `Docs/RenderPipeline_Graph_Integration_Plan.md`

本書の`RenderPipelineGraph`は、Step 16-Fで実装済みの低水準`RenderGraph`を置き換えない。

```text
RenderPipelineGraph
    高水準Node、Asset、Editor、Pipeline Compile
        ↓ RenderOperationへ展開
RenderGraph
    GPU Resource依存、Barrier、Lifetime、Pass Culling、Queue同期
        ↓
RHI / D3D11 Backend
```

---

## 1. 目的

既存のPostProcess Nodeを拡張し、ポストプロセスだけでなく、Shadow、GBuffer、Deferred Lighting、Transparent、Fog、UIなど、フレーム全体の描画処理をNodeとして構築可能にする。

主な目的は次の通り。

- 既存のPostProcess Node資産を維持する
- RenderPassの追加、変更、接続自由度を高める
- Nodeごとの入出力と依存関係を明示する
- Nodeプレビューと本番描画を同じ定義から実行する
- 既存Rendererを段階的に移行する
- コードベースのカスタム処理を許容する
- DirectX 11のリソース競合を中央管理する

単にRenderPassの実行順を並べるのではなく、Node間のリソース接続から実行順とリソース寿命を導出する。

---

## 2. 全体アーキテクチャ

システムを次の層に分離する。

```text
RenderPipelineGraphAsset
    Node、接続、デフォルト設定を保存
            ↓ コンパイル

CompiledRenderPipeline
    Node展開、実行順、Binding、依存関係を保持
            ↓ インスタンス化

RenderPipelineInstance
    カメラ固有状態、History、動的パラメータを保持
            ↓ フレーム準備

RenderOperation
    Raster、Fullscreen、Compute、Copyなどの処理
            ↓ 展開

RenderGraph
    リソース依存、寿命、競合、実行順を管理
            ↓

DirectX 11 Graphics Layer
```

### RenderPipelineGraphAsset

保存・編集対象となる高水準グラフ。

- Node配置
- Node接続
- 公開パラメータ
- Subgraph
- Node型情報
- プレビュー設定
- YAML保存

を担当する。

### CompiledRenderPipeline

Assetを検証・展開した実行可能な中間表現。

- Slot Indexの解決
- Node依存の解析
- Operationへの展開
- Shader Bindingの検証
- 安定した実行順
- エラー情報

を保持する。

### RenderPipelineInstance

カメラやViewportごとの実行状態。

- Temporal History
- 前フレームTexture
- Auto Exposure状態
- カメラ固有パラメータ
- 一時リソース
- Node実行状態

を持つ。

Asset側に実行状態を置かない。Scene View、Game View、複数Cameraの状態混在を防ぐ。

---

## 3. Nodeの基本構造

Nodeは高水準な描画機能を表す。

```cpp
struct RenderPipelineNodeAsset
{
    RenderNodeId id = InvalidRenderNodeId;

    std::string type;
    uint32_t typeVersion = 1;

    std::string name;

    std::vector<ResourceSlotDesc> inputs;
    std::vector<ResourceSlotDesc> outputs;

    std::vector<NodeParameterDesc> parameters;

    std::vector<RenderOperationDesc> operations;
    std::vector<RenderPrepareEventDesc> prepareEvents;

    NodePreviewDesc preview;
    NodeExecutionPolicy executionPolicy;
};
```

NodeはDirectX 11の命令を直接実行しない。

Nodeは、

- 入力リソース
- 出力リソース
- パラメータ
- 事前準備イベント
- 実行するOperation

を宣言する。

---

## 4. Node ID

UUIDは使用しない。

永続化するNode IDには、Graphローカルな単調増加`uint64_t`を使用する。

```cpp
using RenderNodeId = uint64_t;

inline constexpr RenderNodeId InvalidRenderNodeId = 0;
```

```cpp
class RenderPipelineGraphAsset
{
public:
    RenderNodeId AllocateNodeId() noexcept
    {
        return m_nextNodeId++;
    }

private:
    RenderNodeId m_nextNodeId = 1;
};
```

削除したIDは再利用しない。

```text
Node 1、2、3を作成
Node 2を削除
次のNodeは4
```

Graphを複製・統合する場合は、新しいIDを発行して接続をリマップする。

実行時は別のHandleを使用する。

```cpp
struct RenderNodeHandle
{
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
};
```

役割を次のように分離する。

```text
RenderNodeId
    保存
    接続
    Undo / Redo
    Editor選択
    エラー表示

RenderNodeHandle
    コンパイル後の高速参照
    削除済み参照の検出
```

Subgraph内部のIDは書き換えず、実行時にGraph Instanceの名前空間を与える。

```text
MainGraph/Node:12
MainGraph/PostProcess:8/Node:3
```

---

## 5. リソーススロット

Node間ではRenderTargetだけでなく、描画に必要な各種リソースを型付きスロットとして扱う。

```cpp
enum class RenderResourceType
{
    Texture2D,
    TextureCube,
    DepthTexture,
    StructuredBuffer,
    ConstantBuffer,
    DrawList,
    RenderView
};
```

```cpp
enum class RenderAccess
{
    Read,
    Write
};
```

物理的なReadWriteは原則として使用しない。

同一リソースを更新する場合も、内部では別バージョンとして扱う。

```text
HDRScene v0
    ↓ Fog
HDRScene v1
    ↓ Bloom
HDRScene v2
```

これにより、同一TextureのSRV・RTV同時Bindを防ぐ。

---

## 6. リソース契約

単なる`Texture2D`だけではなく、意味と形式を契約として保持する。

```cpp
struct TextureContract
{
    TextureDimension dimension;
    TextureUsage usage;

    DXGI_FORMAT format;
    ColorSpace colorSpace;

    TextureSemantic semantic;

    ResolutionConstraint resolution;
    SampleConstraint samples;

    bool requiresMipmaps = false;
};
```

Semanticの例：

```text
HDRColor
DisplayColor

SceneDepth
LinearDepth

WorldNormal
ViewNormal

MotionVectorUV
MotionVectorPixels

ShadowDepth
ObjectId
```

これにより、次の誤接続を検出する。

- HDRとLDR
- LinearとsRGB
- 通常DepthとLinear Depth
- World NormalとView Normal
- MSAA Textureと通常Texture
- Motion Vectorの単位違い

---

## 7. 出力リソース生成情報

出力スロットは、RenderTargetの生成条件を持つ。

```cpp
struct RenderTextureDesc
{
    ResolutionPolicy resolution;
    DXGI_FORMAT format;

    uint32_t mipLevels = 1;
    uint32_t sampleCount = 1;

    bool allowRenderTarget = true;
    bool allowShaderResource = true;
    bool allowUnorderedAccess = false;

    LoadOperation loadOperation;
    StoreOperation storeOperation;
};
```

解像度は次の方式に対応する。

```cpp
enum class ResolutionMode
{
    Absolute,
    RelativeToViewport,
    MatchInput,
    Custom
};
```

例：

```text
GBuffer
    Viewportと同じ

SSAO
    Viewportの0.5倍

Bloom
    入力Textureの0.5倍

ShadowMap
    2048 × 2048
```

---

## 8. RenderOperation

Node内部の処理はShader配列ではなく、Operation配列として表現する。

```cpp
using RenderOperation = std::variant<
    ClearOperation,
    RasterOperation,
    FullscreenOperation,
    ComputeOperation,
    CopyOperation,
    ResolveOperation,
    GenerateMipsOperation,
    ExternalOperation
>;
```

### RasterOperation

Scene Geometryを描画する。

```cpp
struct RasterOperation
{
    DrawListHandle drawList;

    RenderFilter filter;
    RenderSortMode sortMode;
    MaterialPassKey materialPass;

    RenderTargetBindings targets;
    ParameterBindingSet parameters;
};
```

GBuffer、Shadow、Transparent、Decalなどに使用する。

### FullscreenOperation

Fullscreen Triangleを利用する処理。

- Deferred Lighting
- Fog合成
- Bloom
- Tone Mapping
- Color Grading

などに使用する。

### ComputeOperation

Compute Shader処理。

- Blur
- GPU Culling
- Lighting
- Particle
- Texture生成

などに使用する。

### ExternalOperation

既存Legacy Pass、Effekseer、ImGuiなど、通常Operationへ移行できない処理に使用する。

ExternalOperationもRead・Writeするリソースを宣言する。

---

## 9. NodeとRenderPassの粒度

Editor上のNodeと、GPU上のRenderPassは1対1にしない。

例えばBloom Nodeは、内部で複数Passへ展開する。

```text
Bloom Node
    ↓

Bloom.Extract
Bloom.Downsample0
Bloom.Downsample1
Bloom.Downsample2
Bloom.Upsample2
Bloom.Upsample1
Bloom.Composite
```

GBuffer Nodeも同様に複数Passへ展開できる。

```text
GBuffer Node
    ↓

GBuffer.Clear
GBuffer.Static
GBuffer.Skinned
GBuffer.Terrain
GBuffer.Decal
```

Editor上では高水準機能として読みやすく保ち、RenderGraph上では細かい依存管理を行う。

---

## 10. コードベース事前実行イベント

Nodeはコードベースの任意事前実行イベントを持てる。

主な用途は次の通り。

- DrawList生成
- DrawListフィルタリング
- DrawListソート
- カリング条件計算
- Shader変数計算
- Nodeパラメータ計算
- RenderView派生
- Shadow Camera計算
- Reflection View計算
- LOD選択
- Operation有効・無効判定

コードロジック自体は自由とする。

ただし、イベントへEngineContext全体や`ID3D11DeviceContext`を直接渡さない。

```cpp
class RenderNodePrepareContext
{
public:
    const SceneRenderSnapshot& GetScene() const noexcept;
    const RenderView& GetBaseView() const noexcept;

    DrawListBuilder& GetDrawListBuilder() noexcept;
    NodeParameterWriter& GetParameterWriter() noexcept;
    RenderViewBuilder& GetViewBuilder() noexcept;
    RenderOperationBuilder& GetOperationBuilder() noexcept;

    const FrameTime& GetFrameTime() const noexcept;
    RenderExecutionMode GetExecutionMode() const noexcept;
};
```

任意なのは計算ロジックであり、変更可能な対象はContextで制限する。

### 許可する処理

```text
Node専用DrawListの生成
DrawListのフィルタ・ソート
Node変数の計算
Node専用RenderViewの生成
Operationの構築
ローカル実行条件の計算
```

### 原則として禁止する処理

```text
DirectX 11 Contextの直接操作
直接Draw Callを発行
未宣言RenderTargetへの描画
Scene Componentの変更
Scene Camera Transformの変更
他Node内部状態の変更
隠れたGPUリソース操作
```

GPU処理が必要な場合は、事前イベントからOperationを登録する。

---

## 11. Prepare Eventの登録

イベントはコード上の登録型として扱う。

```cpp
class IRenderNodePrepareEvent
{
public:
    virtual ~IRenderNodePrepareEvent() = default;

    virtual RenderPrepareCapability
        GetCapabilities() const noexcept = 0;

    virtual void Prepare(
        RenderNodePrepareContext& context) = 0;
};
```

Graph Assetには型名とパラメータを保存する。

```yaml
prepareEvents:
  - type: BuildOpaqueDrawList
    typeVersion: 1

    parameters:
      layerMask: 4294967295
      includeStatic: true
      includeDynamic: true
```

Registryから実装を生成する。

```cpp
eventRegistry.Register<BuildOpaqueDrawListEvent>(
    "BuildOpaqueDrawList");
```

---

## 12. Event Capability

イベントが利用する機能を宣言する。

```cpp
enum class RenderPrepareCapability : uint32_t
{
    None            = 0,
    ReadScene       = 1 << 0,
    WriteDrawList   = 1 << 1,
    WriteParameters = 1 << 2,
    WriteLocalView  = 1 << 3,
    BuildOperations = 1 << 4,
    MainThreadOnly  = 1 << 5,
    ExternalState   = 1 << 6
};
```

Capabilityは次の判断に使用する。

- 並列実行可能か
- Main Threadが必要か
- Nodeプレビュー可能か
- キャッシュ可能か
- 外部副作用を持つか

---

## 13. DrawList

Scene全体のDrawListを直接編集しない。

```text
SceneRenderSnapshot
       ↓
Prepare Event
       ↓
Node専用DrawList
       ↓
RasterOperation
```

DrawListはNodeの入力・出力として扱える。

```text
[Culling Node]
      ↓ VisibleDrawList
[GBuffer Node]
```

DrawList Builderでは次を設定できるようにする。

- Layer
- Tag
- Static / Dynamic
- Frustum
- Occlusion
- Material Pass
- Custom Predicate
- Sort Key
- LOD条件

---

## 14. パラメータ

Nodeは自由定義可能なパラメータを持つ。

```cpp
enum class ParameterSource
{
    Constant,
    GraphInput,
    EngineSemantic,
    MaterialProperty,
    NodeOutput,
    PrepareEvent
};
```

例：

```text
FogDensity
    Constant

RainStrength
    EngineSemantic

CameraPosition
    EngineSemantic

CalculatedFogDensity
    PrepareEvent
```

Prepare Eventは直接Constant Bufferへ書かず、論理ParameterBlockへ値を書き込む。

```text
Prepare Event
    ↓
ParameterBlock
    ↓
Reflection検証
    ↓
GPU Constant Buffer更新
```

Node公開パラメータと、Operation内部パラメータは分離する。

---

## 15. RenderView

視点変更はScene Camera自体を変更せず、Node専用の派生RenderViewを生成する。

```text
Scene Camera
    ↓ Base RenderView

Shadow Prepare Event
    ↓ Shadow RenderView

Shadow Raster Operation
```

用途：

- Shadow
- Reflection Probe
- Portal
- Minimap
- Cubemap Capture
- Cascaded Shadow Map

Node内部だけで使用するViewと、後続Nodeへ渡すViewを区別する。

```cpp
enum class RenderViewScope
{
    LocalOperations,
    NodeOutput
};
```

後続Nodeへ渡す場合は、明示的な`RenderView`出力スロットを使用する。

---

## 16. 実行段階

処理を次の段階へ分離する。

```text
1. Pipeline Compile
   Node構造、Slot、依存を解析

2. Frame Prepare
   DrawList、パラメータ、RenderViewを計算

3. RenderGraph Build
   そのフレームのOperationを確定

4. GPU Execute
   DirectX 11命令を実行
```

コードベース事前イベントは原則として`Frame Prepare`で実行する。

---

## 17. RenderGraph

RenderGraphはOperationから実際のGPU実行順を構築する。

主な責務：

- Read・Write依存解析
- トポロジカルソート
- 循環依存検出
- Resource Version管理
- Transient Resource寿命解析
- 未使用Pass削除
- RTV・SRV・UAV競合解除
- 物理RenderTarget再利用
- 安定した実行順の保証

実行順はNodeの画面上の位置や、任意のPriority値を主軸にしない。

```text
GBufferがSceneDepthを生成
SSAOがSceneDepthを読む
LightingがSSAOを読む
```

という接続から、

```text
GBuffer
  ↓
SSAO
  ↓
Lighting
```

を導出する。

依存上どちらが先でもよい処理は、

```text
Domain
NodeId
Operation Index
```

などで安定ソートする。

---

## 18. Temporalリソース

Temporal処理では、現在フレームと前フレームを区別する。

```cpp
enum class ResourceTemporalAccess
{
    CurrentFrame,
    PreviousFrame
};
```

```cpp
enum class ResourceLifetime
{
    Transient,
    FramePersistent,
    History,
    External
};
```

例：

```text
TAA Node

Inputs
- CurrentColor
- MotionVector
- SceneDepth
- PreviousFrameColor

Outputs
- ResolvedColor
- NextFrameColor
```

通常の循環依存と、前フレーム参照を明確に分ける。

Camera Cut、解像度変更、Scene変更時にはHistoryを無効化する。

---

## 19. Nodeプレビュー

Nodeプレビューは、別実装を用意するのではなく、本番と同じNode定義から依存サブグラフを構築する。

```text
ToneMappingをプレビュー
    ↓
HDRSceneが必要
    ↓
Deferred Lightingが必要
    ↓
GBufferが必要
```

プレビューモード：

```cpp
enum class NodePreviewMode
{
    OutputPreview,
    IsolatedPreview,
    PipelinePreview,
    DebugView
};
```

入力方式：

```cpp
enum class PreviewInputMode
{
    LiveFrame,
    CapturedFrame,
    DefaultTexture,
    CustomTexture
};
```

プレビュー時には実行モードをContextへ渡す。

```cpp
enum class RenderExecutionMode
{
    Runtime,
    EditorViewport,
    NodePreview,
    Thumbnail
};
```

副作用を持つ処理にはプレビューポリシーを設定する。

```cpp
enum class PreviewPolicy
{
    ExecuteNormally,
    UseCachedResult,
    UseFallback,
    Skip
};
```

---

## 20. 既存PostProcess Nodeとの互換性

既存PostProcess Nodeは廃止しない。

新しい汎用基底型へ適合させる。

```cpp
class RenderPipelineNode;
class PostProcessNode : public RenderPipelineNode;
```

既存PostProcess GraphはSubgraph Nodeとして扱える。

```text
RenderPipelineGraph
├─ Shadow
├─ GBuffer
├─ Deferred Lighting
├─ PostProcess Subgraph
│   ├─ Bloom
│   ├─ ToneMapping
│   └─ FXAA
├─ UI
└─ Present
```

既存NodeのShader配列は、初期段階ではFullscreenOperation配列へ変換する。

---

## 21. 既存RenderPassとの互換性

既存のShadow、GBuffer、Lightingなどは、最初から全面的に書き換えない。

Legacy Nodeとして包む。

```cpp
class LegacyRenderPassNode final
    : public RenderPipelineNode
{
    std::shared_ptr<IRenderPass> legacyPass;
};
```

初期構成：

```text
Shadow Legacy Node
GBuffer Legacy Node
Lighting Legacy Node
PostProcess Subgraph
UI Legacy Node
Present Node
```

Legacy PassはExternalOperationとして登録する。

移行状態を可視化する。

```text
Declared
    全リソースアクセスが宣言済み

Partially Declared
    一部副作用が不明

Opaque
    内部操作が不透明
```

OpaqueなPassの前後では、Pass削除やリソース再利用を制限する。

---

## 22. Editor ViewのPipeline選択

Editor Viewでは、視点とPipelineを別々に選択できるようにする。

```cpp
enum class EditorViewSource
{
    EditorCamera,
    SceneCamera
};

enum class EditorPipelineSource
{
    BuiltinPipeline,
    SceneCameraPipeline,
    CustomPipeline
};
```

```cpp
struct EditorViewportRenderSettings
{
    EditorViewSource viewSource =
        EditorViewSource::EditorCamera;

    EditorPipelineSource pipelineSource =
        EditorPipelineSource::BuiltinPipeline;

    EntityId sceneCamera = InvalidEntityId;
    RenderPipelineAssetHandle customPipeline;

    bool enableEditorOverlays = true;
};
```

想定する組み合わせ：

| 視点 | Pipeline | 用途 |
|---|---|---|
| Editor Camera | Builtin | 安定した通常編集 |
| Editor Camera | Scene Camera Pipeline | 自由視点でゲームの画作りを確認 |
| Scene Camera | Builtin | Camera構図だけを標準描画で確認 |
| Scene Camera | Scene Camera Pipeline | 実際のGame View相当 |

---

## 23. BuiltinPipeline

Engine標準のBuiltinPipelineを用意する。

最低限、次を保証する。

```text
Shadow
GBuffer
Deferred Lighting
Transparent
Basic Tone Mapping
Present
```

次の場合にはBuiltinPipelineへフォールバックする。

- CameraにPipelineが設定されていない
- Pipeline Assetのロード失敗
- Graphコンパイル失敗
- Shaderコンパイル失敗
- 必須出力がない
- 循環依存がある

自動フォールバックした場合は、Editorへ理由を表示する。

```text
Scene Camera Pipeline failed to compile.
Editor viewport is using BuiltinPipeline.
```

---

## 24. Editor Overlay

Grid、Gizmo、Selection Outline、Object IDなどは、特定Pipelineへ直接組み込まない。

選択したPipelineの後段へ、Editor Overlay Decoratorとして注入する。

```text
Selected Render Pipeline
        ↓ FinalColor / SceneDepth

Editor Overlay Decorator
        ├─ Grid
        ├─ Selection ID
        ├─ Outline
        ├─ Gizmo
        └─ ImGui
```

各Pipelineは標準出力Semanticを公開する。

```cpp
struct RenderPipelineOutputs
{
    RGTextureHandle finalColor;

    std::optional<RGTextureHandle> sceneDepth;
    std::optional<RGTextureHandle> objectId;
    std::optional<RGTextureHandle> motionVectors;
};
```

`FinalColor`は必須。

`SceneDepth`などがない場合は、対応するEditor機能を縮退させる。

---

## 25. 検証

Pipeline Compilerは次を検証する。

- 必須入力の未接続
- 生成前リソースのRead
- 同一バージョンへの複数Write
- 現在フレーム内の循環依存
- Resource Type不一致
- Semantic不一致
- Color Space不一致
- Texture Format不一致
- 解像度不一致
- MSAA数不一致
- UAV非対応Format
- Shader ReflectionとのBinding不一致
- 不正なHistory参照
- 出力Semantic不足
- Prepare Event Capability違反

不正なGraphは黙って補正せず、具体的なエラーを表示する。

---

## 26. 保存データのバージョン

Graph AssetにはSchema Versionを持たせる。

```yaml
renderPipelineGraph:
  schemaVersion: 1
  nextNodeId: 18
```

Node、Prepare Eventにも型Versionを持たせる。

```yaml
type: Bloom
typeVersion: 2
```

ロード時に段階的なマイグレーションを行う。

不明なNode型やEvent型は削除せず、Placeholderとして元データを保持する。

---

## 27. 初期実装の順序

### Phase 1：汎用Node基盤

- RenderPipelineNode
- RenderNodeId
- ResourceSlot
- NodeParameter
- RenderOperation
- Prepare Event
- Subgraph

を追加する。

### Phase 2：既存PostProcess Node適合

既存PostProcess NodeをFullscreenOperationへ変換する。

### Phase 3：既存RenderPassのNode化

Shadow、GBuffer、Lighting、UIなどをLegacy Nodeで包む。

描画結果と順序は変更しない。

### Phase 4：Asset・Compiled・Instance分離

Pipelineの保存状態と実行状態を分離する。

### Phase 5：RenderGraph

- Read・Write依存解析
- トポロジカルソート
- 循環検出
- Resource Version
- SRV・RTV・UAV競合解除

を実装する。

### Phase 6：コードベースPrepare Event

- DrawList生成
- パラメータ計算
- RenderView派生
- Capability検証

を実装する。

### Phase 7：Nodeプレビュー

依存サブグラフ、Captured Frame、Custom Texture入力を実装する。

### Phase 8：BuiltinPipelineとEditor統合

- BuiltinPipeline
- Pipeline切替
- View Source切替
- Editor Overlay注入
- エラー時フォールバック

を実装する。

### Phase 9：Legacy Pass移行

次の順序で正式Operationへ移行する。

1. PostProcess
2. Deferred Lighting
3. GBuffer
4. Shadow
5. Transparent
6. UI・外部Renderer

### Phase 10：最適化

- Transient Resource Pool
- 未使用Pass削除
- 物理Texture再利用
- Shader Permutation
- GPU時間表示

を追加する。

---

## 28. 初期段階で実装しないもの

初期実装では次を対象外とする。

- DX12、Vulkan前提の過剰なAPI抽象化
- Async Compute自動スケジューリング
- 複雑なMemory Aliasing
- Runtime Scriptからの無制限なPass生成
- Nodeからの直接D3D11 Context操作
- GraphによるScene Component変更
- 出力接続状態による過剰なPermutation生成

まずはDirectX 11上で、安全にRenderPassを追加・接続・プレビューできることを優先する。

---

## 29. 最終方針

設計上の中心原則は次の通り。

1. 既存PostProcess Nodeを汎用RenderPipeline Nodeへ拡張する
2. NodeはRead・Writeするリソースを明示する
3. NodeはShader配列ではなくOperation配列を持つ
4. Editor NodeとGPU RenderPassの粒度を分離する
5. 実行順はリソース依存から決定する
6. 同一リソースの更新はVersionとして扱う
7. Prepare Eventでは任意のC++ロジックを許可する
8. Prepare Eventの操作範囲は専用Contextで制限する
9. DrawList、パラメータ、RenderViewはNode単位で生成する
10. GPU命令とリソース操作はRenderGraph実行層へ集約する
11. Node IDにはGraphローカルな単調増加整数を使用する
12. Pipeline Asset、Compiled Pipeline、Instanceを分離する
13. Nodeプレビューは本番と同じ定義から生成する
14. Legacy PassはAdapterで段階的に移行する
15. Editorでは視点とPipelineを別々に選択可能にする
16. BuiltinPipelineを標準描画およびフォールバックとして保証する
17. Editor Overlayは選択Pipelineの後段へ注入する

この構造により、既存設計との互換性を維持しながら、Shadow、GBuffer、Lighting、Fog、Cloud、PostProcess、UIまでを統一されたNodeシステム上で扱えるようにする。
