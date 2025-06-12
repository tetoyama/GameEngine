#include "explosionEffectSystem.h"

#include "bulletSystem.h"
#include "Scene.h"
#include "sceneManager.h"
#include "Engine/DebugTools/debugSystem.h"

#include "Engine/Resources/resourceSystem.h"
#include "Engine/Resources/Data/modelData.h"
#include "Engine/Resources/Loader/modelLoader.h"
#include "Engine/Resources/Loader/shaderLoader.h"
#include "Engine/Resources/Loader/textureLoader.h"
#include "Engine/Resources/Data/vertexShaderData.h"
#include "Engine/Resources/Data/pixelShaderData.h"

#include "Registry/entityRegistry.h"
#include "Registry/componentRegistry.h"

#include "Component/entityNameComponent.h"
#include "Component/transformComponent.h"

#include "Component/textureComponent.h"
#include "Component/BillBoardRendererComponent.h"
#include "Component/meshRendererComponent.h"
#include "Component/explosionEffectComponent.h"

#include "Engine/Graphics/mainRenderer.h"

#include "Engine/Platform/InputSystem/InputSystem.h"

void ExplosionEffectSystem::Update(float deltaTime) {
	// コンポーネントを持つエンティティの検索
	const auto& entities = m_context->component->FindEntitiesWithComponent<ExplosionEffectComponent>();
	if (entities.empty()) {
		return;
	} else {
		for (Entity entity : entities) {
			ExplosionEffectComponent* effect = m_context->component->GetComponent<ExplosionEffectComponent>(entity);
			if (effect) {
				
				// 時間経過で削除
				effect->Timer += deltaTime;
				if (effect->LifeTime <= effect->Timer) {

					m_context->entity->Destroy(entity);
					m_context->component->OnEntityDestroyed(entity);
					continue;
				}

				// テクスチャUVの設定
				TextureComponent* texture = m_context->component->GetComponent<TextureComponent>(entity);
				texture->AnimationNum = (int)((texture->UV_Slice_X * texture->UV_Slice_Y - 1) * effect->Timer / effect->LifeTime);
			}
		}
	}
}
