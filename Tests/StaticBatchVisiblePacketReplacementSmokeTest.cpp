#include <array>
#include <cassert>
#include <cstddef>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchPacketReplacementSet.h"
#include "Engine/Scene/System/Render/StaticBatch/StaticBatchVisibleInstanceBuffer.h"

int main(){
	StaticBatchInstanceGroup sourceGroup;
	sourceGroup.sceneContextID = 7;
	sourceGroup.representativePacketIndex = 100;
	sourceGroup.firstInstance = 0;
	sourceGroup.instanceCount = 3;

	const std::array<StaticBatchInstanceGroup, 1> groups{sourceGroup};
	std::array<StaticBatchInstanceData, 3> instances{};
	instances[0].entityIndex = 10;
	instances[1].entityIndex = 11;
	instances[2].entityIndex = 12;
	const std::array<std::size_t, 3> packetIndices{100, 101, 102};

	StaticBatchVisibleInstanceBuffer visible;
	visible.Reserve(1, 3);
	assert(visible.Build(
		groups,
		instances,
		packetIndices,
		11,
		21,
		false,
		[](const StaticBatchInstanceData&, std::size_t sourceIndex){
			return sourceIndex != 1;
		}
	));
	assert(visible.Groups().size() == 1);
	assert(visible.Groups()[0].instanceCount == 2);
	assert(visible.PacketIndices().size() == 2);
	assert(visible.PacketIndices()[0] == 100);
	assert(visible.PacketIndices()[1] == 102);

	StaticBatchPacketReplacementSet replacements;
	replacements.Begin(103);
	assert(replacements.AddGroup(
		visible.Groups()[0],
		visible.PacketIndices()
	));
	assert(replacements.Contains(100));
	assert(!replacements.Contains(101));
	assert(replacements.Contains(102));
	assert(replacements.Count() == 2);
	return 0;
}
