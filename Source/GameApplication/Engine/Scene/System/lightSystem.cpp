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

	return;

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

				Vector3 front = transform->front();
				Vector3 up = transform->up();
				DirectX::XMStoreFloat4x4(
				&light->light.LightView, DirectX::XMMatrixTranspose(DirectX::XMMatrixLookAtLH(
					transform->position.ToXMVECTOR(),
					(transform->position + front * 100.0f).ToXMVECTOR(),
					(up).ToXMVECTOR()
				)));
				DirectX::XMStoreFloat4x4(

				&light->light.LightProjection , DirectX::XMMatrixTranspose(DirectX::XMMatrixOrthographicLH(
					100.0f,
					100.0f,
					0.01f,
					100.0f
				)));
			}
		}
	}
}

void LightSystem::Draw(){

	LIGHT lights[LIGHT_MAX_COUNT];
	for(int i = 0; i < LIGHT_MAX_COUNT; i++){
		lights[i].Enable = false;
	}

	int lightCount = 0;

	GraphicsContext* graphicsContext = m_context->renderer->GetGraphicsContext();

	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();

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
			if (lightCount < LIGHT_MAX_COUNT) {
				lights[lightCount] = light->light;
				lightCount++;
			}
		}
	}

	//graphicsContext->SetLight(&lights[0]);

}
