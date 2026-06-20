// =======================================================================
//
// EntityCommandCommitSystem.h
//
// =======================================================================
#pragma once

#include <limits>

#include "Scene/Interface/ISystem.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Command/EntityCommandBuffer.h"

// 各Scheduleの最後に、Scene単位のEntityCommandBufferを適用する。
class EntityCommandCommitSystem final : public ISystem {
public:
	explicit EntityCommandCommitSystem(SceneManagerContext* context)
		: m_context(context) {
	}

	const char* GetSystemName() const override {
		return "EntityCommandCommitSystem";
	}

	void RegisterTasks(SystemScheduleBuilder& builder) override {
		RegisterCommitTask(builder, SystemTaskDomain::Fixed, "FixedCommit");
		RegisterCommitTask(builder, SystemTaskDomain::Frame, "FrameCommit");
		RegisterCommitTask(builder, SystemTaskDomain::Editor, "EditorCommit");
		RegisterCommitTask(builder, SystemTaskDomain::Render, "RenderCommit");
	}

private:
	void RegisterCommitTask(
		SystemScheduleBuilder& builder,
		SystemTaskDomain domain,
		const char* taskName
	) {
		SystemAccess access;
		access.SetWorldAccess(WorldAccessMode::Exclusive)
			.SetStructuralAccess(StructuralAccess::ExclusiveWorldWrite);

		builder.AddTask(
			std::string("EntityCommandCommitSystem.") + taskName,
			domain,
			SystemPhase::Latest,
			std::numeric_limits<int32_t>::max(),
			std::move(access),
			ThreadAffinity::MainThread,
			[this](const SystemTaskContext&) {
				CommitAllScenes();
			}
		);
	}

	void CommitAllScenes() {
		if(!m_context || !m_context->sceneManager) return;

		for(auto& [sceneName, scene] : m_context->sceneManager->GetActiveScenes()) {
			if(!scene) continue;

			SceneContext* sceneContext = scene->GetSceneContext();
			if(!sceneContext || !sceneContext->commands) continue;

			sceneContext->commands->Commit(*sceneContext);
		}
	}

	SceneManagerContext* m_context = nullptr;
};
