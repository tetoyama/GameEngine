#include <array>
#include <cassert>

#include "Engine/Scene/System/Render/RenderSystem/RenderPacket/StaticBatchInstanceDataBuffer.h"

namespace {

RenderPacket MakePacket(Entity entity, float translationX){
	RenderPacket packet;
	packet.entity = entity;
	packet.transform.worldMatrix.values[0] = 1.0f;
	packet.transform.worldMatrix.values[5] = 1.0f;
	packet.transform.worldMatrix.values[10] = 1.0f;
	packet.transform.worldMatrix.values[15] = 1.0f;
	packet.transform.worldMatrix.values[12] = translationX;
	return packet;
}

StaticBatchCandidateStorage MakeCandidates(Entity first, Entity second){
	StaticBatchCandidateStorage storage;
	storage.Reserve(2);
	const StaticBatchCandidateKey key{
		RenderPacketKind::Mesh,
		RenderLayer::Opaque3D,
		4,
		11,
		22,
		33,
		44
	};
	assert(key.IsCacheReady());
	assert(storage.Add({key, 9, first, 0, 1}));
	assert(storage.Add({key, 9, second, 1, 2}));
	storage.Sort();
	return storage;
}

} // namespace

int main(){
	const Entity first(7, 2);
	const Entity second(8, 3);
	StaticBatchCandidateStorage candidates = MakeCandidates(first, second);
	std::array<RenderPacket, 2> packets{
		MakePacket(first, 1.0f),
		MakePacket(second, 2.0f)
	};

	StaticBatchPacketCache cache;
	cache.Reserve(2);
	assert(cache.Synchronize(candidates, packets, 1, false));
	assert(cache.IsValid());

	StaticBatchInstanceDataBuffer instances;
	instances.Reserve(2);
	assert(instances.Synchronize(cache, false));
	assert(instances.IsValid());
	assert(!instances.IsOverflowed());
	assert(instances.Groups().size() == 1);
	assert(instances.Groups()[0].firstInstance == 0);
	assert(instances.Groups()[0].instanceCount == 2);
	assert(instances.Instances().size() == 2);
	assert(instances.Instances()[0].entityIndex == first.GetIndex());
	assert(instances.Instances()[0].entityGeneration == first.GetGeneration());
	assert(instances.Instances()[0].sceneContextID == 9);
	assert(instances.Instances()[0].worldMatrix[12] == 1.0f);
	assert(instances.Instances()[1].entityIndex == second.GetIndex());
	assert(instances.Instances()[1].entityGeneration == second.GetGeneration());
	assert(instances.Instances()[1].worldMatrix[12] == 2.0f);
	assert(instances.Telemetry().rebuildCount == 1);
	assert(instances.Telemetry().growthEventCount == 0);

	assert(cache.Synchronize(candidates, packets, 2, false));
	assert(instances.Synchronize(cache, false));
	assert(instances.Generation() == 2);
	assert(instances.Telemetry().rebuildCount == 1);

	packets[1].transform.worldMatrix.values[12] = 5.0f;
	assert(cache.Synchronize(candidates, packets, 3, false));
	assert(instances.Synchronize(cache, false));
	assert(instances.Instances()[1].worldMatrix[12] == 5.0f);
	assert(instances.Telemetry().rebuildCount == 2);

	StaticBatchInstanceDataBuffer strict;
	assert(!strict.Synchronize(cache, false));
	assert(strict.IsOverflowed());
	assert(strict.Telemetry().overflowEventCount == 1);
	assert(!strict.Synchronize(cache, false));
	assert(strict.Telemetry().overflowEventCount == 1);

	strict.Reserve(2);
	assert(strict.Synchronize(cache, true));
	assert(strict.IsValid());
	assert(!strict.IsOverflowed());
	assert(strict.Instances().size() == 2);
	return 0;
}
