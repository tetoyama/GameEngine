#include <array>
#include <cassert>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchPacketReplacementSet.h"

int main(){
	StaticBatchPacketReplacementSet replacements;
	replacements.Begin(4);
	assert(replacements.PacketCount() == 4);
	assert(replacements.Count() == 0);

	constexpr std::array<std::size_t, 4> packetIndices{2, 0, 3, 1};
	StaticBatchPacketCacheEntry group;
	group.firstInstance = 0;
	group.instanceCount = 2;
	assert(replacements.AddGroup(group, packetIndices));
	assert(replacements.Contains(2));
	assert(replacements.Contains(0));
	assert(!replacements.Contains(1));
	assert(!replacements.Contains(3));
	assert(replacements.Count() == 2);

	assert(replacements.AddGroup(group, packetIndices));
	assert(replacements.Count() == 2);

	constexpr std::array<std::size_t, 2> invalidIndices{1, 8};
	StaticBatchPacketReplacementSet invalidReplacement;
	invalidReplacement.Begin(4);
	group.firstInstance = 0;
	group.instanceCount = 2;
	assert(!invalidReplacement.AddGroup(group, invalidIndices));
	assert(invalidReplacement.Count() == 0);
	assert(!invalidReplacement.Contains(1));

	group.firstInstance = packetIndices.size();
	group.instanceCount = 1;
	assert(!replacements.AddGroup(group, packetIndices));

	replacements.Reset();
	assert(replacements.PacketCount() == 0);
	assert(replacements.Count() == 0);
	return 0;
}
