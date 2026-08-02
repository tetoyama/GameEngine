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

	// The packet owns the CPU geometry snapshot for the frame. Stable SubMesh IDs
	// remain independent from the runtime Geometry array order.
	std::shared_ptr<ModelData> model(
		new ModelData(),
		[](ModelData*){}
	);
	model->MeshGeometry.resize(2);

	ImportedMaterialDefinition material;
	material.id = 81;
	material.name = "Default";
	material.descriptor.shaderID = 6;
	material.descriptor.parameters.baseColor = {0.2f, 0.4f, 0.6f, 1.0f};
	model->ImportedMaterials.push_back(material);

	ModelSubMeshDefinition sectionA;
	sectionA.id = 700;
	sectionA.name = "SectionA";
	sectionA.geometryIndex = 1;
	sectionA.defaultMaterialID = material.id;
	model->SubMeshes.push_back(sectionA);

	ModelSubMeshDefinition sectionB;
	sectionB.id = 400;
	sectionB.name = "SectionB";
	sectionB.geometryIndex = 0;
	sectionB.defaultMaterialID = material.id;
	model->SubMeshes.push_back(sectionB);

	RenderPacket source;
	source.sceneContextID = context.contextID;
	source.entity = entity;
	source.kind = RenderPacketKind::Model;
	source.layer = RenderLayer::Opaque3D;
	source.passMask = RenderPacketPassMask::Shadow |
		RenderPacketPassMask::GBuffer;
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

	// Sort order follows runtime Geometry index, while the persistent IDs retain
	// their Model Asset identities.
	assert(frame.Packets()[0].TargetsSubMesh(0));
	assert(frame.Packets()[0].TargetsSubMeshID(400));
	assert(frame.Packets()[1].TargetsSubMesh(1));
	assert(frame.Packets()[1].TargetsSubMeshID(700));
	assert(frame.Packets()[0].modelResource == model);
	assert(frame.Packets()[1].modelResource == model);

	for(const RenderPacket& packet : frame.Packets()){
		assert(packet.bindings.modelRenderer == nullptr);
		assert(packet.modelMaterial.IsResolved());
		assert(packet.modelMaterial.source ==
			ModelMaterialResolutionSource::ImportedMaterial);
		assert(packet.modelMaterial.importedMaterialID == material.id);
		assert(packet.modelMaterial.GetDescriptor()->shaderID == 6);
		assert(packet.materialKey == 6);
		assert(HasRenderPacketPass(
			packet.passMask,
			RenderPacketPassMask::Shadow
		));
	}

	// Resource keys remain incomplete without a renderer snapshot, but packet
	// expansion and material resolution do not depend on legacy component pointers.
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
	assert(frame.Packets()[0].subMeshID == InvalidModelSubMeshID);
	return 0;
}