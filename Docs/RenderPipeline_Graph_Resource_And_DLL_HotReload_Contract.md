# RenderPipeline Graph Resource / DLL Hot Reload Contract

Status: **Architecture Amendment v1.1 — 2026-08-02**

対象:

- `refactor/ecs-scheduler-foundation`
- PR #45

本書は次の文書へ対する規範的な追補である。

- `Docs/RenderPipeline_Graph_Architecture.md`
- `Docs/RenderPipeline_Graph_Integration_Plan.md`
- `Docs/ECS_Scheduler_Migration_Plan.md`

本書と既存文書の記述が衝突する場合、Resource TypeとDLL Hot Reloadについては本書を優先する。

---

# 1. RenderResourceType拡張

`RenderResourceType`は次を正式定義とする。

```cpp
enum class RenderResourceType
{
    Texture2D,
    Texture2DArray,
    Texture3D,

    TextureCube,
    TextureCubeArray,

    DepthTexture,

    StructuredBuffer,
    ByteAddressBuffer,
    IndirectArgumentBuffer,

    ConstantBuffer,
    DrawList,
    RenderView
};
```

## 1.1 各型の用途

| Type | 主用途 |
|---|---|
| `Texture2D` | 通常のColor、Normal、Motion Vector、PostProcess入出力 |
| `Texture2DArray` | Cascade、Layered Render Target、Texture Array |
| `Texture3D` | Volumetric Fog、Cloud Volume、3D LUT、Voxel Data |
| `TextureCube` | Environment Map、Point Reflection、Cube Capture |
| `TextureCubeArray` | 複数Probe、複数Point Shadow、Layered Cube Resource |
| `DepthTexture` | Depth Attachment、Shadow Depth、Scene Depth |
| `StructuredBuffer` | Light、Instance、Particle、GPU Culling Result |
| `ByteAddressBuffer` | Raw Buffer、Packed Data、GPU生成可変Layout Data |
| `IndirectArgumentBuffer` | Draw / Dispatch Indirect Argument |
| `ConstantBuffer` | Frame、View、Material、Node Parameter Block |
| `DrawList` | CPU側または論理描画Item列 |
| `RenderView` | Camera、Shadow、Reflection、Portal等の派生View |

## 1.2 Resource TypeとTexture Contractの責務

`RenderResourceType`だけでTextureの完全な互換性を判定しない。

Texture系Resourceでは、少なくとも次を併用する。

```cpp
struct TextureContract
{
    RenderResourceType resourceType;
    RHITextureDimension dimension;
    RHITextureUsage usage;
    RHITextureFormat format;
    ColorSpace colorSpace;
    TextureSemantic semantic;
    ResolutionConstraint resolution;
    SampleConstraint samples;
    uint32_t arrayLayers = 1;
    bool requiresMipmaps = false;
};
```

`DepthTexture`はDepth用途を表す論理Resource Typeであり、2D、Array、Cube等の物理Dimensionは`dimension`と`arrayLayers`で確定する。

例:

```text
Directional Shadow Atlas
    resourceType = DepthTexture
    dimension    = Texture2D

Cascaded Shadow Array
    resourceType = DepthTexture
    dimension    = Texture2DArray
    arrayLayers  = cascadeCount

Point Shadow Cube Array
    resourceType = DepthTexture
    dimension    = TextureCubeArray
    arrayLayers  = lightCount
```

これにより、Color TextureとDepth Textureの誤接続をResource Typeで拒否しつつ、DepthのDimensionを一種類へ固定しない。

## 1.3 Buffer Contract

Buffer系Resourceは用途を明示する。

```cpp
struct BufferContract
{
    RenderResourceType resourceType;
    uint32_t stride;
    uint64_t elementCount;
    RHIBufferUsage usage;
    RHIBufferBindFlags bindFlags;
    bool cpuReadable = false;
    bool cpuWritable = false;
};
```

検証規則:

- `StructuredBuffer`は`stride > 0`を必須とする
- `ByteAddressBuffer`はRaw View対応と4Byte Alignmentを必須とする
- `IndirectArgumentBuffer`はRHI CapabilityとArgument Layoutを検証する
- `ConstantBuffer`はBackendのAlignment制約をCompilerまたはRHI層で検証する
- Resource Typeが異なるBuffer間を暗黙変換しない

## 1.4 Slot互換性

Slot接続時には次を検証する。

- Resource Type
- Texture Dimension
- Array Layer要件
- Format
- Semantic
- Color Space
- Resolution
- Sample Count
- Mipmap要件
- Buffer Stride
- Buffer Bind Flags
- Indirect Argument Layout

`Texture2D`から`Texture2DArray`、`TextureCube`から`TextureCubeArray`などへの暗黙昇格は行わない。

必要な変換は明示NodeまたはCopy / Convert Operationで表現する。

---

# 2. RenderOperationのDLL Hot Reload対応

## 2.1 基本方針

`RenderOperation`はHot Reload可能なDLL定義型を直接保持しない。

禁止する構造:

```cpp
std::variant<
    BuiltinOperation,
    PluginDefinedCppType
>;
```

また、次をCompiled PipelineやFrame Queueへ保持しない。

- DLL内vtableを持つObject
- DLL内関数を指す生Function Pointer
- DLL側Allocatorで確保された`std::string` / `std::vector`
- DLL側Destructorを必要とする値
- DLL Unload後に破棄される可能性がある`shared_ptr`
- DLL所有Native GPU Resource

理由:

- DLL Unload後にvtableと関数Pointerが無効化される
- Host / DLL間のCRT差でAllocation / Destructionが不正になる
- Compiled Pipeline、Job、Frame Queueに旧世代Operationが残り得る
- GPU完了前にDLL所有Stateを破棄するとLifetimeを追跡できない

## 2.2 Builtin OperationとExtension Operation

Builtin OperationはHost Engineで定義された値型として維持する。

```cpp
using RenderOperation = std::variant<
    ClearOperation,
    RasterOperation,
    FullscreenOperation,
    ComputeOperation,
    CopyOperation,
    ResolveOperation,
    GenerateMipsOperation,
    ExternalOperation,
    ExtensionOperation
>;
```

DLL拡張Operationは、Host所有の`ExtensionOperation` Descriptorとして表現する。

```cpp
struct RenderOperationTypeKey
{
    std::string name;
    uint32_t typeVersion = 1;
};

struct ExtensionOperation
{
    RenderOperationTypeKey type;

    HostOwnedByteBuffer payload;
    RenderOperationResourceAccessSet resources;
    RenderOperationExecutionPolicy executionPolicy;

    uint64_t registryRevision = 0;
};
```

永続化対象:

- `type.name`
- `typeVersion`
- Serializable Parameter / Payload
- Resource Access宣言

永続化しないもの:

- DLL Module Handle
- Function Pointer
- Runtime Registry Revision
- DLL Object Address
- Native Context / Native Resource

## 2.3 推奨拡張方式: Lowering

DLL Operationは、可能な限りBuiltin OperationへLoweringする。

```text
DLL Extension Operation Descriptor
    ↓ Validate / Lower
Builtin RenderOperation[]
    ↓
Step 16-F RenderGraph
    ↓
RHI Command
```

```cpp
struct RenderOperationLoweringApi
{
    uint32_t abiVersion;

    bool (*Validate)(
        const HostOperationInput*,
        HostDiagnosticWriter*);

    bool (*Lower)(
        const HostOperationInput*,
        HostOperationBuilder*);
};
```

Lowering完了後のBuiltin Operation列はHost所有となる。

そのため、GPU Execute時には拡張DLLを必要としない。

これを標準経路とする。

## 2.4 Native Execute Extension

Effekseerや特殊Middlewareなど、Builtin OperationへLoweringできない場合だけ、`ExternalOperation`相当のNative Execute Extensionを許可する。

必須条件:

- Read / Write Resource完全宣言
- Required / Final State宣言
- Viewport / Scissor影響宣言
- Pipeline / IA / OM State影響宣言
- MainThread制約
- Preview Policy
- Module Lease取得
- Command Recording終了までDLLをUnloadしない
- GPU ResourceはHost RHI Handleで所有する
- DLL CallbackをGPU完了後のDestructorとして保持しない

Native Execute Callbackが終了し、RHI Commandへ必要情報がコピーされた時点でModule Leaseを解放できる。

DLL内コードがGPU完了時に再度呼ばれる構造は禁止する。

---

# 3. Operation Registry

## 3.1 Registry Entry

```cpp
struct RenderOperationRegistration
{
    RenderOperationTypeKey type;

    uint32_t abiVersion;
    RenderModuleId moduleId;
    uint64_t moduleGeneration;

    RenderOperationCapability capabilities;
    RenderOperationCallbackTable callbacks;
};
```

Registryは世代付きSnapshotとして公開する。

```cpp
struct RenderOperationRegistrySnapshot
{
    uint64_t revision;
    std::shared_ptr<const RegistryTable> table;
};
```

Compile、Prepare、Loweringは処理開始時にSnapshotを取得し、途中でRegistry内容が切り替わっても同一Snapshotを使い続ける。

## 3.2 ABI境界

DLL境界にはC ABIのFunction Tableを使用する。

```cpp
extern "C"
bool RegisterRenderOperations(
    const RenderHostApi* host,
    RenderOperationRegistrationWriter* writer);
```

境界を跨いでC++ STL Objectやvirtual interfaceを渡さない。

Host APIには次を含める。

- Host Allocator
- Diagnostic Writer
- Parameter Reader
- Operation Builder
- Resource Access Builder
- State Serialization Writer / Reader

MemoryはHost側Allocatorで確保し、Host側で解放する。

## 3.3 Module Lease

旧DLLを参照する処理にはLeaseを持たせる。

対象:

- Pipeline Compile
- Frame Prepare
- Operation Validation
- Operation Lowering
- Native Command Recording
- Runtime State Serialize / Destroy

```cpp
class RenderModuleLease
{
    RenderModuleId module;
    uint64_t generation;
};
```

Lease数が0になるまで該当GenerationをUnloadしない。

---

# 4. Compiled Pipelineとの関係

## 4.1 Compiled Pipelineに保持するもの

```cpp
struct CompiledExtensionOperation
{
    RenderOperationTypeKey type;
    HostOwnedByteBuffer normalizedPayload;
    RenderOperationResourceAccessSet resources;

    uint64_t compiledRegistryRevision;
    RenderModuleId sourceModule;
    uint64_t sourceModuleGeneration;
};
```

ただし、Lowering済みの場合は`sourceModule`に依存せず、Builtin Operationだけを保持する。

## 4.2 Stale判定

次の場合にCompiled PipelineをStaleとする。

- Registry Revision変更
- Operation Type Version変更
- Module Generation変更
- Node Compiler Version変更
- Asset Revision変更
- Shader Reflection Revision変更

Stale Pipelineを実行中に破棄しない。

```text
Active Compiled Pipeline
    ↓ Candidate Compile
Candidate Validation Success
    ↓ Frame Boundary Swap
New Active Pipeline
    ↓ Old Lease Zero
Old Pipeline Destroy
```

Candidate Compileに失敗した場合は旧Pipelineを維持する。

旧Pipelineを維持できない場合だけBuiltinPipelineへFallbackする。

---

# 5. Runtime State Hot Reload

Custom OperationまたはCustom NodeがRuntime Stateを持つ場合、DLL Objectをそのまま保持しない。

```cpp
struct ExtensionRuntimeStateRecord
{
    RenderOperationTypeKey type;
    HostOwnedByteBuffer serializedState;
    uint32_t stateVersion;
};
```

DLLが一時的なRuntime Objectを必要とする場合は、必ず同GenerationのCallbackで生成・破棄する。

必要Callback:

```cpp
CreateState
SerializeState
DestroyState
MigrateState
DeserializeState
```

規則:

- `DestroyState`完了前に旧DLLをUnloadしない
- Serialize結果はHost所有Bufferへ書く
- 新DLLは旧`stateVersion`からMigrationできる
- Migration失敗時はNode StateだけReset可能
- State Resetで描画継続できない場合はCandidate PipelineをRejectする
- Temporal History Texture自体はHostの`RenderPipelineInstance` / RHIが所有する
- DLL StateはHistory Handleを参照できるが所有しない

---

# 6. Hot Reload手順

Render Operation DLLのReloadは次の順序で行う。

```text
1. Build Candidate DLL
2. Shadow Copy DLL / PDB
3. Load Candidate Module
4. ABI Version検証
5. Candidate RegistryへOperation型を登録
6. Type重複、Version、Capability、Callback検証
7. 影響PipelineをCandidate Registryで再Compile
8. 旧Runtime StateをHost BufferへSerialize
9. 新GenerationへState Migration / Deserialize
10. Candidate PipelineとStateを検証
11. Frame Prepare / Compileの旧Generation新規受付を停止
12. 旧GenerationのCPU Job / Command Recording完了を待つ
13. Frame BoundaryでRegistry SnapshotとPipelineをAtomic Swap
14. 旧Runtime Objectを旧DLL CallbackでDestroy
15. Module Leaseが0になったことを確認
16. 旧DLLをUnload
```

Native Execute Extensionを使用する場合も、待つ対象はCommand Recording Callback完了までとする。

GPU完了待ちが必要になるのは、旧Generation専用RHI Resourceを破棄する場合だけである。

その場合もResource破棄はHost RHI側で行い、DLL DestructorをGPU Fence Callbackへ登録しない。

---

# 7. Reload失敗時の処理

各段階で次のRollbackを行う。

| Failure | Behavior |
|---|---|
| Build失敗 | 現在のDLL、Registry、Pipelineを維持 |
| Candidate Load失敗 | 現在状態を維持 |
| ABI不一致 | CandidateをReject |
| Type登録不正 | CandidateをReject |
| Pipeline再Compile失敗 | 旧Pipelineを維持 |
| State Migration失敗 | State Reset可能なら継続、不可ならCandidateをReject |
| Frame Swap前失敗 | 旧状態を維持 |
| Swap後の致命的不整合 | BuiltinPipelineへFallbackし、旧Moduleを診断用に保持 |

不明Operation型をAssetから削除しない。

Placeholderとして次を保持する。

- Type Name
- Type Version
- Raw Parameter Data
- Resource Slot情報
- Connection情報

Reload後に型が復帰した場合、再Compileを試みる。

---

# 8. Script DLL Hot Reloadとの統合

Step 20のScript Hot Reloadと共通化するもの:

- Shadow Copy
- Candidate Load
- ABI検証
- Module Generation
- Module Lease
- Safe Point
- Atomic Registry Swap
- Failure Rollback
- DLL / PDB Cleanup

共通化しないもの:

- Script Instance State
- RenderPipelineInstance State
- Render Operation Registry
- Script Execution Scheduler
- RenderGraph / RHI Command Recording

推奨共通基盤:

```text
HotReloadModuleService
    ├─ ScriptModuleAdapter
    └─ RenderExtensionModuleAdapter
```

RenderPipeline Graphを`Script.dll`へ直接依存させない。

Render Extension DLLとScript DLLが同一Binaryになる構成は許容できるが、RegistryとRuntime Stateの責務は分離する。

---

# 9. Capability

DLL OperationはCapabilityを宣言する。

```cpp
enum class RenderOperationCapability : uint32_t
{
    None                 = 0,
    LowerToBuiltin       = 1 << 0,
    NativeCommandRecord  = 1 << 1,
    MainThreadOnly       = 1 << 2,
    HasRuntimeState      = 1 << 3,
    SupportsPreview      = 1 << 4,
    SupportsStateReload  = 1 << 5,
    Deterministic        = 1 << 6,
    ExternalState        = 1 << 7
};
```

CompilerとEditorはCapabilityから次を判断する。

- Preview可能性
- Parallel Prepare / Lowering可否
- Hot Reload時のState Migration要否
- Native Command Recordingの有無
- Cache可否
- BuiltinPipelineでの使用可否

BuiltinPipelineでは原則としてDLL Extension Operationを使用しない。

これにより、ユーザーDLL破損時にもFallbackを保証する。

---

# 10. タスク計画への組み込み

## RPG-0: Contract Alignmentへ追加

- [ ] 拡張後`RenderResourceType`をRHI Resource / Viewへ対応付け
- [ ] Texture2DArray / Texture3D / CubeArray Capability表を作成
- [ ] DepthTexture Dimension契約を固定
- [ ] Structured / ByteAddress / Indirect Buffer契約を固定
- [ ] Host / DLL ABI Contractを定義
- [ ] Render Module Generation / Lease契約を定義
- [ ] Script Hot Reload共通化可能範囲を確定

## RPG-1: Asset / Registryへ追加

- [ ] Extension Operation Type Name / Version保存
- [ ] Unknown Operation Placeholder
- [ ] Raw Payload保持
- [ ] Render Operation Registry
- [ ] Registry Snapshot / Revision
- [ ] Module ID / Generation管理

## RPG-2: Compilerへ追加

- [ ] Resource Type拡張のSlot互換性検証
- [ ] Buffer Contract検証
- [ ] Operation Type存在確認
- [ ] ABI / Capability検証
- [ ] Registry RevisionをCompiled Cache Keyへ追加
- [ ] Reload時のAffected Pipeline列挙
- [ ] Candidate CompileとStale判定

## RPG-3: Operation Loweringへ追加

- [ ] `ExtensionOperation`
- [ ] C ABI Lowering Callback Table
- [ ] Host Operation Builder
- [ ] Host Allocator境界
- [ ] Builtin OperationへのLowering
- [ ] Native Execute Extension隔離
- [ ] Module Lease付きCommand Recording

## RPG-7: Pipeline Instanceへ追加

- [ ] Extension Runtime State Record
- [ ] State Serialize / Migrate / Deserialize
- [ ] Module Generation変更時のInstance State処理
- [ ] Temporal History HandleとDLL Stateの所有権分離

## RPG-8: Builtin / Editorへ追加

- [ ] Reload失敗理由表示
- [ ] Missing Operation Placeholder表示
- [ ] Candidate Compile診断表示
- [ ] Active / Candidate Module Generation表示
- [ ] BuiltinPipelineがDLL Extensionへ依存しないことを検証

## RPG-9: Previewへ追加

- [ ] Preview中Module Lease
- [ ] Reload時Preview Pipeline再Compile
- [ ] Preview非対応OperationのFallback / Skip
- [ ] Captured Frameと旧DLL Stateを混在させない

## RPG-10: Optimizationへ追加

- [ ] Lowering結果Cache
- [ ] Registry Revision単位のCache Invalidation
- [ ] Module Reload時間計測
- [ ] Affected Pipeline再Compile時間計測
- [ ] Old Module Lease残存診断

---

# 11. CI / Smoke Test

追加する検証:

```text
Render Resource Type Contract
Render Buffer Contract
Render Operation Extension ABI
Render Operation Registry Revision
Render Operation DLL Reload
Render Operation Reload Rollback
Render Operation State Migration
Render Operation Module Lease
Render Operation Unknown Type Preservation
```

最低ケース:

- `Texture2DArray`と`Texture2D`を誤接続するとCompile Error
- `TextureCubeArray`のLayer数不一致を検出
- `ByteAddressBuffer`のAlignment不正を検出
- `IndirectArgumentBuffer`非対応Capabilityを検出
- DLL Reload後に旧Function Pointerを呼ばない
- Compile中ReloadでもRegistry Snapshotが変化しない
- Candidate Compile失敗時に旧Pipelineを維持
- State Migration失敗時にRollback
- Native Command Recording中は旧DLLをUnloadしない
- Lease解放後に旧DLLをUnload
- Missing OperationをPlaceholderとして再保存可能
- BuiltinPipelineがExtension DLLなしでCompile / Execute可能

---

# 12. 完了条件

- [ ] 拡張後の全`RenderResourceType`がCompiler、Slot、RHI Loweringで扱える
- [ ] Depth TextureのDimension / Array契約が曖昧でない
- [ ] Buffer Type間の暗黙変換がない
- [ ] RenderOperationがDLL定義C++ Objectを保持しない
- [ ] DLL Reload後に旧vtable / Function Pointerへ到達しない
- [ ] Candidate Compile成功前にActive Pipelineを破棄しない
- [ ] Runtime StateをHost所有Buffer経由で移行できる
- [ ] Module Leaseにより実行中DLLをUnloadしない
- [ ] Reload失敗時に旧PipelineまたはBuiltinPipelineで描画を継続する
- [ ] BuiltinPipelineが外部DLLなしで常に利用できる
- [ ] Script Hot Reloadと共通基盤を再利用しつつ、RegistryとStateを混同しない
