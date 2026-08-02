#pragma once

#include <cstdint>

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Resources/resourceService.h"
#include "Resources/Data/modelData.h"
#include "System/Render/Animation/AnimationPoseEvaluator.h"
#include "ModelSubMeshStateSynchronization.h"

namespace ModelRendererRuntime {

inline void ResetAnimationRuntime(ModelRendererComponent& component){
	component.evaluatedBones.clear();
	component.cpuSkinnedVertices.clear();
	component.animationPoseRevision = 0;
	component.animationPoseSourceModelRevision = 0;
	component.animationPoseSourceInputRevision = 0;
	component.animationUploadFailureCount = 0;
	component.animationPoseReady = false;
	component.cpuSkinningReady = false;

	// 0は未設定値として使うため、周回時も有効世代へ進める。
	// RenderSystem側GPU Runtimeはこの世代差で古いBufferを置換する。
	++component.modelRuntimeRevision;
	if(component.modelRuntimeRevision == 0){
		++component.modelRuntimeRevision;
	}
}

inline void ReleaseBuffers(ModelRendererComponent& component){
	// Step 18-A以降、GPU BufferはComponentではなくRenderSystemが所有する。
	// 互換APIとして残すが、実際の無効化はmodelRuntimeRevision更新と
	// RenderWorld/RenderSystem Resetで行う。
	(void)component;
}

inline bool CreateModel(ModelRendererComponent& component, SceneContext* context){
	if(component.modelFilePath.empty() || !context || !context->manager ||
		!context->manager->resource){
		component.subMeshes.clear();
		return false;
	}

	ReleaseBuffers(component);
	ResetAnimationRuntime(component);
	component.model.reset();
	component.model = context->manager->resource->Load<ModelData>(
		component.modelFilePath,
		component.isBlender
	);
	if(!component.model || !component.model->AiScene){
		component.subMeshes.clear();
		return false;
	}

	ModelSubMeshStateSynchronization::Synchronize(
		component.subMeshes,
		component.model->SubMeshes
	);

	for(const auto& [animationName, animationPath] : component.animations){
		component.model->LoadAnimationSource(
			animationPath.c_str(),
			animationName.c_str()
		);
	}
	return true;
}

inline void ResetModel(ModelRendererComponent& component){
	ReleaseBuffers(component);
	ResetAnimationRuntime(component);
	component.model.reset();
	component.subMeshes.clear();
	component.blendedAnimations.clear();
}

} // namespace ModelRendererRuntime

inline ModelRendererComponent::~ModelRendererComponent(){
	ModelRendererRuntime::ResetAnimationRuntime(*this);
	model.reset();
}

inline void ModelRendererComponent::ReleaseBuffers(){
	ModelRendererRuntime::ReleaseBuffers(*this);
}

inline void ModelRendererComponent::ResetAnimationRuntime(){
	ModelRendererRuntime::ResetAnimationRuntime(*this);
}

inline void ModelRendererComponent::CreateModel(SceneContext* context){
	ModelRendererRuntime::CreateModel(*this, context);
}
