# Step 17-C Animation CPU Build / GPU Upload分離設計

## 目的

現在MainThread上で連続実行しているAnimation Pose計算とD3D11 Uploadを、次の3段階へ分離する。

```text
RenderSystem.Animation.Prepare
        ↓
RenderSystem.Animation.BuildPose
        ↓
RenderSystem.Animation.Upload
```

- `Prepare`: ECS ComponentからFrame-local入力Snapshotを構築する
- `BuildPose`: Worker上でAnimation補間、Bone Pose、Skinning Matrixを生成する
- `Upload`: MainThread上でGPU Resourceへ反映する

Graphics API操作はMainThreadへ残し、純粋CPU計算だけをWorkerへ移す。

---

# 1. 現在の実装状態

現在の`RenderSystem::EditorUpdate`では、各`ModelRendererComponent`について次を一つのMainThread処理で行っている。

```text
ModelRendererComponent検索
    ↓
ModelData::UpdateBoneAnimation
    ↓
GPU Skinningの場合
    ModelData::UpdateAndDispatchSkinning

CPU Fallbackの場合
    ModelData::CPU_Skinning
    ID3D11DeviceContext::Map
    memcpy相当の頂点書き込み
    Unmap
```

現状の主な問題は以下。

- Animation補間とBone行列生成がMainThreadに残っている
- GPU Skinning DispatchとCPU Pose計算の責務が同一関数に混在する
- CPU FallbackではCPU計算と`Map / Unmap`が同じLoop内にある
- `ModelRendererComponent`が`ID3D11Buffer*`を直接保持している
- `ModelData`がResource、Animation入力、可変Bone Pose、GPU Resourceを同時に所有している

---

# 2. 先に解消すべき共有ModelData問題

`ModelRendererComponent::model`は`std::shared_ptr<ModelData>`である。

一方、現在の`ModelData::UpdateBoneAnimation`は次の可変状態を書き換える。

```text
ModelData::m_Bones[].AnimationMatrix
ModelData::m_Bones[].WorldMatrix
ModelData::m_Bones[].Matrix
```

このため、同じ`ModelData`を複数Entityが共有しながら、Entityごとに異なるAnimation時間やBlendを持つ構造は安全ではない。

```text
Entity A ─┐
          ├─ shared ModelData ─ mutable m_Bones
Entity B ─┘
```

並列化するとData Raceになり、直列でも後から計算したEntityのPoseが共有Resourceへ上書きされる。

したがって、既存の`UpdateBoneAnimation`をそのままWorkerへ移すことは禁止する。

---

# 3. 目標所有権

## 3.1 ModelResource

Model Assetとして共有可能な不変データだけを持つ。

```text
ModelResource
    Mesh topology
    Static vertex / index data
    Skeleton definition
    Bone name -> index
    Bind pose / offset matrix
    Animation clip data
    Texture / Material resource reference
```

Frame中の可変PoseやEntity固有Runtime Bufferは持たない。

## 3.2 AnimationInstanceState

EntityごとのAnimation状態。

```cpp
struct AnimationInstanceState
{
    float animationTime = 0.0f;
    uint64_t revision = 0;
    std::vector<AnimationBlend> blends;
};
```

最終的には`ModelRendererComponent`から分離可能だが、Step 17-C初期段階では既存フィールドをSnapshot元として利用する。

## 3.3 ModelRendererRuntime

EntityごとのGPU Runtime Resourceを所有する。

```text
ModelRendererRuntime
    Dynamic Vertex Buffers
    Bone Constant Buffer
    Skin Input / Output Buffer
    Skin SRV / UAV
    Runtime generation
```

`ModelRendererComponent`はNative API Pointerを直接所有しない。

## 3.4 AnimationPoseResult

Workerが生成するFrame-local CPU結果。

```cpp
struct AnimationPoseResult
{
    uint32_t sceneContextID = 0;
    Entity entity{};
    uint64_t inputRevision = 0;
    uint64_t frameGeneration = 0;

    std::vector<Matrix4x4> localPose;
    std::vector<Matrix4x4> worldPose;
    std::vector<Matrix4x4> skinningMatrices;

    // CPU fallback時のみ使用
    std::vector<std::vector<VERTEX_3D>> deformedVertices;
};
```

GPU ObjectやComponent Pointerを所有しない。

---

# 4. Task契約

## 4.1 RenderSystem.Animation.Prepare

### Domain / Thread

```text
Domain: FrameまたはEditor
Affinity: MainThreadまたはAnyWorker
```

Component Registryを読むだけであればAnyWorkerへ置けるが、初期実装はSnapshot整合性を優先し、既存Scheduler Access契約に合わせる。

### 入力

- `ModelRendererComponent`
- Entity generation
- SceneContext ID
- Animation time
- Blend列
- Model Resource Handle
- Model / Runtime generation

### 出力

```cpp
struct AnimationBuildInput
{
    uint32_t sceneContextID = 0;
    Entity entity{};
    uint64_t inputRevision = 0;
    uint64_t frameGeneration = 0;

    std::shared_ptr<const ModelResource> model;
    float animationTime = 0.0f;
    std::vector<AnimationBlend> blends;
    bool useGpuSkinning = false;
};
```

### 禁止事項

- D3D11 Contextへのアクセス
- `ModelData::m_Bones`の書き換え
- Dynamic Vertex BufferのMap
- Runtime GPU Resource生成

---

## 4.2 RenderSystem.Animation.BuildPose

### Domain / Thread

```text
Domain: FrameまたはEditor
Affinity: AnyWorker
```

### 処理

- Animation Clip検索
- Keyframe区間探索
- Translation補間
- Rotation Slerp
- Scale補間
- Local Pose生成
- Skeleton階層からWorld Pose生成
- Offset Matrix適用
- Skinning Matrix生成
- CPU fallback用頂点変形

### 出力

`AnimationPoseResult`をWorker-local Bufferへ追加する。

### 並列単位

初期実装はEntity列を2～4 Rangeへ分割する。

```text
BuildPose.Range0
BuildPose.Range1
BuildPose.Range2
BuildPose.Range3
```

Entity数が少ない場合は1 Taskとする。

### 禁止事項

- D3D11 API
- Component Registry書き込み
- Shared Model Resourceの可変状態更新
- 他EntityのPose Result参照

---

## 4.3 RenderSystem.Animation.Upload

### Domain / Thread

```text
Domain: RenderまたはEditor
Affinity: MainThread
```

### 検証

Upload前に次を確認する。

- SceneContext IDが現在も有効
- Entity generationが一致
- ModelRenderer Runtime generationが一致
- Input revisionが最新
- Frame generationが現在のUpload対象

不一致結果は破棄する。

### GPU Skinning

- Bone Matrix Constant / Structured Buffer更新
- Skinning Compute Dispatch
- Output Vertex Buffer反映

### CPU Fallback

- Dynamic Vertex Buffer Map
- Pose Resultから頂点列をコピー
- Unmap

### 禁止事項

- Animation Keyframe再計算
- Shared Model ResourceのPose更新
- 古いFrame ResultのUpload

---

# 5. Frame-local Buffer

```cpp
class AnimationFrameBuffer
{
public:
    void BeginFrame(uint64_t generation);
    void SetInputs(std::vector<AnimationBuildInput> inputs);
    void MergeResults(std::span<const AnimationWorkerResultBuffer> workers);

    uint64_t Generation() const;
    std::span<const AnimationBuildInput> Inputs() const;
    std::span<const AnimationPoseResult> Results() const;
};
```

Render Packet Bufferと同様に、Frame generationを持つ。

```text
Prepare Write
    ↓
BuildPose Read Input / Write Worker Result
    ↓
Merge Write Frame Result
    ↓
Upload Read Frame Result
```

---

# 6. Revision契約

EntityごとのAnimation入力変更時にRevisionを増加させる。

対象:

- Model変更
- Animation Clip追加・削除
- Blend Weight変更
- Loop設定変更
- Animation Start Time変更
- Skeleton / Model Reload

単純な`animationTime`進行はFrame Snapshotへ含めるが、構造変更Revisionとは分けてもよい。

```cpp
struct AnimationInputVersion
{
    uint64_t structureRevision = 0;
    uint64_t modelGeneration = 0;
    uint64_t runtimeGeneration = 0;
};
```

---

# 7. Editor Preview契約

Stopped状態でもEditor Previewを維持する。

```text
Editor Animation.Prepare
    ↓
Editor Animation.BuildPose
    ↓
Editor Animation.Upload
```

RuntimeとEditorで同じFrame Bufferを同時利用せず、DomainまたはGenerationを分離する。

`deltaTime == 0`でもInspectorからCurrent Timeを変更した場合はDirty Revisionにより再Buildする。

---

# 8. 段階実装

## Phase 1: 所有権分離の準備

- [ ] `AnimationBuildInput`を追加
- [ ] `AnimationPoseResult`を追加
- [ ] `AnimationFrameBuffer`を追加
- [ ] ModelDataのImmutable / Mutableフィールド一覧を確定
- [ ] ModelData共有時のInstance Pose上書きをRegression Test化
- [ ] ModelRenderer Runtime Handle設計

## Phase 2: Pose計算の純粋関数化

- [ ] `UpdateBoneAnimation`からKeyframe補間を分離
- [ ] Shared ModelDataへ書き込まないAPIを追加
- [ ] Skeleton Definition + Clip + Blend + TimeからPose Resultを返す
- [ ] 単一Entityで旧結果と行列比較
- [ ] Root Motion出力をResultへ分離

## Phase 3: Frame-local Build

- [ ] `RenderSystem.Animation.Prepare` Task
- [ ] `RenderSystem.Animation.BuildPose` Task
- [ ] Worker-local Result Buffer
- [ ] 決定的Merge
- [ ] 2～4 Range分割
- [ ] Entity数に応じたGrain Size

## Phase 4: GPU Upload

- [ ] `RenderSystem.Animation.Upload`をMainThread登録
- [ ] GPU Skinning Buffer更新をUploadへ移動
- [ ] CPU fallback Map / UnmapをUploadへ移動
- [ ] Runtime generation検証
- [ ] 古いPose Result破棄

## Phase 5: Native Resource所有権移動

- [ ] `ModelRendererComponent::dynamicVertexBuffers`を削除
- [ ] ModelDataからInstance固有GPU Bufferを削除
- [ ] `ModelRendererRuntime`へ移動
- [ ] Component / Resource / RuntimeのShutdown順序確認
- [ ] Stop → Play → Stop回帰

---

# 9. 初期実装で行わないこと

- Animation System全体のSoA化
- GPU Animation Sampling
- Animation Compression刷新
- Jobごとの細粒度Bone分割
- Frameを跨ぐPose Pipeline
- 1Frame遅延を利用した二重Buffer
- D3D12 / Vulkan専用最適化

まずEntity単位のCPU BuildとMainThread Upload分離を完成させる。

---

# 10. 検証

## Compile / Smoke

- [ ] Engine Debug x64
- [ ] Engine Release x64
- [ ] Script Debug x64
- [ ] Script Release x64
- [ ] 同一Modelを共有する2 Entityで異なるAnimation時間を保持
- [ ] Pose Result generation不一致拒否
- [ ] Entity破棄後Result拒否
- [ ] Model Reload後Result拒否

## Runtime

- [ ] GPU Skinning表示
- [ ] CPU Fallback表示
- [ ] Blend Animation表示
- [ ] Root Motion表示
- [ ] Editor停止中Preview
- [ ] Play / Pause / Stop回帰
- [ ] Scene TempLoad回帰

## Schedule Capture

- [ ] `Animation.Prepare`がCPU Build前にある
- [ ] `Animation.BuildPose`がAnyWorkerで実行される
- [ ] `Animation.Upload`がMainThreadで実行される
- [ ] Buildと独立Taskが並列化される
- [ ] UploadがRender Submit前に完了する

---

# 11. 完了条件

- Animation補間とPose生成がD3D11 APIへ依存しない
- 同一Model Resourceを複数Entityが安全に共有できる
- EntityごとのPoseがShared ModelDataを上書きしない
- GPU Resource更新はMainThreadだけで行われる
- CPU fallbackのMap / UnmapがMainThreadへ限定される
- 古いEntity / Revision / RuntimeへのUploadを拒否できる
- Editor PreviewとRuntime Animationに回帰がない
- Schedule CaptureでBuild / Upload分離を確認できる
