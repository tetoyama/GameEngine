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
		"TransformSystem.Draw",
		SystemTaskDomain::Render,
		SystemPhase::Default,
		0,
		ThreadAffinity::MainThread,
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

		// 結果vectorを生成せず、生存EntityとComponentMaskを遅延走査する。
		const auto query =
			context->component->ReadQuery<TransformComponent>();

		for(Entity entity : query){
			auto* transform =
				context->component->GetComponent<TransformComponent>(entity);
			if(!transform) continue;

			if(transform->parent != 0 &&
				!context->entity->IsAlive(transform->parent)){
				if(context->commands){
					context->commands->DestroyEntity(entity);
				} else {
					// Schedule外から呼ばれた場合の互換フォールバック。
					context->component->OnEntityDestroyed(entity);
					context->entity->Destroy(entity);
				}
			}
		}
	}
}