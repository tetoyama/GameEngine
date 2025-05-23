// Engine/Scene/System/renderSystem.cpp
#include "renderSystem.h"
#include "Entity/entityRegistry.h"
#include "Component/transformComponent.h"
#include "Component/meshRendererComponent.h"
#include "Engine/Graphics/mainRenderer.h"

void RenderSystem::Render(){
	const auto& meshComponents = m_registry->GetComponents();
	for(const auto& typePair : meshComponents){
		// MeshRendererComponent型のみ処理
		if(typePair.first == typeid(MeshRendererComponent)){
			for(const auto& entityPair : typePair.second){
				EntityID entity = entityPair.first;
				auto* meshRenderer = static_cast<MeshRendererComponent*>(entityPair.second.get());
				if(!meshRenderer || !meshRenderer->mesh) continue;

				// TransformComponent取得（なければスキップ）
				auto* transform = m_registry->GetComponent<TransformComponent>(entity);
				if(!transform) continue;

				// MainRendererに描画を依頼
				//m_renderer->DrawMesh(meshRenderer, transform);
			}
		}
	}
}
