#include <array>
#include <cassert>

#include "Engine/Scene/Registry/componentRegistry.h"
#include "Engine/Scene/Registry/entityRegistry.h"
#include "Engine/Scene/System/Render/RenderSystem/RenderPacket/RenderPacketBuffer.h"

namespace {

RenderPacket MakePacket(
	SceneContext* context,
	Entity entity,
	RenderPacketKind kind,
	RenderLayer layer,
	std::uint32_t materialKey,
	std::uint64_t sequence
){
	RenderPacket packet;
	packet.sceneContextID = context->contextID;
	packet.entity = entity;
	packet.kind = kind;
	packet.layer = layer;
	packet.materialKey = materialKey;
	packet.passMask = RenderPacketPassesForLayer(layer);
	packet.sortKey = MakeRenderPacketSortKey(layer, kind, materialKey, 0);
	packet.stableSequence = sequence;
	packet.bindings.sceneContext = context;
	return packet;
}

RenderPacketWorkerBuffer BuildWorker(
	SceneContext* context,
	Entity staticModel,
	Entity staticMesh,
	Entity dynamicModel
){
	RenderPacketWorkerBuffer worker(0);
	worker.Add(MakePacket(
		context,
		staticMesh,
		RenderPacketKind::Mesh,
		RenderLayer::Opaque3D,
		4,
		2
	));
	worker.Add(MakePacket(
		context,
		staticModel,
		RenderPacketKind::Model,
		RenderLayer::Opaque3D,
		2,
		1
	));
	worker.Add(MakePacket(
		context,
		dynamicModel,
		RenderPacketKind::Model,
		RenderLayer::Opaque3D,
		1,
		0
	));
	return worker;
}

} // namespace

int main(){
	EntityRegistry entities;
	SceneContext context{};
	ComponentRegistry components(&entities, &context);
	context.entity = &entities;
	context.component = &components;
	context.contextID = 31;
	context.storageConfig.renderPacketReserve = 16;
	context.storageConfig.staticBatchReserve = 8;

	const Entity staticModel = entities.Create();
	const Entity staticMesh = entities.Create();
	const Entity dynamicModel = entities.Create();
	assert(staticModel && staticMesh && dynamicModel);
	assert(components.AddComponent<StaticEntityComponent>(staticModel));
	assert(components.AddComponent<StaticEntityComponent>(staticMesh));

	const RenderPacketWorkerBuffer worker = BuildWorker(
		&context,
		staticModel,
		staticMesh,
		dynamicModel
	);
	const std::array<RenderPacketWorkerBuffer, 1> workers{worker};

	RenderPacketFrameBuffer packets;
	packets.BeginFrame(1);
	packets.Merge(workers);

	const StaticBatchCandidateStorage& candidates = packets.StaticBatchCandidates();
	assert(candidates.Size() == 2);
	assert(candidates.Capacity() >= context.storageConfig.staticBatchReserve);
	assert(candidates.GrowthEventCount() == 0);

	const auto result = candidates.Candidates();
	assert(result[0].key.kind == RenderPacketKind::Model);
	assert(result[0].entity == staticModel);
	assert(result[1].key.kind == RenderPacketKind::Mesh);
	assert(result[1].entity == staticMesh);
	assert(result[0].packetIndex < packets.Packets().size());
	assert(result[1].packetIndex < packets.Packets().size());

	const StaticBatchCandidateStorageTelemetry telemetry =
		packets.StaticBatchTelemetry();
	assert(telemetry.currentSize == 2);
	assert(telemetry.peakSize == 2);
	assert(telemetry.capacity == candidates.Capacity());
	assert(telemetry.growthEventCount == 0);

	packets.ResetPeakMetrics();
	assert(packets.StaticBatchCandidates().PeakSize() == 2);
	assert(packets.StaticBatchCandidates().GrowthEventCount() == 0);

	context.storageConfig.staticBatchReserve = 1;
	RenderPacketFrameBuffer growthPackets;
	growthPackets.BeginFrame(2);
	growthPackets.Merge(workers);
	assert(growthPackets.StaticBatchCandidates().Size() == 2);
	assert(growthPackets.StaticBatchCandidates().GrowthEventCount() > 0);
	return 0;
}
