#include <cassert>
#include <typeindex>
#include <vector>

#include "Engine/Scene/System/Render/Culling/CullingSystem.h"

int main(){
	CullingSystem system(nullptr);
	std::vector<SystemTask> tasks;
	SystemScheduleBuilder builder(&system, 9, tasks);
	system.RegisterTasks(builder);

	assert(tasks.size() == 1);
	const SystemTask& task = tasks.front();
	assert(task.name == "CullingSystem.Bounds.Update");
	assert(task.domain == SystemTaskDomain::Render);
	assert(task.order.phase == SystemPhase::Early);
	assert(task.order.priority == -100);
	assert(task.threadAffinity == ThreadAffinity::AnyWorker);
	assert(task.access.componentReads.contains(typeid(TransformComponent)));
	assert(task.access.componentReads.contains(typeid(ModelRendererComponent)));
	assert(task.access.componentReads.contains(typeid(TerrainComponent)));
	assert(task.access.componentReads.contains(typeid(WaveComponent)));
	assert(task.access.componentWrites.contains(typeid(CullingComponent)));
	assert(task.access.resourceReads.contains(typeid(SceneManager)));
	return 0;
}
