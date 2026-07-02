#include <cassert>
#include <vector>

#include "Engine/Scene/System/Render/Animation/RenderSystemAnimationTaskRegistrar.h"

namespace {

class DeltaProbeSystem final : public ISystem {
public:
	const char* GetSystemName() const override { return "DeltaProbeSystem"; }
	void CalculateAnimationPoses(){ ++poseCalls; }
	void UploadAnimationPoses(float value){
		++uploadCalls;
		receivedValue = value;
	}
	int poseCalls = 0;
	int uploadCalls = 0;
	float receivedValue = 0.0f;
};

} // namespace

int main(){
	DeltaProbeSystem system;
	std::vector<SystemTask> tasks;
	SystemScheduleBuilder builder(&system, 2, tasks);
	RenderSystemAnimationTaskRegistrar::Register(system, builder);
	assert(tasks.size() == 2);

	SystemTaskContext execution;
	execution.deltaTime = 0.75f;
	tasks[0].execute(execution);
	tasks[1].execute(execution);

	assert(system.poseCalls == 1);
	assert(system.uploadCalls == 1);
	assert(system.receivedValue == 0.75f);
	return 0;
}
