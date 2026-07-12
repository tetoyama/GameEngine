#include <array>
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "Engine/Scene/System/Render/RenderSystem/RenderWorld/RenderWorldExtraction.h"
#include "Engine/Scene/System/Render/RenderSystem/RenderWorld/RenderWorldExtractionTaskRegistrar.h"

namespace {

std::string ReadTextFile(const char* path){
	std::ifstream stream(path, std::ios::binary);
	assert(stream && "contract source file must exist");
	return std::string(
		std::istreambuf_iterator<char>(stream),
		std::istreambuf_iterator<char>()
	);
}

struct FakeRenderSystem {
	int extractionCount = 0;

	void BuildRenderPackets(){
		++extractionCount;
	}
};

void ValidatePublishContract(){
	RenderWorld world;
	std::array<RenderPacketWorkerBuffer, 1> workers{
		RenderPacketWorkerBuffer(0)
	};

	RenderWorldExtraction::Publish(world, 42, workers);
	assert(world.Generation() == 42);
	assert(world.IsReady());
	assert(world.Packets().Size() == 0);
	assert(world.Visibility().FrameSerial() == 42);
}

void ValidateTaskContract(){
	const SystemAccess access = RenderWorldExtractionTaskRegistrar::BuildAccess();
	assert(access.componentReads.contains(typeid(TransformComponent)));
	assert(access.componentReads.contains(typeid(RenderLayerComponent)));
	assert(access.componentReads.contains(typeid(OrderInLayerComponent)));
	assert(access.componentReads.contains(typeid(MaterialComponent)));
	assert(access.componentReads.contains(typeid(TextureComponent)));
	assert(access.componentReads.contains(typeid(ModelRendererComponent)));
	assert(access.componentReads.contains(typeid(MeshRendererComponent)));
	assert(access.componentReads.contains(typeid(SpriteRendererComponent)));
	assert(access.componentReads.contains(typeid(BillBoardRendererComponent)));
	assert(access.componentReads.contains(typeid(ParticleComponent)));
	assert(access.componentReads.contains(typeid(TerrainComponent)));
	assert(access.componentReads.contains(typeid(WaveComponent)));
	assert(access.componentReads.contains(typeid(EffectComponent)));
	assert(access.componentReads.contains(typeid(EnvironmentMapComponent)));
	assert(access.resourceReads.contains(typeid(SceneManager)));
	assert(access.resourceWrites.contains(typeid(RenderPacketFrameBuffer)));
	assert(access.worldAccess == WorldAccessMode::None);
	assert(access.structuralAccess == StructuralAccess::None);

	FakeRenderSystem system;
	std::vector<SystemTask> tasks;
	SystemScheduleBuilder builder(nullptr, 11, tasks);
	RenderWorldExtractionTaskRegistrar::Register(system, builder);
	assert(tasks.size() == 1);

	const SystemTask& task = tasks.front();
	assert(task.name == "RenderSystem.Packet.Build");
	assert(task.domain == SystemTaskDomain::Render);
	assert(task.order.phase == SystemPhase::Early);
	assert(task.order.priority == 0);
	assert(task.threadAffinity == ThreadAffinity::AnyWorker);
	assert(static_cast<bool>(task.execute));
	task.execute({});
	assert(system.extractionCount == 1);
}

void ValidateActiveImplementationContract(){
	const std::string active = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/RenderSystem/renderSystem.cpp"
	);
	const std::string legacy = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderSystemLegacyImplementation.inl"
	);
	const std::string header = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/RenderSystem/renderSystem.h"
	);

	assert(active.find("RenderWorldExtraction::Extract") != std::string::npos);
	assert(active.find("RenderWorldExtractionTaskRegistrar::Register") !=
		std::string::npos);
	assert(active.find("m_renderWorld.MarkSubmitted()") != std::string::npos);
	assert(active.find("FindEntitiesWithComponent<TransformComponent>()") ==
		std::string::npos);
	assert(legacy.find("FindEntitiesWithComponent<TransformComponent>()") !=
		std::string::npos);
	assert(header.find("void BuildRenderPacketsLegacy();") != std::string::npos);
	assert(header.find("void SubmitRenderPacketsLegacy();") != std::string::npos);
	assert(header.find("void RegisterTasksLegacy(SystemScheduleBuilder& builder);") !=
		std::string::npos);
}

} // namespace

int main(){
	ValidatePublishContract();
	ValidateTaskContract();
	ValidateActiveImplementationContract();
	return 0;
}
