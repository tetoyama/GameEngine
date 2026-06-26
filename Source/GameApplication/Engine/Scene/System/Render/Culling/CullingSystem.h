// =======================================================================
//
// CullingSystem.h
//
// =======================================================================
#pragma once

#include <utility>

#include "Interface/ISystem.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Component/CullingComponent.h"
#include "Scene/Component/modelRendererComponent.h"
#include "Scene/Component/terrainComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Component/waveComponent.h"
#include "CullingBoundsUpdater.h"
#include "ModelCullingBoundsProvider.h"
#include "TerrainCullingBoundsProvider.h"
#include "WaveCullingBoundsProvider.h"

struct SceneManagerContext;

class CullingSystem final : public ISystem {
public:
	explicit CullingSystem(SceneManagerContext* context)
		: m_context(context) {
	}

	const char* GetSystemName() const override {
		return "CullingSystem";
	}

	void RegisterTasks(SystemScheduleBuilder& builder) override {
		SystemAccess access;
		access
			.ReadComponent<TransformComponent>()
			.ReadComponent<ModelRendererComponent>()
			.ReadComponent<TerrainComponent>()
			.ReadComponent<WaveComponent>()
			.WriteComponent<CullingComponent>()
			.ReadResource<SceneManager>();

		builder.AddTask(
			"CullingSystem.Bounds.Update",
			SystemTaskDomain::Render,
			SystemPhase::Early,
			-100,
			std::move(access),
			ThreadAffinity::AnyWorker,
			[this](const SystemTaskContext&){ UpdateBounds(); }
		);
	}

private:
	void UpdateBounds(){
		if(!m_context || !m_context->sceneManager) return;

		for(auto& [sceneName, scene] :
			m_context->sceneManager->GetActiveScenes()){
			(void)sceneName;
			if(!scene) continue;

			SceneContext* context = scene->GetSceneContext();
			if(!context || !context->component) continue;

			ModelCullingBoundsProvider::UpdateScene(*context->component);
			TerrainCullingBoundsProvider::UpdateScene(*context->component);
			WaveCullingBoundsProvider::UpdateScene(*context->component);
			CullingBoundsUpdater::UpdateScene(*context->component);
		}
	}

	SceneManagerContext* m_context = nullptr;
};
