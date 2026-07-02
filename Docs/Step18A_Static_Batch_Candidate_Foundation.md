# Step 18-A Static Batch CPU Foundation

## 目的

Static Entityを直ちに別描画経路へ移さず、既存Render Packet Merge後の安定したPacket列から次のCPUデータを段階的に構築する。

1. Static Batch Candidate
2. Scene / Resource Key単位Group
3. Static Packet Cache
4. GPU Upload向けInstance Data

この段階では既存の通常Draw Call、Render Pass、Renderable Executeを置き換えない。いずれかのStatic Batch Stageが失敗またはOverflowしても、通常Render Packet提出を必ず維持する。

## CPU Pipeline

```text
Worker-local Packet Build
    -> RenderPacket Merge
    -> Published Packet Stable Sort
    -> Static Batch Candidate Build
    -> Candidate Stable Sort / Group Build
    -> Static Packet Cache Synchronize
    -> Static Instance Data Synchronize
    -> ordinary RenderPacket Submit
```

## Resource Key

Candidateは次の永続的な値を保持する。

```cpp
struct StaticBatchCandidateKey {
    RenderPacketKind kind;
    RenderLayer layer;
    uint32_t materialKey;
    uint64_t pipelineKey;
    uint64_t geometryKey;
    uint64_t textureSetKey;
    uint64_t materialStateKey;
};
```

PointerやNative API ObjectはKeyへ含めない。

現在の生成元:

- Pipeline: Kind / Layer / Pass Mask / Material Key
- Geometry: Mesh geometryResourceKey、またはModel Resource metadata
- Texture Set: Texture pathまたはModel texture name set
- Material State: Shader ID / MATERIAL値 / Model material metadata

全Keyが非0のGroupだけをCache Readyとして扱う。

## Candidate / Group

Candidateは世代付きEntity、同一FrameのPublished Packet index、Stable Sequenceを保持する。

GroupはCandidate配列のRangeだけを保持し、同一Scene / Resource Key単位で構築する。同じKeyでもSceneを跨いでGroup化しない。

Sort順:

1. Packet Kind
2. Render Layer
3. Material Key
4. Pipeline Key
5. Geometry Key
6. Texture Set Key
7. Material State Key
8. Scene Context ID
9. Stable Sequence

## Static Packet Cache

Cache Ready Groupから次を複製し、Frameを跨いでSource Revisionが変化しない限り再構築を省略する。

- Group Entry
- Entity配列
- Transform Snapshot配列
- Generation
- Source Revision

Source RevisionにはResource Key、Entity generation、World Matrixを含める。Candidate Overflow、Range不整合、Growth禁止時の容量不足ではCache全体を無効化し、部分Cacheを公開しない。

## Static Instance Data

Static Packet CacheからGPU Upload向けの連続配列を生成する。

```cpp
struct StaticBatchInstanceData {
    float worldMatrix[16];
    uint32_t entityIndex;
    uint32_t entityGeneration;
    uint32_t sceneContextID;
};
```

Entity generationとScene Context IDを保持し、将来のObject ID / Pickingとstale Entity検証に使用する。

Cache Source Revisionが同じ場合は再構築を省略し、Generationだけを更新する。

## RHI Instanced Draw契約

Static Batch提出の前提として、Backend非依存Command Listへ次の契約を追加した。

```cpp
bool DrawIndexedInstanced(
    uint32_t indexCountPerInstance,
    uint32_t instanceCount,
    uint32_t firstIndex = 0,
    int32_t vertexOffset = 0,
    uint32_t firstInstance = 0
);
```

未対応Backendは既定実装で`false`を返す。複数回の通常Drawへ暗黙変換しないため、Instance IDやInstance Bufferの意味を壊さない。

D3D11 BackendはGraphics Command ListのRecording中かつ、Index CountとInstance Countが非0の場合に`ID3D11DeviceContext::DrawIndexedInstanced`を実行して`true`を返す。Compute / Copy Queue、未記録状態、不正なCountでは`false`を返す。

この契約とBackend実装は完了したが、Static Batch Groupからの実際の提出接続はまだ行わない。

## Scene Storage接続

`SceneStorageConfig::staticBatchReserve`をSceneごとに一度だけ合算し、次の全CPU Storageへ適用する。

- Candidate
- Group
- Static Packet Cache Entry
- Static Packet Cache Entity / Transform
- Static Instance Group
- Static Instance Data

明示ReserveはRuntime Growth Eventへ計上しない。

## Growth禁止時の契約

Static Batchは最適化なので、`allowRuntimeGrowth=false`でどこかのCPU Stageが容量不足になった場合は、そのStageの部分データを破棄して無効化する。

```text
Static Batch Stage Capacity不足
    -> partial data clear
    -> overflowed = true
    -> downstream Static Stage invalidate
    -> ordinary RenderPacket submission continues
```

Render Packet自体は欠落させない。

## Telemetry

Scene Storage Runtime Telemetryへ次を公開する。

- Candidate / Group Current / Peak / Capacity
- Cache Ready Group Current / Peak
- Packet Cache Entry / Instance Current / Peak / Capacity
- Static Instance Group / Data Current / Peak / Capacity
- Rebuild Count
- Growth Event Count
- Overflow Event Count
- Skipped Incomplete Group Count
- Valid / Overflowed

## 検証

専用Workflow:

- `.github/workflows/static-batch-candidate.yml`
- `.github/workflows/rhi-smoke.yml`

Smoke Test:

- `StaticBatchCandidateStorageSmokeTest.cpp`
- `StaticBatchResourceKeySmokeTest.cpp`
- `StaticBatchInstanceDataBufferSmokeTest.cpp`
- `StaticBatchFrameBufferIntegrationSmokeTest.cpp`
- `RHISmokeTest.cpp`
- `D3D11RHIBackendSmokeTest.cpp`

統合Testでは、FrameBuffer MergeからCandidate、Cache、Instance Dataまでが接続されることと、Growth禁止時にも通常Packetが全件残ることを確認する。RHI Testでは未対応Backendが`false`を返すことと、D3D11 Command ListがInstanced Draw契約をOverrideすることを確認する。

## 完了済み

- [x] Static Model / Mesh Candidate抽出
- [x] 決定的Candidate Sort
- [x] Scene / Resource Key単位Group生成
- [x] Pipeline / Texture / Mesh / Material Resource Key
- [x] Static Packet Cache
- [x] Source Revision / Rebuild省略
- [x] Static Instance Data Build
- [x] Scene設定Reserveの全CPU Stage接続
- [x] Candidate / Cache / Instance Telemetry
- [x] Growth禁止時の通常Packet Fallback
- [x] Runtime Telemetry UI接続
- [x] FrameBuffer統合Smoke Test / Workflow
- [x] Backend非依存`DrawIndexedInstanced`契約
- [x] D3D11 Native `DrawIndexedInstanced`実装
- [x] Null / D3D11 RHI契約Test

## 未完了

- [ ] Static Instance Buildを専用AnyWorker Taskへ分離
- [ ] GPU Instance Buffer Upload
- [ ] Static Batch Groupから`DrawIndexedInstanced`を実際に提出
- [ ] Dynamic Packetとの同一Pass提出
- [ ] Picking / Object ID出力
- [ ] Shadow / GBuffer提出
- [ ] Spatial Cell単位Batch AABB / Culling
- [ ] Transform / Material / Texture変更通知による明示Dirty化
- [ ] Static Batch専用CPU / GPU計測

## 次工程

1. Instance DataをFrame-local Buffer所有からRenderWorld側Storageへ移す
2. Instance BuildをAnyWorker Taskへ分離する
3. RHI Bufferを使ったMainThread Upload Taskを追加する
4. GBuffer / ShadowでStatic Batch Groupを`DrawIndexedInstanced`提出する
5. Object ID / PickingとBatch Cullingを接続する
