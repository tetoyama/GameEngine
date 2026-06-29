#include <cassert>
#include <cstddef>
#include <vector>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchVisibleInstanceBuffer.h"

namespace {

StaticBatchInstanceGroup MakeGroup(
	std::size_t representativePacketIndex,
	std::size_t firstInstance,
	std::size_t instanceCount
){
	StaticBatchInstanceGroup group;
	group.sceneContextID = 7;
	group.representativePacketIndex = representativePacketIndex;
	group.firstInstance = firstInstance;
	group.instanceCount = instanceCount;
	return group;
}

} // namespace

int main(){
	std::vector<StaticBatchInstanceGroup> groups{
		MakeGroup(100, 0, 3),
		MakeGroup(103, 3, 2)
	};
	std::vector<StaticBatchInstanceData> instances(5);
	std::vector<std::size_t> packetIndices{100, 101, 102, 103, 104};
	for(std::size_t index = 0; index < instances.size(); ++index){
		instances[index].entityIndex = static_cast<std::uint32_t>(10 + index);
		instances[index].entityGeneration = 1;
		instances[index].sceneContextID = 7;
	}

	StaticBatchVisibleInstanceBuffer compacted;
	compacted.Reserve(groups.size(), instances.size());
	std::size_t predicateCallCount = 0;
	assert(compacted.Build(
		groups,
		instances,
		packetIndices,
		11,
		21,
		false,
		[&](const StaticBatchInstanceData&, std::size_t sourceIndex){
			++predicateCallCount;
			return sourceIndex == 0 || sourceIndex == 2;
		}
	));
	assert(compacted.IsValid());
	assert(!compacted.IsOverflowed());
	assert(predicateCallCount == 5);
	assert(compacted.Groups().size() == 1);
	assert(compacted.SourceGroupIndices().size() == 1);
	assert(compacted.SourceGroupIndices()[0] == 0);
	assert(compacted.Groups()[0].firstInstance == 0);
	assert(compacted.Groups()[0].instanceCount == 2);
	assert(compacted.Groups()[0].representativePacketIndex == 100);
	assert(compacted.Instances().size() == 2);
	assert(compacted.Instances()[0].entityIndex == 10);
	assert(compacted.Instances()[1].entityIndex == 12);
	assert(compacted.PacketIndices().size() == 2);
	assert(compacted.PacketIndices()[0] == 100);
	assert(compacted.PacketIndices()[1] == 102);
	assert(compacted.SourceRevision() == 11);
	assert(compacted.ViewRevision() == 21);
	assert(compacted.SelectionRevision() != 0);

	const StaticBatchVisibleInstanceBufferTelemetry firstTelemetry =
		compacted.Telemetry();
	assert(firstTelemetry.sourceGroupCount == 2);
	assert(firstTelemetry.outputGroupCount == 1);
	assert(firstTelemetry.sourceInstanceCount == 5);
	assert(firstTelemetry.visibleInstanceCount == 2);
	assert(firstTelemetry.culledInstanceCount == 3);
	assert(firstTelemetry.allVisibleGroupCount == 0);
	assert(firstTelemetry.allCulledGroupCount == 1);
	assert(firstTelemetry.mixedGroupCount == 1);

	// Reusing the same source/view revision must preserve both the compacted
	// selection and its telemetry without invoking the predicate again.
	assert(compacted.Build(
		groups,
		instances,
		packetIndices,
		11,
		21,
		false,
		[&](const StaticBatchInstanceData&, std::size_t){
			++predicateCallCount;
			return false;
		}
	));
	assert(predicateCallCount == 5);
	const StaticBatchVisibleInstanceBufferTelemetry cachedTelemetry =
		compacted.Telemetry();
	assert(cachedTelemetry.visibleInstanceCount == 2);
	assert(cachedTelemetry.culledInstanceCount == 3);
	assert(cachedTelemetry.mixedGroupCount == 1);
	assert(compacted.SourceGroupIndices()[0] == 0);
	assert(compacted.PacketIndices()[0] == 100);
	assert(compacted.PacketIndices()[1] == 102);

	const std::uint64_t mixedSelectionRevision =
		compacted.SelectionRevision();
	assert(compacted.Build(
		groups,
		instances,
		packetIndices,
		11,
		22,
		false,
		[](const StaticBatchInstanceData&, std::size_t){
			return true;
		}
	));
	assert(compacted.SelectionRevision() != mixedSelectionRevision);
	assert(compacted.Groups().size() == 2);
	assert(compacted.SourceGroupIndices().size() == 2);
	assert(compacted.SourceGroupIndices()[0] == 0);
	assert(compacted.SourceGroupIndices()[1] == 1);
	assert(compacted.Instances().size() == 5);
	assert(compacted.Telemetry().allVisibleGroupCount == 2);
	assert(compacted.Telemetry().mixedGroupCount == 0);
	assert(compacted.Telemetry().allCulledGroupCount == 0);

	std::vector<StaticBatchInstanceGroup> invalidGroups{
		MakeGroup(100, 1, 3),
		MakeGroup(103, 4, 1)
	};
	assert(!compacted.Build(
		invalidGroups,
		instances,
		packetIndices,
		12,
		23,
		true,
		[](const StaticBatchInstanceData&, std::size_t){ return true; }
	));
	assert(!compacted.IsValid());
	assert(!compacted.IsOverflowed());
	assert(compacted.Groups().empty());
	assert(compacted.SourceGroupIndices().empty());
	assert(compacted.Instances().empty());
	assert(compacted.PacketIndices().empty());

	StaticBatchVisibleInstanceBuffer strict;
	assert(!strict.Build(
		groups,
		instances,
		packetIndices,
		13,
		24,
		false,
		[](const StaticBatchInstanceData&, std::size_t){ return true; }
	));
	assert(!strict.IsValid());
	assert(strict.IsOverflowed());
	assert(strict.Telemetry().overflowEventCount == 1);
	return 0;
}
