#include <array>
#include <cassert>
#include <memory>

#include "Engine/Resources/Data/modelData.h"
#include "Engine/Scene/Component/EntityStateComponents.h"
#include "Engine/Scene/Component/modelRendererComponent.h"
#include "Engine/Scene/Registry/componentRegistry.h"
#include "Engine/Scene/Registry/entityRegistry.h"
#include "Engine/Scene/System/Render/RenderSystem/RenderPacket/RenderPacketBuffer.h"

// The smoke binary does not link modelData.cpp. Its test model owns no native
// resources, so a minimal release definition is sufficient for this contract.
void ModelData::Release(){
	AiScene = nullptr;
}

int main(){
	EntityRegistry entities;
	SceneContext context{};
	ComponentRegistry components(&entities, &context);
	context.entity = &entities;
	context.component = &components;
	context.contextID = 73;
	context.storageConfig.renderPacketReserve = 2;
	context.storageConfig.staticBatchReserve = 2;
	context.storageConfig.allowRuntimeGrowth = true;

	const Entity entity = entities.Create();
	assert(entity);
	assert(components.AddComponent<StaticEntityComponent>(entity));
	ModelRendererComponent* renderer =
		components.AddComponent<ModelRendererComponent>(entity);
	assert(renderer);
	renderer->modelFilePath = "Asset/Test/TwoSubMeshes.fbx";
	renderer->modelRuntimeRevision = 1;

	auto scene = std::make_unique<aiScene>();
	scene->mNumMeshes = 2;
	scene->mMeshes = new aiMesh*[2]{new aiMesh(), new aiMesh()};
	scene->mMeshes[0]->mNumVertices = 3;
	scene->mMeshes[0]->mNumFaces = 1;
	scene->mMeshes[0]->mMaterialIndex = 0;
	scene->mMeshes[1]->mNumVertices = 6;
	scene->mMeshes[1]->mNumFaces = 2;
	scene->mMeshes[1]->mMaterialIndex = 1;
	scene->mNumMaterials = 2;

	auto model = std::make_shared<ModelData>();
	model->AiScene = scene.get();
	model->VertexBuffer.resize(2, nullptr);
	model->IndexBuffer.resize(2, nullptr);
	renderer->model = model;

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
	source.bindings.sceneContext = &context;
	source.bindings.modelRenderer = renderer;

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
	assert(frame.Packets()[0].bindings.modelRenderer == renderer);
	assert(frame.Packets()[1].bindings.modelRenderer == renderer);
	assert(frame.StaticBatchCandidates().Size() == 2);
	assert(frame.StaticBatchCandidates().GroupCount() == 2);
	assert(frame.StaticBatchCandidates().CacheReadyGroupCount() == 2);
	assert(frame.StaticBatchCache().IsValid());
	assert(frame.StaticBatchCache().Entries().size() == 2);
	assert(frame.StaticBatchInstances().IsValid());
	assert(frame.StaticBatchInstances().Groups().size() == 2);
	assert(
		frame.StaticBatchCandidates().Groups()[0].key.geometryKey !=
		frame.StaticBatchCandidates().Groups()[1].key.geometryKey
	);

	// Missing model data keeps the legacy packet intact so ordinary rendering
	// and later resource loading can continue without packet loss.
	renderer->model.reset();
	frame.BeginFrame(2);
	frame.Merge(workers);
	assert(frame.IsReady());
	assert(frame.Size() == 1);
	assert(frame.Packets()[0].TargetsAllSubMeshes());

	model->AiScene = nullptr;
	model.reset();
	return 0;
}
