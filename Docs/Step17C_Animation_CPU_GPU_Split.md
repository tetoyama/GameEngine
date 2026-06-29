# Step 17-C Animation CPU / GPU Split

## 目的

Animationの姿勢評価とCPU SkinningをWorker Threadへ移し、D3D11 APIを触るUpload / Dispatch / CopyだけをMain Threadへ残す。

## 調査で判明した問題

`ResourceLoader<ModelData>`は同じResource Keyに対して同じ`shared_ptr<ModelData>`を返す。

一方、従来のAnimation更新はEntityごとに次の共有領域を書き換えていた。

- `ModelData::m_Bones`
- Bone Constant Buffer
- Compute Skinning Output Buffer

GPU ResourceはMain Threadで直列利用できるが、`m_Bones`を共有Resource側へ保存すると、同じModelを使う複数Entityが別Animationを再生した場合に姿勢を上書きし合う。

したがって、単純に従来処理をWorker Threadへ移すことはできない。

## 確定方針

### 共有ModelData

読み取り専用Resourceとして扱う。

- Assimp Scene
- Bone Name / Index Map
- Bind Pose / Offset Matrix
- Animation Clip
- Static Vertex / Weight Data
- GPU Skinning共通Resource

### ModelRendererComponent

Entity固有の可変状態を保持する。

- Evaluated Bone Pose
- CPU Skinned Vertex Staging
- Pose Revision
- Model Runtime Revision
- Pose Source Model Revision
- Pose Source Animation Input Revision
- Pose Ready Flag
- CPU Skinning Ready Flag

### Worker Stage

D3D11 APIを触らない。

1. Blend配列とAnimation TimeをEntity固有Snapshotへコピー
2. Snapshot Signatureを計算
3. Animation Blendを評価
4. Entity固有Bone Poseを生成
5. Bone数がGPU上限を超える場合はCPU Skinning結果をStagingへ生成
6. 評価終了時に現在入力とSnapshot Signatureを再照合
7. Pose / Model / Input RevisionとReady Flagを更新

### Main Thread Stage

D3D11 APIとResource生成だけを扱う。

1. 未生成ModelとEntity固有Dynamic Vertex Bufferを生成
2. Pose Source Model Revisionと現在のModel Runtime Revisionを照合
3. Pose Source Input Revisionと現在のBlend / Time Signatureを照合
4. GPU Skinning対象はBone Constant Bufferを更新
5. Compute ShaderをDispatch
6. Output BufferをEntity固有Dynamic Vertex BufferへCopy
7. CPU Skinning対象はWorkerが生成したStaging VertexをMap / memcpy / Unmap

ModelまたはAnimation入力Revisionが一致しない場合、古いBone Pose / CPU Stagingを破棄し、現在のBufferへUploadしない。

## Animation Input Revision

`AnimationInputRevision`は次を順序込みでHashする。

- Blend数
- Clip名
- Weight
- Animation Start Time
- Loop Flag
- Animation Time

Signatureは0を未設定値として使うため、計算結果が0の場合は1へ補正する。

```text
Pose Calculate
    -> Blend / Time Snapshot
    -> Input Signatureを保存

Upload
    -> 現在のBlend / Time Signatureを再計算
    -> 不一致ならPose / CPU Stagingを全破棄
    -> 一致時だけGPU / CPU BufferへUpload
```

Scheduler上ではPose CalculateとUploadの両方が`ModelRendererComponent`をWrite宣言するため、Task間は直列化される。SignatureはTask間で入力が更新された場合の古い結果利用を防ぐ世代契約であり、未宣言の同時書換えを許可する仕組みではない。

## 実装済み基盤

- [x] `ModelRendererComponent`へEntity固有Animation Runtime状態を追加
- [x] Model Reset / Destruction時にAnimation Runtime状態を破棄
- [x] `AnimationPoseEvaluator`を追加
- [x] 共有`ModelData`を変更せずBone Poseを出力する契約を追加
- [x] Bone上限超過用CPU Skinning Staging生成を追加
- [x] `AnimationSkinningUpload`を追加
- [x] GPU Dispatch / CopyとCPU Staging UploadをMain Thread用APIへ分離
- [x] Windows Build対象Translation UnitからHelper HeaderをCompile Check
- [x] `RenderSystem.Animation.Pose.Calculate`をEditor / Earliest / AnyWorkerとして接続
- [x] `RenderSystem.Animation.Upload`をEditor / Early / MainThreadとして接続
- [x] 従来のSchedule実行経路から`ModelData::UpdateBoneAnimation`とMain Thread内CPU Skinningを除外
- [x] Model生成とD3D11 Buffer生成をMain Thread Stageへ限定
- [x] 同一共有ModelDataからEntity固有Poseを分離して生成するSmoke Test
- [x] Model Runtime RevisionによるReload前PoseのUpload拒否
- [x] Animation Input RevisionによるClip選択 / Blend / Time変更前PoseのUpload拒否
- [x] Reset / Create / Destruction時のPose Source Revision無効化

## 現在の接続構造

`RenderSystem`の既存Task登録を段階移行するため、Schedule Compile前の互換Hookで旧Animation Upload Taskを次の二段Taskへ置換している。

```text
RenderSystem.Animation.Pose.Calculate
    Editor / Earliest / AnyWorker
    Write: ModelRendererComponent
    Read : SceneManager, ModelData

RenderSystem.Animation.Upload
    Editor / Early / MainThread
    Write: ModelRendererComponent, ResourceService, ModelData, GraphicsContext
    Read : SceneManager
```

両Taskは`ModelRendererComponent`へのWrite競合によってPose CalculateからUploadへ依存する。

SceneManagerの呼び出し順は次のため、UploadはRender Packet Buildより前に完了する。

```text
Frame Domain
Editor Domain
Render Domain
```

互換Hookは大規模な`renderSystem.cpp`を安全に段階移行するための一時的なAdapterであり、最終形では`RenderSystem::RegisterTasks`へ直接登録して撤去する。

## 未完了

- [ ] `MigrateRegisteredTasks`互換Hookを撤去し、二段Taskを`RegisterTasks`へ直接定義
- [ ] 同一Modelを共有する複数Entityの別AnimationをPlayer / Editor Viewで実機回帰
- [ ] GPU Skinning / CPU Skinningの見た目一致確認
- [ ] Inspectorから共有ModelDataへClipをImportする処理のMain Thread専用Command化
- [ ] Upload失敗時の再試行回数・診断ログ契約

## Scheduler Access契約

### Animation.Pose.Calculate

- Domain: Editor
- Phase: Earliest
- Affinity: AnyWorker
- Read: Shared Model Resource
- Write: `ModelRendererComponent` Animation Runtime領域

### Animation.Upload

- Domain: Editor
- Phase: Early
- Affinity: MainThread
- Read: Evaluated Pose / CPU Vertex Staging
- Write: D3D11 Buffer / Model生成Resource

UploadはPose Calculateより後に実行し、Render Packet Buildより前に完了させる。

## 検証

### 自動検証

- [x] Debug x64 Windows Build
- [x] Release x64 Windows Build
- [x] Script Debug / Release x64 Build
- [x] 共有ModelDataのBone状態をEvaluatorが変更しない
- [x] 同じModelから異なるAnimation Poseを独立出力できる
- [x] 一方のPose出力変更が他Entity相当のPoseへ伝播しない
- [x] Animation Pose Isolation Smoke Test
- [x] Clip名 / Weight / Start Time / Loop / Animation TimeのSignature分離
- [x] Signatureの決定性と非ゼロ契約

### 実機検証

- [ ] GPU Skinning ModelのAnimation再生
- [ ] CPU Skinning fallback ModelのAnimation再生
- [ ] 同一Model・異なるAnimationの複数Entity同時再生
- [ ] Scene Stop → TempLoad → Play再開
- [ ] Model Reload / Animation Clip追加削除

## 完了条件

- 同じModel Resourceを使うEntity同士でBone Poseを共有しない
- Worker StageがD3D11 APIへ触れない
- Main Thread StageでAnimation補間やCPU Skinning計算を行わない
- GPU / CPU両Skinning経路がEntity固有Dynamic Vertex Bufferを更新する
- Scene Stop / Model Reload時にStagingとPoseを安全に破棄する
- Blend / Time変更後の古いPoseをUploadしない
- Taskが`RenderSystem::RegisterTasks`へ直接定義され、一時互換Hookが残っていない
