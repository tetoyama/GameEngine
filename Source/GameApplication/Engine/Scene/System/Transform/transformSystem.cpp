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
#include "Scene/Command/EntityCommandBuffer.h"

#include "Component/transformComponent.h"

#include "DebugTools/debugSystem.h"

void TransformSystem::Initialize(){
	m_context->debug->LOG_DEBUG("TransformSystemを初期化中...");
}

void TransformSystem::RegisterTasks(SystemScheduleBuilder& builder){
	using TransformReadQuery = ECSQuery::ComponentQueryView<
		ECSQuery::Read<TransformComponent>
	>;

	builder.AddQueryTask<TransformReadQuery>(
		"TransformSystem.Hierarchy.Cleanup",
		SystemTaskDomain::Render,
		SystemPhase::Earliest,
		0,
		StructuralAccess::EmitCommands,
		ThreadAffinity::AnyWorker,
		[this](const SystemTaskContext&){
			Draw();
		}
	);
}

void TransformSystem::Draw(){
	// 親が削除されているEntityは、描画中にRegistryを直接変更せず、
	// Render Domain末尾のCommit Taskで安全に削除する。
	for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
		(void)name;
		auto* context = scene->GetSceneContext();
		if(!context || !context->component || !context->entity){
			continue;
		}

		// Schedule外ではCommandBufferが存在しない場合がある。
		// 即時削除はStorage走査を無効化するため、互換経路だけ
		// Entity一覧のスナップショットを使う。
		if(!context->commands){
			const auto entities =
				context->component->FindEntitiesWithComponent<TransformComponent>();

			for(Entity entity : entities){
				auto* transform =
					context->component->GetComponent<TransformComponent>(entity);
				if(!transform) continue;

				if(transform->parent != 0 &&
					!context->entity->IsAlive(transform->parent)){
					context->component->OnEntityDestroyed(entity);
					context->entity->Destroy(entity);
				}
			}
			continue;
		}

		// TransformがDirectPagedの場合は、Alive Entity全体ではなく
		// 割当済みPageのOccupied Slotだけを走査する。
		const bool traversedDirectPaged =
			context->component->ForEachDirectPagedComponent<TransformComponent>(
				[context](Entity entity, TransformComponent& transform){
					if(transform.parent != 0 &&
						!context->entity->IsAlive(transform.parent)){
						context->commands->DestroyEntity(entity);
					}
				}
			);

		if(traversedDirectPaged){
			continue;
		}

		// Storage移行中の互換経路。
		const auto query =
			context->component->ReadQuery<TransformComponent>();

		for(Entity entity : query){
			auto* transform =
				context->component->GetComponent<TransformComponent>(entity);
			if(!transform) continue;

			if(transform->parent != 0 &&
				!context->entity->IsAlive(transform->parent)){
				context->commands->DestroyEntity(entity);
			}
		}
	}
}
