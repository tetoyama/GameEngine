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
	m_pContext->pDebug->LOG_DEBUG("TransformSystemを初期化中...");
}

void TransformSystem::Draw(){

	// 親が削除されている場合子も削除する処理
	// lateUpdate実装する必要がありそう？

	// コンポーネントを持つエンティティの検索

	for (auto& [name, scene] : m_pContext->pSceneManager->GetActiveScenes()) {
		auto m_Context= scene->GetSceneContext();

		const auto m_Entities= pContext->pComponent->FindEntitiesWithComponent<TransformComponent>();
		for (Entity entity : entities) {
			auto* transform = context->pComponent->GetComponent<TransformComponent>(entity);
			if (!transform) continue;
			if (transform->parent != 0 && !pContext->pEntity->IsAlive(transform->parent)) {
				context->pComponent->OnEntityDestroyed(entity);
				context->pEntity->Destroy(entity);
			}
		}
	}
}
