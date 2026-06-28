#include <array>
#include <cassert>

#include "Engine/Scene/Component/EntityStateComponents.h"
#include "Engine/Scene/Component/meshRendererComponent.h"
#include "Engine/Scene/Registry/componentRegistry.h"
#include "Engine/Scene/Registry/entityRegistry.h"
#include "Engine/Scene/System/Render/RenderSystem/RenderPacket/RenderPacketBuffer.h"

namespace {

RenderPacket MakeStaticMeshPacket(
	Entity entity,
	std::uint64_t sequence,
	float translationX,
	SceneContext* context,
	MeshRendererComponent* mesh
){
	RenderPacket packet;
	packet.sceneContextID = context->contextID;
	packet.entity = entity;
	packet.kind = RenderPacketKind::Mesh;
	packet.layer = RenderLayer::Transparent3D;
	packet.passMask = RenderPacketPassMask::Forward;
	packet.materialKey = 4;
	packet.stableSequence = sequence;
	packet.bindings.sceneContext = context;
	packet.bindings.meshRenderer = mesh;
	packet.transform.worldMatrix.values[12] = translationX;
	return packet;
}

} // namespace

int main(){
	EntityRegistry entities;
	SceneContext context{};
	ComponentRegistry components(&entities, &context);
	context.entity = &entities;
	context.component = &components;
	context.contextID = 41;
	context.storageConfig.renderPacketReserve = 2;
	context.storageConfig.staticBatchReserve = 2;
	context.storageConfig.allowRuntimeGrowth = true;

	const Entity first = entities.Create();
	const Entity second = entities.Create();
	assert(components.AddComponent<StaticEntityComponent>(first));
	assert(components.AddComponent<StaticEntityComponent>(second));

	MeshRendererComponent firstMesh;
	MeshRendererComponent secondMesh;
	firstMesh.mesh.SetGeometryResourceKey(0x1234ull);
	secondMesh.mesh.SetGeometryResourceKey(0x1234ull);

	RenderPacketWorkerBuffer worker(0);
	worker.Add(MakeStaticMeshPacket(first, 0, 1.0f, &context, &firstMesh));
	worker.Add(MakeStaticMeshPacket(second, 1, 2.0f, &context, &secondMesh));
	std::array<RenderPacketWorkerBuffer, 1> workers{worker};

	RenderPacketFrameBuffer frame;
	frame.BeginFrame(1);
	frame.Merge(workers);

	assert(frame.IsReady());
	assert(frame.Size() == 2);
	assert(!frame.StaticBatchCandidates().IsOverflowed());
	assert(frame.StaticBatchCandidates().Size() == 2);
	assert(frame.StaticBatchCache().IsValid());
	assert(frame.StaticBatchInstances().IsValid());
	assert(!frame.StaticBatchInstances().IsOverflowed());
	assert(frame.StaticBatchInstances().Groups().size() == 1);
	assert(frame.StaticBatchInstances().Instances().size() == 2);
	assert(frame.StaticBatchInstances().Instances()[0].worldMatrix[12] == 1.0f);
	assert(frame.StaticBatchInstances().Instances()[1].worldMatrix[12] == 2.0f);
	assert(frame.StaticBatchInstanceTelemetry().growthEventCount == 0);

	// Static batching is optional. With no reserve and runtime growth disabled,
	// its CPU stages may overflow, but ordinary RenderPackets must remain intact.
	context.storageConfig.staticBatchReserve = 0;
	context.storageConfig.allowRuntimeGrowth = false;
	RenderPacketFrameBuffer strictFrame;
	strictFrame.BeginFrame(2);
	strictFrame.Merge(workers);

	assert(strictFrame.IsReady());
	assert(strictFrame.Size() == 2);
	assert(strictFrame.StaticBatchCandidates().IsOverflowed());
	assert(!strictFrame.StaticBatchCache().IsValid());
	assert(!strictFrame.StaticBatchInstances().IsValid());
	return 0;
}
