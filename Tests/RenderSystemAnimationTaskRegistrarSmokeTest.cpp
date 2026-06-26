#include <cassert>
#include <vector>

#include "Engine/Scene/System/Render/Animation/RenderSystemAnimationTaskRegistrar.h"

namespace {

class MockRenderSystem final : public ISystem {
public:
	const char* GetSystemName() const override {
		return "MockRenderSystem";
	}

	void CalculateAnimationPoses(){ ++poseCalls; }
	void UploadAnimationPoses(float deltaTime){
		++uploadCalls;
		lastDeltaTime = deltaTime;
	}

	int poseCalls = 0;
	int uploadCalls = 0;
	float lastDeltaTime = 0.0f;
};

} // namespace

int main(){
	MockRenderSystem system;
	std::vector<SystemTask> tasks;
	SystemScheduleBuilder builder(&system, 4, tasks);

	RenderSystemAnimationTaskRegistrar::Register(system, builder);
	assert(tasks.size() == 2);

	const SystemTask& pose = tasks[0];
	assert(pose.name == "RenderSystem.Animation.Pose.Calculate");
	assert(pose.domain == SystemTaskDomain::Editor);
	assert(pose.order.phase == SystemPhase::Earliest);
	assert(pose.threadAffinity == ThreadAffinity::AnyWorker);
	assert(pose.access.componentWrites.contains(typeid(ModelRendererComponent)));
	assert(pose.access.resourceReads.contains(typeid(SceneManager)));
	assert(pose.access.resourceReads.contains(typeid(ModelData)));

	const SystemTask& upload = tasks[1];
	assert(upload.name == "RenderSystem.Animation.Upload");
	assert(upload.domain == SystemTaskDomain::Editor);
	assert(upload.order.phase == SystemPhase::Early);
	assert(upload.threadAffinity == ThreadAffinity::MainThread);
	assert(upload.access.componentWrites.contains(typeid(ModelRendererComponent)));
	assert(upload.access.resourceReads.contains(typeid(SceneManager)));
	assert(upload.access.resourceWrites.contains(typeid(ResourceService)));
	assert(upload.access.resourceWrites.contains(typeid(ModelData)));
	assert(upload.access.resourceWrites.contains(typeid(GraphicsContext)));
	return 0;
}
