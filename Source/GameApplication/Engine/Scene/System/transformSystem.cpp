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

void TransformSystem::Draw(){
	// コンポーネントを持つエンティティの検索
	const auto& Entities = m_context->component->FindEntitiesWithComponent<TransformComponent>();
	if(Entities.empty()){
		return;
	} else{
		for(Entity entity : Entities){
			auto* transform = m_context->component->GetComponent<TransformComponent>(entity);
			if(transform->parent != 0 && !m_context->entity->IsAlive(transform->parent)){
				m_context->entity->Destroy(entity);
				m_context->component->OnEntityDestroyed(entity);
			}
		}
	}
}
