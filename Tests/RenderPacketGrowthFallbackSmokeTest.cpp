#include <array>
#include <cassert>

#include "Engine/Scene/Registry/componentRegistry.h"
#include "Engine/Scene/Registry/entityRegistry.h"
#include "Engine/Scene/System/Render/RenderSystem/RenderPacket/RenderPacketBuffer.h"

namespace {

RenderPacket MakePacket(
	uint32_t sceneID,
	Entity entity,
	uint64_t sequence,
	SceneContext* context
){
	RenderPacket packet;
	packet.sceneContextID = sceneID;
	packet.entity = entity;
	packet.kind = RenderPacketKind::Model;
	packet.layer = RenderLayer::Opaque3D;
	packet.passMask = RenderPacketPassMask::GBuffer;
	packet.stableSequence = sequence;
	packet.bindings.sceneContext = context;
	return packet;
}

} // namespace

int main(){
	EntityRegistry entities;
	SceneContext context{};
	ComponentRegistry components(&entities, &context);
	context.entity = &entities;
	context.component = &components;
	context.contextID = 77;
	context.storageConfig.renderPacketReserve = 1;
	context.storageConfig.staticBatchReserve = 0;
	context.storageConfig.allowRuntimeGrowth = false;

	const Entity first = entities.Create();
	const Entity second = entities.Create();
	const Entity third = entities.Create();

	RenderPacketWorkerBuffer worker(0);
	worker.Add(MakePacket(context.contextID, first, 0, &context));
	worker.Add(MakePacket(context.contextID, second, 1, &context));
	worker.Add(MakePacket(context.contextID, third, 2, &context));
	std::array<RenderPacketWorkerBuffer, 1> workers{worker};

	RenderPacketFrameBuffer frame;
	frame.Reserve(1);
	frame.BeginFrame(1);
	frame.Merge(workers);

	assert(frame.IsReady());
	assert(frame.Size() == 3);
	assert(frame.GrowthEventCount() == 1);
	assert(frame.SafetyGrowthOverrideCount() == 1);
	assert(frame.LastMergeUsedSafetyGrowth());
	assert(frame.Telemetry().lastMergeUsedSafetyGrowth);
	assert(frame.Telemetry().safetyGrowthOverrideCount == 1);

	// Once the safety capacity exists, the next frame must publish every packet
	// without counting another override.
	frame.BeginFrame(2);
	frame.Merge(workers);
	assert(frame.IsReady());
	assert(frame.Size() == 3);
	assert(frame.GrowthEventCount() == 1);
	assert(frame.SafetyGrowthOverrideCount() == 1);
	assert(!frame.LastMergeUsedSafetyGrowth());

	frame.ResetPeakMetrics();
	assert(frame.GrowthEventCount() == 0);
	assert(frame.SafetyGrowthOverrideCount() == 0);
	assert(!frame.LastMergeUsedSafetyGrowth());
	return 0;
}
