#include <cassert>
#include <utility>
#include <vector>

#include "Engine/Scene/System/Render/RenderSystem/RenderPacket/StaticBatchPacketCache.h"

int main(){
	StaticBatchCandidateStorage candidates;
	candidates.Reserve(2);

	std::vector<RenderPacket> packets(2);
	packets[0].sceneContextID = 4;
	packets[0].entity = Entity{10, 1};
	packets[0].kind = RenderPacketKind::Model;
	packets[0].layer = RenderLayer::Opaque3D;
	packets[0].materialKey = 0;
	packets[0].transform.worldMatrix.values[12] = 10.0f;
	packets[1].sceneContextID = 4;
	packets[1].entity = Entity{11, 1};
	packets[1].kind = RenderPacketKind::Model;
	packets[1].layer = RenderLayer::Opaque3D;
	packets[1].materialKey = 0;
	packets[1].transform.worldMatrix.values[12] = 11.0f;

	StaticBatchCandidateKey key;
	key.kind = RenderPacketKind::Model;
	key.layer = RenderLayer::Opaque3D;
	key.materialKey = 0;
	key.pipelineKey = 1;
	key.geometryKey = 2;
	key.textureSetKey = 3;
	key.materialStateKey = 4;

	assert(candidates.Add({key, 4, packets[0].entity, 0, 0}));
	assert(candidates.Add({key, 4, packets[1].entity, 1, 1}));
	candidates.Sort();

	StaticBatchPacketCache cache;
	cache.Reserve(2);
	assert(cache.Synchronize(candidates, packets, 1, true));
	assert(cache.IsValid());
	assert(cache.Entries().size() == 1);
	assert(cache.Entities().size() == 2);
	assert(cache.Entities()[0] == Entity(10, 1));
	assert(cache.Entities()[1] == Entity(11, 1));
	assert(cache.PacketIndices().size() == 2);
	assert(cache.PacketIndices()[0] == 0);
	assert(cache.PacketIndices()[1] == 1);
	assert(cache.Entries()[0].representativePacketIndex == 0);
	assert(cache.Entries()[0].firstInstance == 0);
	assert(cache.Entries()[0].instanceCount == 2);
	const std::uint64_t firstRevision = cache.SourceRevision();
	const std::size_t firstRebuildCount = cache.Telemetry().rebuildCount;

	// Keep the candidate entity order and transforms stable while changing only
	// the published RenderPacket positions.
	std::swap(packets[0], packets[1]);
	candidates.BeginFrame();
	assert(candidates.Add({key, 4, packets[1].entity, 1, 0}));
	assert(candidates.Add({key, 4, packets[0].entity, 0, 1}));
	candidates.Sort();

	assert(cache.Synchronize(candidates, packets, 2, true));
	assert(cache.IsValid());
	assert(cache.SourceRevision() != firstRevision);
	assert(cache.Telemetry().rebuildCount == firstRebuildCount + 1);
	assert(cache.Generation() == 2);
	assert(cache.Entities()[0] == Entity(10, 1));
	assert(cache.Entities()[1] == Entity(11, 1));
	assert(cache.Transforms()[0].worldMatrix.values[12] == 10.0f);
	assert(cache.Transforms()[1].worldMatrix.values[12] == 11.0f);
	assert(cache.PacketIndices().size() == 2);
	assert(cache.PacketIndices()[0] == 1);
	assert(cache.PacketIndices()[1] == 0);
	assert(cache.Entries()[0].representativePacketIndex == 1);

	// A candidate must never be allowed to reference a packet for another
	// entity, scene, kind, layer, or material key.
	candidates.BeginFrame();
	assert(candidates.Add({key, 4, Entity{99, 1}, 0, 0}));
	candidates.Sort();
	assert(cache.Synchronize(candidates, packets, 3, true));
	assert(cache.IsValid());
	assert(cache.Entries().empty());
	assert(cache.PacketIndices().empty());
	assert(cache.Telemetry().skippedIncompleteGroupCount == 1);

	cache.Reset();
	assert(cache.PacketIndices().empty());
	return 0;
}
