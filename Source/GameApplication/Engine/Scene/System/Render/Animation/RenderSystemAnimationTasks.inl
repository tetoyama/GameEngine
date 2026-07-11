#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Component/modelRendererComponent.h"
#include "Scene/Registry/componentRegistry.h"
#include "Graphics/graphicsContext.h"
#include "Resources/resourceService.h"
#include "Resources/Data/modelData.h"
#include "DebugTools/debugSystem.h"
#include "System/Render/Animation/AnimationInputRevision.h"
#include "System/Render/Animation/AnimationPoseEvaluator.h"
#include "System/Render/Animation/AnimationSkinningUpload.h"
#include "System/Render/Animation/AnimationSourceSynchronization.h"
#include "System/Render/Animation/RenderSystemAnimationTaskRegistrar.h"

namespace RenderSystemAnimationTasksDetail {

inline void ClearPendingPose(ModelRendererComponent& component){
	component.evaluatedBones.clear();
	component.cpuSkinnedVertices.clear();
	component.animationPoseSourceModelRevision = 0;
	component.animationPoseSourceInputRevision = 0;
	component.animationPoseReady = false;
	component.cpuSkinningReady = false;
}

inline bool ShouldLogUploadFailure(uint32_t failureCount) noexcept {
	return failureCount == 1 || (failureCount % 300) == 0;
}

inline void RecordUploadFailure(
	SceneManagerContext* context,
	Entity entity,
	ModelRendererComponent& component
){
	if(component.animationUploadFailureCount <
		(std::numeric_limits<uint32_t>::max)()){
		++component.animationUploadFailureCount;
	}
	if(!context || !context->debug ||
		!ShouldLogUploadFailure(component.animationUploadFailureCount)){
		return;
	}
	context->debug->LOG_WARNING(
		"Animation skinning upload failed. entity=" +
		std::to_string(entity.GetPackedValue()) +
		" model=" + component.modelFilePath +
		" consecutiveFailures=" +
		std::to_string(component.animationUploadFailureCount)
	);
}

inline ModelRendererGpuRuntimeKey MakeRuntimeKey(
	const SceneContext& context,
	Entity entity
) noexcept {
	ModelRendererGpuRuntimeKey key;
	key.sceneContextID = context.contextID;
	key.entity = entity.GetPackedValue();
	return key;
}

} // namespace RenderSystemAnimationTasksDetail

inline void RenderSystem::CalculateAnimationPoses(){
	if(!m_context || !m_context->sceneManager) return;
	for(const auto& [sceneName, scene] :
		m_context->sceneManager->GetActiveScenes()){
		(void)sceneName;
		if(!scene) continue;
		SceneContext* context = scene->GetSceneContext();
		if(!context || !context->component) continue;
		const auto modelEntities =
			context->component->FindEntitiesWithComponent<ModelRendererComponent>();
		for(Entity entity : modelEntities){
			ModelRendererComponent* modelRenderer =
				context->component->GetComponent<ModelRendererComponent>(entity);
			if(!modelRenderer) continue;
			modelRenderer->animationPoseSourceModelRevision = 0;
			modelRenderer->animationPoseSourceInputRevision = 0;
			modelRenderer->animationPoseReady = false;
			modelRenderer->cpuSkinningReady = false;
			const std::shared_ptr<ModelData>& model = modelRenderer->model;
			if(!model || modelRenderer->blendedAnimations.empty()){
				RenderSystemAnimationTasksDetail::ClearPendingPose(*modelRenderer);
				continue;
			}

			const std::vector<AnimationBlend> animationSnapshot =
				modelRenderer->blendedAnimations;
			const float animationTimeSnapshot = modelRenderer->animationTime;
			const std::uint64_t inputRevision = AnimationInputRevision::Compute(
				animationSnapshot,
				animationTimeSnapshot
			);
			if(!AnimationPoseEvaluator::Evaluate(
				*model,
				animationSnapshot,
				animationTimeSnapshot,
				modelRenderer->evaluatedBones
			)){
				RenderSystemAnimationTasksDetail::ClearPendingPose(*modelRenderer);
				continue;
			}
			if(inputRevision != AnimationInputRevision::Compute(
				modelRenderer->blendedAnimations,
				modelRenderer->animationTime
			)){
				RenderSystemAnimationTasksDetail::ClearPendingPose(*modelRenderer);
				continue;
			}

			const bool useGPUSkinning =
				modelRenderer->evaluatedBones.size() <= BONE_MAX_COUNT;
			if(useGPUSkinning){
				modelRenderer->cpuSkinnedVertices.clear();
			}else{
				if(!AnimationPoseEvaluator::SkinCPU(
					*model,
					modelRenderer->evaluatedBones,
					modelRenderer->cpuSkinnedVertices
				)){
					RenderSystemAnimationTasksDetail::ClearPendingPose(*modelRenderer);
					continue;
				}
				modelRenderer->cpuSkinningReady = true;
			}
			++modelRenderer->animationPoseRevision;
			if(modelRenderer->animationPoseRevision == 0){
				++modelRenderer->animationPoseRevision;
			}
			modelRenderer->animationPoseSourceModelRevision =
				modelRenderer->modelRuntimeRevision;
			modelRenderer->animationPoseSourceInputRevision = inputRevision;
			modelRenderer->animationPoseReady = true;
		}
	}
}

inline void RenderSystem::UploadAnimationPoses(float deltaTime){
	lazyTimer += deltaTime;
	if(!m_context || !m_context->sceneManager || !m_context->graphics) return;
	GraphicsContext* graphics = m_context->graphics;
	ID3D11Device* device = graphics->GetDevice();
	ID3D11DeviceContext* deviceContext = graphics->GetDeviceContext();
	for(const auto& [sceneName, scene] :
		m_context->sceneManager->GetActiveScenes()){
		(void)sceneName;
		if(!scene) continue;
		SceneContext* context = scene->GetSceneContext();
		if(!context || !context->component || context->contextID == 0) continue;
		const auto modelEntities =
			context->component->FindEntitiesWithComponent<ModelRendererComponent>();
		for(Entity entity : modelEntities){
			ModelRendererComponent* modelRenderer =
				context->component->GetComponent<ModelRendererComponent>(entity);
			if(!modelRenderer) continue;
			if(!modelRenderer->model){
				modelRenderer->CreateModel(context);
				continue;
			}
			if(AnimationSourceSynchronization::Synchronize(*modelRenderer)){
				RenderSystemAnimationTasksDetail::ClearPendingPose(*modelRenderer);
				continue;
			}
			if(!modelRenderer->animationPoseReady) continue;
			if(modelRenderer->animationPoseSourceModelRevision == 0 ||
				modelRenderer->animationPoseSourceModelRevision !=
					modelRenderer->modelRuntimeRevision ||
				modelRenderer->animationPoseSourceInputRevision == 0 ||
				modelRenderer->animationPoseSourceInputRevision !=
					AnimationInputRevision::Compute(
						modelRenderer->blendedAnimations,
						modelRenderer->animationTime
					)){
				RenderSystemAnimationTasksDetail::ClearPendingPose(*modelRenderer);
				continue;
			}

			const ModelRendererGpuRuntimeKey runtimeKey =
				RenderSystemAnimationTasksDetail::MakeRuntimeKey(*context, entity);
			ModelRendererGpuRuntime& runtime =
				m_modelRendererGpuRuntime.Acquire(runtimeKey);
			if(!runtime.Ensure(
				device,
				*modelRenderer->model,
				modelRenderer->modelRuntimeRevision
			)){
				RenderSystemAnimationTasksDetail::RecordUploadFailure(
					m_context,
					entity,
					*modelRenderer
				);
				continue;
			}

			bool uploaded = false;
			const bool useGPUSkinning =
				modelRenderer->evaluatedBones.size() <= BONE_MAX_COUNT;
			if(useGPUSkinning){
				uploaded = AnimationSkinningUpload::DispatchGPU(
					*graphics,
					*modelRenderer->model,
					modelRenderer->evaluatedBones,
					runtime.RawBuffers()
				);
			}else if(modelRenderer->cpuSkinningReady && deviceContext){
				uploaded = AnimationSkinningUpload::UploadCPU(
					*deviceContext,
					modelRenderer->cpuSkinnedVertices,
					runtime.RawBuffers()
				);
			}
			if(uploaded){
				modelRenderer->animationUploadFailureCount = 0;
				modelRenderer->animationPoseSourceModelRevision = 0;
				modelRenderer->animationPoseSourceInputRevision = 0;
				modelRenderer->animationPoseReady = false;
				modelRenderer->cpuSkinningReady = false;
			}else{
				RenderSystemAnimationTasksDetail::RecordUploadFailure(
					m_context,
					entity,
					*modelRenderer
				);
			}
		}
	}
}

// Step 17-C: `MigrateRegisteredTasks`互換Hookは撤去済み。
// Animation二段Taskは`RenderSystem::RegisterTasks`が
// `RenderSystemAnimationTaskRegistrar::Register`で直接登録する。
