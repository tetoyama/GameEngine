// =======================================================================
// 
// transformSystem.cpp
// 
// =======================================================================
#include "transformSystem.h"
#include "Scene.h"
#include "sceneManager.h"

#include "Backends/ImGui/ImGui.h"

#include "Entity/Entity.h"
#include "Registry/componentRegistry.h"

#include "Component/transformComponent.h"

#include "DebugTools/debugSystem.h"

void TransformSystem::Initialize(){
	m_context->debug->LOG_DEBUG("TransformSystemを初期化中...");
}

void TransformSystem::Draw(){

	// 親が削除されている場合子も削除する処理
	// lateUpdate実装する必要がありそう？

	// コンポーネントを持つエンティティの検索

	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto m_Context= scene->GetSceneContext();

		const auto m_Entities= context->component->FindEntitiesWithComponent<TransformComponent>();
		for (Entity entity : entities) {
			auto* transform = context->component->GetComponent<TransformComponent>(entity);
			if (!transform) continue;
			if (transform->parent != 0 && !context->entity->IsAlive(transform->parent)) {
				context->component->OnEntityDestroyed(entity);
				context->entity->Destroy(entity);
			}
		}
	}
}
