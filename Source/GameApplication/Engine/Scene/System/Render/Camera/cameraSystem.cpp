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
	m_context->debug->LOG_DEBUG("CameraSystemを初期化中...");
}

void CameraSystem::Draw() {

	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		auto CameraBuffers = context->component->GetAllBaseComponents<CameraComponent>();
		for (auto& [entity, CameraBuffer] : CameraBuffers) {
			TransformComponent* transform = context->component->GetComponent<TransformComponent>(entity);
			if (transform) {
				if (CameraBuffer->isLock) {
					CameraBuffer->viewMatrix = DirectX::XMMatrixLookAtLH(
						transform->position.ToXMVECTOR(),
						CameraBuffer->Target.ToXMVECTOR(),
						{ 0.0f, 1.0f, 0.0f }
					);

				} else {

					CameraBuffer->viewMatrix = DirectX::XMMatrixLookAtLH(
						transform->position.ToXMVECTOR(),
						(transform->position + transform->front()).ToXMVECTOR(),
						{0.0f, 1.0f, 0.0f}
						);
				}
			}
		}
	}
}
