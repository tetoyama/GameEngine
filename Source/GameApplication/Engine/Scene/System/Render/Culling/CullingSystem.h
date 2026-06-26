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
#include "Scene/Component/transformComponent.h"
#include "CullingBoundsUpdater.h"
#include "ModelCullingBoundsProvider.h"

struct SceneManagerContext;

// CullingのLocal Bounds供給と派生World Bounds更新をRender Packet生成から分離する。
// View Visibility構築は後続Taskとして接続する。
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
			.WriteComponent<CullingComponent>()
			.ReadResource<SceneManager>();

		builder.AddTask(
			"CullingSystem.Bounds.Update",
			SystemTaskDomain::Render,
			SystemPhase::Early,
			-100,
			std::move(access),
			ThreadAffinity::AnyWorker,
			[this](const SystemTaskContext&){
				UpdateBounds();
			}
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
			CullingBoundsUpdater::UpdateScene(*context->component);
		}
	}

	SceneManagerContext* m_context = nullptr;
};
