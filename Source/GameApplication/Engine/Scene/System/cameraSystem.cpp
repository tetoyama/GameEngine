#include "cameraSystem.h"

#include <DirectXMath.h>
#include "Backends/myVector3.h"

#include "Backends/Imgui/Imgui.h"
#include "Backends/Imgui/Imguizmo.h"

#include "Engine/DebugTools/debugSystem.h"
#include "Engine/DebugTools/imguisystem.h"


#include "Engine/Graphics/graphicsContext.h"
#include "Engine/Graphics/mainRenderer.h"

#include "Scene.h"
#include "sceneManager.h"

#include "Registry/entityRegistry.h"
#include "Registry/componentRegistry.h"

#include "Component/cameraComponent.h"
#include "Component/transformComponent.h"

void CameraSystem::Initialize(){
	m_context->debug->LOG_DEBUG("CameraSystemを初期化中...");
}

void CameraSystem::Update(float deltaTime) {

}
