#include <array>
#include <cassert>
#include <memory>

#include "Engine/Resources/Data/modelData.h"
#include "Engine/Scene/Component/EntityStateComponents.h"
#include "Engine/Scene/Registry/componentRegistry.h"
#include "Engine/Scene/Registry/entityRegistry.h"
#include "Engine/Scene/System/Render/RenderSystem/RenderPacket/RenderPacketBuffer.h"

int main(){
	EntityRegistry entities;
	SceneContext context{};
	ComponentRegistry components(&entities, &context);
	context.entity = &entities;
	context.component = &components;
	context.contextID = 73;
	context.resolverOwner = &context;
	context.resolver = [](void* owner, uint32_t) -> SceneContext* {
		return static_cast<SceneContext*>(owner);
	};
	context.storageConfig.renderPacketReserve = 2;
	context.storageConfig.staticBatchReserve = 2;
	context.storageConfig.allowRuntimeGrowth = true;

	const Entity entity = entities.Create();
	assert(entity);
	assert(components.AddComponent<StaticEntityComponent>(entity));

	// The packet owns the CPU geometry snapshot for the frame. No renderer
	// component, Assimp scene, YAML, or editor implementation is required to
	// determine sub-mesh packet multiplicity.
	std::shared_ptr<ModelData> model(
		new ModelData(),
		[](ModelData*){}
	);
	model->MeshGeometry.resize(2);

	RenderPacket source;
	source.sceneContextID = context.contextID;
	source.entity = entity;
	source.kind = RenderPacketKind::Model;
	source.layer = RenderLayer::Opaque3D;
	source.passMask = RenderPacketPassMask::GBuffer;
	source.sortKey = MakeRenderPacketSortKey(
		source.layer,
		source.kind,
		source.materialKey,
		0
	);
	source.modelResource = model;
	source.bindings.sceneContext = &context;

	RenderPacketWorkerBuffer worker(0);
	worker.Add(source);
	const std::array<RenderPacketWorkerBuffer, 1> workers{worker};

	RenderPacketFrameBuffer frame;
	frame.BeginFrame(1);
	frame.Merge(workers);
	assert(frame.IsReady());
	assert(frame.Size() == 2);
	assert(frame.Packets()[0].TargetsSubMesh(0));
	assert(frame.Packets()[1].TargetsSubMesh(1));
	assert(frame.Packets()[0].modelResource == model);
	assert(frame.Packets()[1].modelResource == model);
	assert(frame.Packets()[0].bindings.modelRenderer == nullptr);
	assert(frame.Packets()[1].bindings.modelRenderer == nullptr);

	// Resource keys remain incomplete without a material/renderer snapshot, but
	// packet expansion itself must not depend on those legacy component pointers.
	assert(frame.StaticBatchCandidates().Size() == 2);
	assert(frame.StaticBatchCandidates().GroupCount() == 1);
	assert(frame.StaticBatchCandidates().CacheReadyGroupCount() == 0);
	assert(frame.StaticBatchCache().IsValid());
	assert(frame.StaticBatchCache().Entries().empty());
	assert(frame.StaticBatchInstances().IsValid());
	assert(frame.StaticBatchInstances().Groups().empty());

	// Missing model data keeps the legacy all-sub-mesh packet intact so ordinary
	// rendering and later resource loading can continue without packet loss.
	source.modelResource.reset();
	RenderPacketWorkerBuffer missingWorker(0);
	missingWorker.Add(source);
	const std::array<RenderPacketWorkerBuffer, 1> missingWorkers{missingWorker};
	frame.BeginFrame(2);
	frame.Merge(missingWorkers);
	assert(frame.IsReady());
	assert(frame.Size() == 1);
	assert(frame.Packets()[0].TargetsAllSubMeshes());
	return 0;
}
