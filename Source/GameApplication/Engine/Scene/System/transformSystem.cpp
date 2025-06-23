#include "transformSystem.h"
#include "Scene.h"
#include "sceneManager.h"

#include "Backends/ImGui/ImGui.h"

#include "Entity/Entity.h"
#include "Registry/componentRegistry.h"

#include "Component/transformComponent.h"

#include "Engine/DebugTools/debugSystem.h"

void TransformSystem::Initialize(){
	m_context->manager->debug->LOG_DEBUG("TransformSystemを初期化中...");
}
