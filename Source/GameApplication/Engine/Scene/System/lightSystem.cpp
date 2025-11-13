#include "lightSystem.h"
#include "Scene.h"
#include "sceneManager.h"

#include "Backends/ImGui/ImGui.h"

#include "Entity/Entity.h"
#include "Registry/componentRegistry.h"

#include "Component/lightComponent.h"

#include "Engine/DebugTools/debugSystem.h"
#include "Engine/Graphics/graphicsContext.h"
#include "Engine/Graphics/mainRenderer.h"
#include <Component/transformComponent.h>

void LightSystem::Initialize(){}

void LightSystem::Update(float deltaTime){

	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		// ライトコンポーネントを持つエンティティの検索
		const auto& lightEntities = context->component->FindEntitiesWithComponent<LightComponent>();
		if (lightEntities.empty()) {
			continue;
		}
		for (Entity entity : lightEntities) {
			LightComponent* light = context->component->GetComponent<LightComponent>(entity);
			if (!light) {
				continue;
			}
			TransformComponent* transform = context->component->GetComponent<TransformComponent>(entity);
			if (transform) {
				// ライトの位置をエンティティの位置に設定
				light->light.Position = DirectX::XMFLOAT4(transform->position.x, transform->position.y, transform->position.z, 0.0f);
				// ライトの方向をエンティティの向きに設定
				light->light.Direction = DirectX::XMFLOAT4(transform->front().x, transform->front().y, transform->front().z, 0.0f);
				light->light.Enable = light->light.Enable;
			}
		}
	}
}

void LightSystem::Draw(){

	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		GraphicsContext* graphicsContext = m_context->renderer->GetGraphicsContext();

		// ライトコンポーネントを持つエンティティの検索
		const auto& lightEntities = context->component->FindEntitiesWithComponent<LightComponent>();
		if (lightEntities.empty()) {
			return;
		}
		for (Entity entity : lightEntities) {
			LightComponent* light = context->component->GetComponent<LightComponent>(entity);
			if (!light) {
				continue;
			}
			// ライトの描画処理
			graphicsContext->SetLight(light->light);
		}
	}
}
