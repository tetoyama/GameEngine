#include <cassert>
#include <vector>

#include "Engine/Scene/System/Render/RenderSystem/RenderPacket/StaticBatchPacketCache.h"

int main(){
	StaticBatchCandidateStorage candidates;
	candidates.Reserve(2);

	std::vector<RenderPacket> packets(2);
	packets[0].sceneContextID = 4;
	packets[0].entity = Entity{10, 1};
	packets[0].kind = RenderPacketKind::Model;
	packets[1].sceneContextID = 4;
	packets[1].entity = Entity{11, 1};
	packets[1].kind = RenderPacketKind::Model;

	StaticBatchCandidateKey key;
	key.kind = RenderPacketKind::Model;
	key.layer = RenderLayer::Opaque3D;
	key.pipelineKey = 1;
	key.geometryKey = 2;
	key.textureSetKey = 3;
	key.materialStateKey = 4;

	assert(candidates.Add({key, 4, packets[0].entity, 1, 1}));
	assert(candidates.Add({key, 4, packets[1].entity, 0, 0}));
	candidates.Sort();

	StaticBatchPacketCache cache;
	cache.Reserve(2);
	assert(cache.Synchronize(candidates, packets, 1, true));
	assert(cache.IsValid());
	assert(cache.Entries().size() == 1);
	assert(cache.Entities().size() == 2);
	assert(cache.PacketIndices().size() == 2);
	assert(cache.PacketIndices()[0] == 0);
	assert(cache.PacketIndices()[1] == 1);
	assert(cache.Entries()[0].representativePacketIndex == 0);
	assert(cache.Entries()[0].firstInstance == 0);
	assert(cache.Entries()[0].instanceCount == 2);
	const std::uint64_t firstRevision = cache.SourceRevision();
	const std::size_t firstRebuildCount = cache.Telemetry().rebuildCount;

	// Entity order, transforms, and resource keys remain unchanged. Only the
	// RenderPacket indices move. The cache must rebuild its packet mapping.
	candidates.BeginFrame();
	assert(candidates.Add({key, 4, packets[0].entity, 0, 1}));
	assert(candidates.Add({key, 4, packets[1].entity, 1, 0}));
	candidates.Sort();

	assert(cache.Synchronize(candidates, packets, 2, true));
	assert(cache.IsValid());
	assert(cache.SourceRevision() != firstRevision);
	assert(cache.Telemetry().rebuildCount == firstRebuildCount + 1);
	assert(cache.Generation() == 2);
	assert(cache.PacketIndices().size() == 2);
	assert(cache.PacketIndices()[0] == 1);
	assert(cache.PacketIndices()[1] == 0);
	assert(cache.Entries()[0].representativePacketIndex == 1);

	cache.Reset();
	assert(cache.PacketIndices().empty());
	return 0;
}
