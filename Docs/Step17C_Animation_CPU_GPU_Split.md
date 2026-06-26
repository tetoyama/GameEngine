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
- Pose Ready Flag
- CPU Skinning Ready Flag

### Worker Stage

D3D11 APIを触らない。

1. Animation Blendを評価
2. Entity固有Bone Poseを生成
3. Bone数がGPU上限を超える場合はCPU Skinning結果をStagingへ生成
4. Pose RevisionとReady Flagを更新

### Main Thread Stage

D3D11 APIだけを扱う。

1. GPU Skinning対象はBone Constant Bufferを更新
2. Compute ShaderをDispatch
3. Output BufferをEntity固有Dynamic Vertex BufferへCopy
4. CPU Skinning対象はStaging VertexをMap / memcpy / Unmap

## 実装済み基盤

- [x] `ModelRendererComponent`へEntity固有Animation Runtime状態を追加
- [x] Model Reset / Destruction時にAnimation Runtime状態を破棄
- [x] `AnimationPoseEvaluator`を追加
- [x] 共有`ModelData`を変更せずBone Poseを出力する契約を追加
- [x] Bone上限超過用CPU Skinning Staging生成を追加
- [x] `AnimationSkinningUpload`を追加
- [x] GPU Dispatch / CopyとCPU Staging UploadをMain Thread用APIへ分離
- [x] Windows Build対象Translation UnitからHelper HeaderをCompile Check

## 未接続

- [ ] `RenderSystem.Animation.Pose.Calculate` Worker Taskを登録
- [ ] 従来の`ModelData::UpdateBoneAnimation`呼び出しを新Evaluatorへ置換
- [ ] 従来のMain Thread内CPU SkinningをStaging Uploadへ置換
- [ ] `RenderSystem.Animation.Upload`をD3D11操作だけに限定
- [ ] 同一Modelを共有する複数Entityの別Animation回帰
- [ ] GPU Skinning / CPU Skinningの見た目一致確認
- [ ] Animation Clip追加とWorker評価の排他契約

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
- Write: D3D11 Buffer

UploadはPose Calculateより後に実行し、Render Packet Buildより前に完了させる。

## 完了条件

- 同じModel Resourceを使うEntity同士でBone Poseを共有しない
- Worker StageがD3D11 APIへ触れない
- Main Thread StageでAnimation補間やCPU Skinning計算を行わない
- GPU / CPU両Skinning経路がEntity固有Dynamic Vertex Bufferを更新する
- Scene Stop / Model Reload時にStagingとPoseを安全に破棄する
