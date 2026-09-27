// =======================================================================
//
// modelRendererComponent.h
//
// =======================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Interface/IComponent.h"
#include "Resources/Data/modelData.h"

// モデル設定とEntity固有AnimationのCPU状態を保持するComponent。
// 動的Vertex BufferなどのGPU RuntimeはRenderSystem側Storageが所有する。
class ModelRendererComponent: public IComponent {
public:
	std::shared_ptr<ModelData> model;
	std::string modelFilePath;

	// Entity固有のSubMesh表示状態とMaterial割当。
	// Materialパラメータ自体はModelDataまたはMaterialComponentが所有する。
	std::vector<ModelSubMeshRenderState> subMeshes;

	std::vector<std::pair<std::string, std::string>> animations;
	std::vector<AnimationBlend> blendedAnimations;
	bool isBlender = false;
	float animationTime = 0.0f;

	// Worker Threadで計算し、Main ThreadのGPU Uploadで消費するEntity固有状態。
	std::vector<BONE> evaluatedBones;
	std::vector<std::vector<VERTEX_3D>> cpuSkinnedVertices;
	uint64_t animationPoseRevision = 0;

	// Modelの再生成・Reloadを跨いだ古いPose Uploadを拒否する世代。
	// GPU Runtime StorageもこのRevisionで古いBufferを置換する。
	uint64_t modelRuntimeRevision = 1;
	uint64_t animationPoseSourceModelRevision = 0;

	// Pose計算時のBlend設定とAnimation Timeを識別するSignature。
	// Upload前に現在値と再比較し、Clip切替後の古いPoseを拒否する。
	uint64_t animationPoseSourceInputRevision = 0;

	// Main Thread Uploadの連続失敗数。成功またはModel Resetで0へ戻す。
	uint32_t animationUploadFailureCount = 0;

	bool animationPoseReady = false;
	bool cpuSkinningReady = false;

	ModelRendererComponent() = default;
	~ModelRendererComponent() override;

	YAML::Node encode() override;
	bool decode(SceneContext* context, const YAML::Node& node) override;
	void inspector(SceneContext* context) override;

	void CreateModel(SceneContext* context);
	void ReleaseBuffers();
	void ResetAnimationRuntime();

	const ModelSubMeshRenderState* FindSubMeshState(
		ModelSubMeshID id
	) const noexcept {
		if(id == InvalidModelSubMeshID){
			return nullptr;
		}
		for(const ModelSubMeshRenderState& state : subMeshes){
			if(state.subMeshID == id){
				return &state;
			}
		}
		return nullptr;
	}

	ModelSubMeshRenderState* FindSubMeshState(
		ModelSubMeshID id
	) noexcept {
		return const_cast<ModelSubMeshRenderState*>(
			static_cast<const ModelRendererComponent*>(this)->FindSubMeshState(id)
		);
	}
};

#include "Operations/ModelRendererRuntime.h"
#include "Operations/ModelRendererSerialization.h"
#include "Operations/ModelRendererInspector.h"
