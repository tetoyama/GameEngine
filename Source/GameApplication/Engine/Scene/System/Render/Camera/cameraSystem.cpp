// =======================================================================
// 
// cameraSystem.cpp
// 
// =======================================================================
#include "CameraSystem.h"

#include <DirectXMath.h>
#include "Backends/myVector3.h"

#include "Backends/Imgui/Imgui.h"
#include "Backends/Imgui/Imguizmo.h"

#include "DebugTools/debugSystem.h"
#include "DebugTools/imguisystem.h"

#include "Graphics/graphicsContext.h"
#include "Graphics/mainRenderer.h"

#include "Scene.h"
#include "sceneManager.h"

#include "Registry/entityRegistry.h"
#include "Registry/componentRegistry.h"

#include "Component/CameraComponent.h"
#include "Component/transformComponent.h"

void CameraSystem::Initialize(){
	m_context->debug->LOG_DEBUG("CameraSystem initialize...");
}

void CameraSystem::RegisterTasks(SystemScheduleBuilder& builder){
	using CameraUpdateQuery = ECSQuery::ComponentQueryView<
		ECSQuery::Read<TransformComponent>,
		ECSQuery::Write<CameraComponent>
	>;

	builder.AddQueryTask<CameraUpdateQuery>(
		"CameraSystem.UpdateViewMatrix",
		SystemTaskDomain::Render,
		SystemPhase::Early,
		0,
		StructuralAccess::None,
		ThreadAffinity::AnyWorker,
		[this](const SystemTaskContext&){
			Draw();
		}
	);
}

void CameraSystem::Draw(){
	for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
		(void)name;

		SceneContext* context = scene->GetSceneContext();
		if(!context || !context->component){
			continue;
		}

		const auto cameraEntities =
			context->component->FindEntitiesWithComponent<CameraComponent>();

		for(Entity entity : cameraEntities){
			CameraComponent* camera =
				context->component->GetComponent<CameraComponent>(entity);
			TransformComponent* transform =
				context->component->GetComponent<TransformComponent>(entity);

			if(!camera || !transform){
				continue;
			}

			if(camera->isLock){
				camera->viewMatrix = DirectX::XMMatrixLookAtLH(
					transform->position.ToXMVECTOR(),
					camera->Target.ToXMVECTOR(),
					{0.0f, 1.0f, 0.0f}
				);
			} else{
				camera->viewMatrix = DirectX::XMMatrixLookAtLH(
					transform->position.ToXMVECTOR(),
					(transform->position + transform->front()).ToXMVECTOR(),
					{0.0f, 1.0f, 0.0f}
				);
			}
		}
	}
}
