#pragma once

#include <utility>

#include "Graphics/graphicsContext.h"
#include "Interface/ISystem.h"
#include "Resources/Data/modelData.h"
#include "Resources/resourceService.h"
#include "Scene/Component/modelRendererComponent.h"
#include "Scene/sceneManager.h"

namespace RenderSystemAnimationTaskRegistrar {

template<typename RenderSystemT>
void Register(
	RenderSystemT& system,
	SystemScheduleBuilder& builder
){
	SystemAccess poseAccess;
	poseAccess
		.WriteComponent<ModelRendererComponent>()
		.ReadResource<SceneManager>()
		.ReadResource<ModelData>();

	builder.AddTask(
		"RenderSystem.Animation.Pose.Calculate",
		SystemTaskDomain::Editor,
		SystemPhase::Earliest,
		0,
		std::move(poseAccess),
		ThreadAffinity::AnyWorker,
		[&system](const SystemTaskContext&){
			system.CalculateAnimationPoses();
		}
	);

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
		[&system](const SystemTaskContext& taskContext){
			system.UploadAnimationPoses(taskContext.deltaTime);
		}
	);
}

} // namespace RenderSystemAnimationTaskRegistrar
