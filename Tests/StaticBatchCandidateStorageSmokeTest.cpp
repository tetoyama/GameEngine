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
	std::uint64_t sequence,
	float translationX = 0.0f
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
	packet.transform.worldMatrix.values[0] = 1.0f;
	packet.transform.worldMatrix.values[5] = 1.0f;
	packet.transform.worldMatrix.values[10] = 1.0f;
	packet.transform.worldMatrix.values[15] = 1.0f;
	packet.transform.worldMatrix.values[12] = translationX;
	packet.bindings.sceneContext = context;
	packet.bindings.modelRenderer =
		context->component->GetComponent<ModelRendererComponent>(entity);
	packet.bindings.meshRenderer =
		context->component->GetComponent<MeshRendererComponent>(entity);
	return packet;
}

RenderPacketWorkerBuffer BuildWorker(
	SceneContext* context,
	Entity staticModelA,
	Entity staticModelB,
	Entity staticMesh,
	Entity dynamicModel,
	float meshTranslationX = 0.0f
){
	RenderPacketWorkerBuffer worker(0);
	worker.Add(MakePacket(
		context,
		staticMesh,
		RenderPacketKind::Mesh,
		RenderLayer::Opaque3D,
		4,
		3,
		meshTranslationX
	));
	worker.Add(MakePacket(
		context,
		staticModelB,
		RenderPacketKind::Model,
		RenderLayer::Opaque3D,
		2,
		2
	));
	worker.Add(MakePacket(
		context,
		staticModelA,
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

	const Entity staticModelA = entities.Create();
	const Entity staticModelB = entities.Create();
	const Entity staticMesh = entities.Create();
	const Entity dynamicModel = entities.Create();
	assert(staticModelA && staticModelB && staticMesh && dynamicModel);
	assert(components.AddComponent<StaticEntityComponent>(staticModelA));
	assert(components.AddComponent<StaticEntityComponent>(staticModelB));
	assert(components.AddComponent<StaticEntityComponent>(staticMesh));

	auto* modelA = components.AddComponent<ModelRendererComponent>(staticModelA);
	auto* modelB = components.AddComponent<ModelRendererComponent>(staticModelB);
	auto* mesh = components.AddComponent<MeshRendererComponent>(staticMesh);
	assert(modelA && modelB && mesh);
	modelA->modelFilePath = "Asset/Test/SharedModel.fbx";
	modelB->modelFilePath = modelA->modelFilePath;
	mesh->mesh.SetGeometryResourceKey(0x1234u);

	const RenderPacketWorkerBuffer worker = BuildWorker(
		&context,
		staticModelA,
		staticModelB,
		staticMesh,
		dynamicModel
	);
	const std::array<RenderPacketWorkerBuffer, 1> workers{worker};

	RenderPacketFrameBuffer packets;
	packets.BeginFrame(1);
	packets.Merge(workers);

	const StaticBatchCandidateStorage& candidates = packets.StaticBatchCandidates();
	assert(candidates.Size() == 3);
	assert(candidates.GroupCount() == 2);
	assert(candidates.CacheReadyGroupCount() == 1);
	assert(candidates.Capacity() >= context.storageConfig.staticBatchReserve);
	assert(candidates.GroupCapacity() >= context.storageConfig.staticBatchReserve);
	assert(candidates.GrowthEventCount() == 0);
	assert(!candidates.IsOverflowed());

	const auto result = candidates.Candidates();
	assert(result[0].key.kind == RenderPacketKind::Model);
	assert(result[0].entity == staticModelA);
	assert(!result[0].key.IsCacheReady());
	assert(result[1].key.kind == RenderPacketKind::Model);
	assert(result[1].entity == staticModelB);
	assert(result[1].key.geometryKey == result[0].key.geometryKey);
	assert(result[2].key.kind == RenderPacketKind::Mesh);
	assert(result[2].entity == staticMesh);
	assert(result[2].key.IsCacheReady());
	assert(result[2].key.geometryKey == mesh->mesh.geometryResourceKey);

	const auto groups = candidates.Groups();
	assert(groups[0].key.kind == RenderPacketKind::Model);
	assert(groups[0].candidateCount == 2);
	assert(!groups[0].cacheReady);
	assert(groups[1].key.kind == RenderPacketKind::Mesh);
	assert(groups[1].candidateCount == 1);
	assert(groups[1].cacheReady);

	const StaticBatchPacketCache& cache = packets.StaticBatchCache();
	assert(cache.IsValid());
	assert(!cache.IsOverflowed());
	assert(cache.Entries().size() == 1);
	assert(cache.Entities().size() == 1);
	assert(cache.Entities()[0] == staticMesh);
	assert(cache.Transforms().size() == 1);
	assert(cache.Transforms()[0].worldMatrix.values[12] == 0.0f);
	const StaticBatchPacketCacheTelemetry initialCache = cache.Telemetry();
	assert(initialCache.currentEntryCount == 1);
	assert(initialCache.currentInstanceCount == 1);
	assert(initialCache.rebuildCount == 1);
	assert(initialCache.growthEventCount == 0);
	assert(initialCache.skippedIncompleteGroupCount == 1);

	packets.BeginFrame(2);
	packets.Merge(workers);
	assert(packets.StaticBatchCache().Generation() == 2);
	assert(packets.StaticBatchCache().Telemetry().rebuildCount == 1);

	const RenderPacketWorkerBuffer movedWorker = BuildWorker(
		&context,
		staticModelA,
		staticModelB,
		staticMesh,
		dynamicModel,
		5.0f
	);
	const std::array<RenderPacketWorkerBuffer, 1> movedWorkers{movedWorker};
	packets.BeginFrame(3);
	packets.Merge(movedWorkers);
	assert(packets.StaticBatchCache().Telemetry().rebuildCount == 2);
	assert(packets.StaticBatchCache().Transforms()[0].worldMatrix.values[12] == 5.0f);

	const StaticBatchCandidateStorageTelemetry telemetry =
		packets.StaticBatchTelemetry();
	assert(telemetry.currentSize == 3);
	assert(telemetry.currentGroupCount == 2);
	assert(telemetry.currentCacheReadyGroupCount == 1);
	assert(!telemetry.overflowed);

	packets.ResetPeakMetrics();
	assert(packets.StaticBatchCandidates().PeakSize() == 3);
	assert(packets.StaticBatchCandidates().PeakGroupCount() == 2);
	assert(packets.StaticBatchCandidates().PeakCacheReadyGroupCount() == 1);
	assert(packets.StaticBatchCache().Telemetry().rebuildCount == 0);

	context.storageConfig.staticBatchReserve = 1;
	RenderPacketFrameBuffer growthPackets;
	growthPackets.BeginFrame(4);
	growthPackets.Merge(workers);
	assert(growthPackets.StaticBatchCandidates().Size() == 3);
	assert(growthPackets.StaticBatchCandidates().GrowthEventCount() > 0);
	assert(growthPackets.StaticBatchCache().IsValid());

	context.storageConfig.allowRuntimeGrowth = false;
	RenderPacketFrameBuffer strictPackets;
	strictPackets.BeginFrame(5);
	strictPackets.Merge(workers);
	assert(strictPackets.Size() == 4);
	assert(strictPackets.IsReady());
	assert(strictPackets.StaticBatchCandidates().Size() == 0);
	assert(strictPackets.StaticBatchCandidates().IsOverflowed());
	assert(!strictPackets.StaticBatchCache().IsValid());
	assert(strictPackets.StaticBatchCache().IsOverflowed());
	assert(strictPackets.StaticBatchCache().Telemetry().overflowEventCount == 1);
	return 0;
}
