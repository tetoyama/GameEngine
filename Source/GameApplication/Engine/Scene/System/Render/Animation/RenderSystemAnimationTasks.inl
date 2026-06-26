#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Component/modelRendererComponent.h"
#include "Scene/Registry/componentRegistry.h"
#include "Graphics/graphicsContext.h"
#include "Resources/resourceService.h"
#include "Resources/Data/modelData.h"
#include "DebugTools/debugSystem.h"
#include "System/Render/Animation/AnimationPoseEvaluator.h"
#include "System/Render/Animation/AnimationSkinningUpload.h"

namespace RenderSystemAnimationTasksDetail {

inline void ClearPendingPose(ModelRendererComponent& component){
	component.evaluatedBones.clear();
	component.cpuSkinnedVertices.clear();
	component.animationPoseSourceModelRevision = 0;
	component.animationPoseReady = false;
	component.cpuSkinningReady = false;
}

inline bool ShouldLogUploadFailure(uint32_t failureCount) noexcept {
	// 初回は即時通知し、継続失敗は約300回ごとに抑制して記録する。
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
			context->component->FindEntitiesWithComponent<
				ModelRendererComponent
			>();

		for(Entity entity : modelEntities){
			ModelRendererComponent* modelRenderer =
				context->component->GetComponent<ModelRendererComponent>(entity);
			if(!modelRenderer) continue;

			modelRenderer->animationPoseSourceModelRevision = 0;
			modelRenderer->animationPoseReady = false;
			modelRenderer->cpuSkinningReady = false;

			const std::shared_ptr<ModelData>& model = modelRenderer->model;
			if(!model || modelRenderer->blendedAnimations.empty()){
				RenderSystemAnimationTasksDetail::ClearPendingPose(*modelRenderer);
				continue;
			}

			if(!AnimationPoseEvaluator::Evaluate(
				*model,
				modelRenderer->blendedAnimations,
				modelRenderer->animationTime,
				modelRenderer->evaluatedBones
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
					RenderSystemAnimationTasksDetail::ClearPendingPose(
						*modelRenderer
					);
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
			modelRenderer->animationPoseReady = true;
		}
	}
}

inline void RenderSystem::UploadAnimationPoses(float deltaTime){
	lazyTimer += deltaTime;

	if(!m_context || !m_context->sceneManager || !m_context->graphics) return;

	GraphicsContext* graphics = m_context->graphics;
	ID3D11DeviceContext* deviceContext = graphics->GetDeviceContext();

	for(const auto& [sceneName, scene] :
		m_context->sceneManager->GetActiveScenes()){
		(void)sceneName;
		if(!scene) continue;

		SceneContext* context = scene->GetSceneContext();
		if(!context || !context->component) continue;

		const auto modelEntities =
			context->component->FindEntitiesWithComponent<
				ModelRendererComponent
			>();

		for(Entity entity : modelEntities){
			ModelRendererComponent* modelRenderer =
				context->component->GetComponent<ModelRendererComponent>(entity);
			if(!modelRenderer) continue;

			// Resource LoadとD3D11 Buffer生成はMain Threadに限定する。
			if(!modelRenderer->model){
				modelRenderer->CreateModel(context);
				continue;
			}

			if(!modelRenderer->animationPoseReady) continue;

			// Pose計算後にModelがReloadされた場合、旧Model由来のBone/Stagingを
			// 新しいDynamic BufferへUploadしない。
			if(modelRenderer->animationPoseSourceModelRevision == 0 ||
				modelRenderer->animationPoseSourceModelRevision !=
					modelRenderer->modelRuntimeRevision){
				RenderSystemAnimationTasksDetail::ClearPendingPose(*modelRenderer);
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
					modelRenderer->dynamicVertexBuffers
				);
			}else if(modelRenderer->cpuSkinningReady && deviceContext){
				uploaded = AnimationSkinningUpload::UploadCPU(
					*deviceContext,
					modelRenderer->cpuSkinnedVertices,
					modelRenderer->dynamicVertexBuffers
				);
			}

			if(uploaded){
				modelRenderer->animationUploadFailureCount = 0;
				modelRenderer->animationPoseSourceModelRevision = 0;
				modelRenderer->animationPoseReady = false;
				modelRenderer->cpuSkinningReady = false;
			}else{
				// Ready状態を維持するため、次のMain Thread Stageで再試行する。
				RenderSystemAnimationTasksDetail::RecordUploadFailure(
					m_context,
					entity,
					*modelRenderer
				);
			}
		}
	}
}

inline void RenderSystem::MigrateRegisteredTasks(
	SystemScheduleBuilder& builder,
	std::vector<SystemTask>& tasks
){
	auto legacyIterator = std::find_if(
		tasks.begin(),
		tasks.end(),
		[this](const SystemTask& task){
			return task.owner == this &&
				task.name == "RenderSystem.Animation.Upload" &&
				task.domain == SystemTaskDomain::Editor &&
				task.access.worldAccess == WorldAccessMode::Exclusive;
		}
	);
	if(legacyIterator == tasks.end()) return;

	SystemAccess poseAccess;
	poseAccess
		.WriteComponent<ModelRendererComponent>()
		.ReadResource<SceneManager>()
		.ReadResource<ModelData>();

	legacyIterator->name = "RenderSystem.Animation.Pose.Calculate";
	legacyIterator->order.phase = SystemPhase::Earliest;
	legacyIterator->order.priority = 0;
	legacyIterator->access = std::move(poseAccess);
	legacyIterator->threadAffinity = ThreadAffinity::AnyWorker;
	legacyIterator->execute = [this](const SystemTaskContext&){
		CalculateAnimationPoses();
	};

	SystemAccess uploadAccess;
	uploadAccess
		.WriteComponent<ModelRendererComponent>()
		.ReadResource<SceneManager>()
		.WriteResource<ResourceService>()
		.WriteResource<ModelData>()
		.WriteResource<GraphicsContext>();

	builder.AddTask(
		"RenderSystem.Animation.Upload",
		SystemTaskDomain::Editor,
		SystemPhase::Early,
		0,
		std::move(uploadAccess),
		ThreadAffinity::MainThread,
		[this](const SystemTaskContext& context){
			UploadAnimationPoses(context.deltaTime);
		}
	);
}
