#include <cassert>
#include <vector>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchUploadSystem.h"

int main(){
	SceneManagerContext context{};
	StaticBatchUploadSystem system(&context);
	std::vector<SystemTask> tasks;
	SystemScheduleBuilder builder(&system, 7, tasks);

	system.RegisterTasks(builder);

	assert(tasks.size() == 1);
	const SystemTask& task = tasks.front();
	assert(task.owner == &system);
	assert(task.name == "StaticBatchUploadSystem.Instance.Upload");
	assert(task.domain == SystemTaskDomain::Render);
	assert(task.order.phase == SystemPhase::Default);
	assert(task.order.priority == 0);
	assert(task.threadAffinity == ThreadAffinity::MainThread);
	assert(static_cast<bool>(task.execute));
	return 0;
}
