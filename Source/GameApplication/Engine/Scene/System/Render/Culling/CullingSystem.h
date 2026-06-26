// =======================================================================
//
// CullingSystem.h
//
// =======================================================================
#pragma once

#include "Interface/ISystem.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Component/CullingComponent.h"
#include "Scene/Component/transformComponent.h"
#include "CullingBoundsUpdater.h"

struct SceneManagerContext;

// Cullingの派生Bounds更新をRender Packet生成から分離する。
// Resource Local Bounds供給とView Visibility構築は後続Taskとして接続する。
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
			CullingBoundsUpdater::UpdateScene(*context->component);
		}
	}

	SceneManagerContext* m_context = nullptr;
};
