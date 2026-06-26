#include <cassert>
#include <vector>
#include "Engine/Scene/System/Render/Culling/CullingSystem.h"

int main(){
	CullingSystem system(nullptr);
	std::vector<SystemTask> tasks;
	SystemScheduleBuilder builder(&system, 9, tasks);
	system.RegisterTasks(builder);
	assert(tasks.size() == 1);
	assert(tasks.front().name == "CullingSystem.Bounds.Update");
	return 0;
}
